#include "mfvideo.h"

#include <QImage>

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

static const LONGLONG MF_UNITS = 10000000; ///< Media Foundation's time base: 100-ns units

/**
 * @brief Releases a COM interface and nulls the pointer.
 * @param p interface pointer, may already be null.
 *
 * @details MinGW has no ComPtr/ATL, and every early-out of the writer below routes
 * through the same `cleanup` label, so one release helper is all that is needed.
 */
template<class T> static void ComRelease(T *&p)
{
    if(p){
        p->Release();
        p = nullptr;
    }
}

/**
 * @brief Wraps one frame's pixels in a time-stamped IMFSample.
 * @param img the frame, already converted to QImage::Format_RGB32.
 * @param w even width to encode (the image's leftmost `w` columns are used).
 * @param h even height to encode (the image's topmost `h` rows are used).
 * @param time presentation time in 100-ns units.
 * @param duration frame duration in 100-ns units.
 * @param out receives the sample on success; untouched on failure.
 * @return the first failing HRESULT, or S_OK.
 *
 * @details QImage::Format_RGB32 and MFVideoFormat_RGB32 are the same bytes in memory
 * (B,G,R,X little-endian), so no per-pixel conversion is needed — but MF's RGB32 is
 * bottom-up, hence the copy from the last row with a negative source stride.
 */
static HRESULT MakeSample(const QImage &img, int w, int h, LONGLONG time,
                          LONGLONG duration, IMFSample **out)
{
    const DWORD stride = (DWORD)w * 4;
    const qsizetype src_stride = img.bytesPerLine();
    IMFMediaBuffer *buffer = nullptr;
    IMFSample *sample = nullptr;
    BYTE *dst = nullptr;

    HRESULT hr = MFCreateMemoryBuffer(stride * (DWORD)h, &buffer);
    if(SUCCEEDED(hr))
        hr = buffer->Lock(&dst, nullptr, nullptr);
    if(SUCCEEDED(hr)){
        // RGB32 di Media Foundation è bottom-up: si copia dall'ultima riga con stride negativo
        hr = MFCopyImage(dst, (LONG)stride, img.constBits() + src_stride * (h - 1),
                         -(LONG)src_stride, stride, (DWORD)h);
        buffer->Unlock();
    }
    if(SUCCEEDED(hr))
        hr = buffer->SetCurrentLength(stride * (DWORD)h);
    if(SUCCEEDED(hr))
        hr = MFCreateSample(&sample);
    if(SUCCEEDED(hr))
        hr = sample->AddBuffer(buffer);
    if(SUCCEEDED(hr))
        hr = sample->SetSampleTime(time);
    if(SUCCEEDED(hr))
        hr = sample->SetSampleDuration(duration);
    ComRelease(buffer);
    if(FAILED(hr)){
        ComRelease(sample);
        return hr;
    }
    *out = sample;
    return S_OK;
}

/**
 * @details One sink writer, one video stream, one sample per frame. `stage` carries what
 * was being attempted so a failing HRESULT comes back as something readable instead of a
 * bare hex code. Every variable is declared before the first `goto cleanup` — jumping over
 * an initialization does not compile.
 */
bool WriteMp4WithMediaFoundation(const QString &out_file, const QStringList &frame_files,
                                 int fps, QString *error)
{
    if(frame_files.isEmpty()){
        if(error) *error = "no frames were rendered";
        return false;
    }
    const QImage probe(frame_files.first());
    if(probe.isNull()){
        if(error) *error = "the rendered frames could not be read back";
        return false;
    }
    // l'encoder H.264 rifiuta i lati dispari: si scarta al massimo una riga/colonna di bordo
    const int w = probe.width() & ~1;
    const int h = probe.height() & ~1;
    if(w < 2 || h < 2){
        if(error) *error = "the canvas is too small to encode";
        return false;
    }
    const int rate = fps > 0 ? fps : 24;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    // se il thread ha già COM inizializzato con un altro modello non è un errore: si prosegue
    // senza rilasciarlo, perché non è nostro
    const bool com_owned = SUCCEEDED(hr);
    if(FAILED(hr) && hr != RPC_E_CHANGED_MODE){
        if(error) *error = "COM could not be initialized";
        return false;
    }
    hr = MFStartup(MF_VERSION);
    if(FAILED(hr)){
        if(com_owned) CoUninitialize();
        if(error) *error = "Media Foundation is not available on this system";
        return false;
    }

    IMFAttributes *attributes = nullptr;
    IMFSinkWriter *writer = nullptr;
    IMFMediaType *out_type = nullptr;
    IMFMediaType *in_type = nullptr;
    DWORD stream = 0;
    LONGLONG time = 0;
    const LONGLONG duration = MF_UNITS / rate;
    // ponytail: bitrate euristico, un controllo qualità nella finestra di export se serve
    const UINT32 bitrate = (UINT32)qBound(Q_INT64_C(1000000),
                                          (qint64)w * h * rate / 10, Q_INT64_C(20000000));
    const char *stage = "starting up";
    bool ok = false;

    // senza DISABLE_THROTTLING il sink writer encoda al ritmo del tempo reale del video
    if(SUCCEEDED(MFCreateAttributes(&attributes, 2))){
        attributes->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);
        attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    }

    stage = "creating the output file";
    hr = MFCreateSinkWriterFromURL(reinterpret_cast<const wchar_t*>(out_file.utf16()),
                                   nullptr, attributes, &writer);
    if(FAILED(hr)) goto cleanup;

    stage = "configuring the H.264 encoder";
    hr = MFCreateMediaType(&out_type);
    if(FAILED(hr)) goto cleanup;
    hr = out_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if(FAILED(hr)) goto cleanup;
    hr = out_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    if(FAILED(hr)) goto cleanup;
    hr = out_type->SetUINT32(MF_MT_AVG_BITRATE, bitrate);
    if(FAILED(hr)) goto cleanup;
    hr = out_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if(FAILED(hr)) goto cleanup;
    hr = MFSetAttributeSize(out_type, MF_MT_FRAME_SIZE, w, h);
    if(FAILED(hr)) goto cleanup;
    hr = MFSetAttributeRatio(out_type, MF_MT_FRAME_RATE, rate, 1);
    if(FAILED(hr)) goto cleanup;
    hr = MFSetAttributeRatio(out_type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if(FAILED(hr)) goto cleanup;
    hr = writer->AddStream(out_type, &stream);
    if(FAILED(hr)) goto cleanup;

    // il sink writer inserisce da sé il convertitore RGB->YUV che l'encoder pretende
    stage = "setting up the RGB input";
    hr = MFCreateMediaType(&in_type);
    if(FAILED(hr)) goto cleanup;
    hr = in_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if(FAILED(hr)) goto cleanup;
    hr = in_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    if(FAILED(hr)) goto cleanup;
    hr = in_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if(FAILED(hr)) goto cleanup;
    hr = MFSetAttributeSize(in_type, MF_MT_FRAME_SIZE, w, h);
    if(FAILED(hr)) goto cleanup;
    hr = MFSetAttributeRatio(in_type, MF_MT_FRAME_RATE, rate, 1);
    if(FAILED(hr)) goto cleanup;
    hr = MFSetAttributeRatio(in_type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if(FAILED(hr)) goto cleanup;
    hr = writer->SetInputMediaType(stream, in_type, nullptr);
    if(FAILED(hr)) goto cleanup;

    stage = "starting the encoder";
    hr = writer->BeginWriting();
    if(FAILED(hr)) goto cleanup;

    stage = "encoding a frame";
    for(const QString &file : frame_files){
        const QImage img = QImage(file).convertToFormat(QImage::Format_RGB32);
        IMFSample *sample = nullptr;
        if(img.width() < w || img.height() < h){
            hr = E_FAIL; // un frame di dimensioni diverse dagli altri: la copia leggerebbe fuori
            goto cleanup;
        }
        hr = MakeSample(img, w, h, time, duration, &sample);
        if(SUCCEEDED(hr))
            hr = writer->WriteSample(stream, sample);
        ComRelease(sample);
        if(FAILED(hr)) goto cleanup;
        time += duration;
    }

    stage = "finalizing the file";
    hr = writer->Finalize();
    ok = SUCCEEDED(hr);

cleanup:
    ComRelease(in_type);
    ComRelease(out_type);
    ComRelease(writer);
    ComRelease(attributes);
    MFShutdown();
    if(com_owned)
        CoUninitialize();
    if(!ok && error)
        *error = QString("%1 failed (0x%2)").arg(stage)
                     .arg((quint32)hr, 8, 16, QChar('0'));
    return ok;
}
