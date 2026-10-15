# Third-party notices

SAM itself is distributed under the PolyForm Strict License 1.0.0 —
see [LICENSE](LICENSE). It is a proprietary (source-available, noncommercial)
application, which the LGPLv3 expressly permits for a work that merely links
against LGPL libraries. The notices below cover the third-party components it
is built with.

## Qt

SAM is built against **Qt 6.6.1** (`core`, `gui`, `widgets`), used
under the **GNU Lesser General Public License v3** (LGPLv3). The full license
text is included at [LICENSES/LGPLv3.txt](LICENSES/LGPLv3.txt), together with
the [GPLv3](LICENSES/GPLv3.txt) it incorporates by reference — LGPLv3 §4(b)
requires both to accompany the combined work.

- Qt is linked **dynamically** — the Qt shared libraries (`Qt6Core.dll`,
  `Qt6Gui.dll`, `Qt6Widgets.dll`, etc.) ship alongside the executable and are
  not modified or statically linked in.
- Qt's own source code is available from The Qt Company at
  <https://download.qt.io/official_releases/qt/> (this build: the 6.6.1
  release archive).
- Qt has not been patched; no source obligation beyond linking to the above
  applies.
- Qt copyright notices are not stripped from the shipped binaries or
  installer. The application's **Help ▸ About** dialog names Qt, its copyright
  holder and where these license texts are found (LGPLv3 §4(a) and §4(c),
  the latter applying because that dialog displays copyright notices during
  execution), and **Help ▸ About Qt** shows Qt's own version and license
  information at runtime.
- Users are expressly permitted to modify, rebuild and replace the Qt
  libraries, to relink SAM against their own build of them, and to
  reverse engineer SAM as far as needed to debug such changes —
  see "Additional permissions" in [LICENSE](LICENSE). LGPLv3 §4 requires that
  the application's own terms not restrict this, so that grant is a
  compliance requirement rather than a courtesy.
- "Qt" and the Qt logo are trademarks of The Qt Company Ltd. This project is
  an independent application built with Qt and is not endorsed by or
  affiliated with The Qt Company.

## MinGW runtime (Windows builds)

Windows builds link the MinGW-w64 runtime (`libgcc`, `libstdc++`,
`libwinpthread`), distributed under GPLv3 with the GCC Runtime Library
Exception, which permits linking into non-GPL applications without imposing
a source-distribution requirement on this project.

## FFmpeg

FFmpeg is **not bundled**, and no FFmpeg code or binary is distributed with this
project. It is the optional, advanced half of video export: the user points the app
at their own installation through the export dialog's "Locate FFmpeg" button, and
SAM then runs that executable as a **subprocess** — it links no FFmpeg
code and shares no address space with it, so FFmpeg's licence attaches to the user's
own executable alone and never to this project.

MP4 export does not involve FFmpeg at all (see "Windows Media Foundation" below), so
a SAM install with no FFmpeg anywhere on the system still exports video.
The wmv, avi and gif formats are FFmpeg's, and are disabled until one is located.

Should a build ever be shipped alongside the application, it would have to be an
LGPL one (an `ffmpeg-*-lgpl` package: no `libx264`, `libx265` or other GPL-only
component) and the release would then also owe FFmpeg's licence text, that build's
exact version and `configure` line, and access to its corresponding sources from
the same place as the installer. None of that applies as things stand.

## Windows Media Foundation

MP4 (H.264) export uses the encoder built into Windows, through Media Foundation
(`mfplat`, `mfreadwrite` — see `mfvideo.cpp`). It is an operating-system component:
nothing is bundled, and the AVC patent licence covering it is the one that came
with the user's copy of Windows. The import libraries linked against it
(`libmfplat.a`, `libmfreadwrite.a`, `libmfuuid.a`) are part of the MinGW-w64
runtime covered above.
