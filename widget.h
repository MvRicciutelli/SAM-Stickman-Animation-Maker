#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QListWidget>
#include "myview.h"
QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

/**
 * @brief The toolbar/side-panel shell around the canvas: owns every drawing tool,
 *        layer/frame list, library panel and per-stick property control, and the
 *        `MyView` canvas itself.
 *
 * Widget is almost entirely a wall of `on_<objectName>_<signalName>` Designer
 * auto-connect slots (one per button/slider/spinbox in widget.ui) that translate a UI
 * action into a call on `view`/`view->my_animation` — the actual editing logic lives in
 * MyView/Animation/Frame/StickFigure, not here. Only a handful of hand-written methods
 * (config/paths/library setup, frame/stickfigure creation) don't follow that pattern.
 */
class Widget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Builds the UI, wires the canvas into it, and loads/creates the config,
     *        default library and terms-acceptance state.
     * @param parent parent widget (Qt ownership).
     *
     * Exits the process if the user declines the terms-and-conditions prompt on a
     * first run (see TermsAndConditions()).
     */
    Widget(QWidget *parent = nullptr);
    ~Widget();
    MyView *view; ///< the animation canvas (myview.h); owns the document being edited

    /**
     * @brief Adds a new, empty stickfigure layer to the current frame.
     * @return the newly created figure (becomes the frame's current stickfigure).
     */
    StickFigure* AddStick();

    /**
     * @brief Inserts a new frame right after the current one, makes it current, and
     *        gives it one empty stickfigure layer to start from.
     */
    void AddFrame();

    /**
     * @brief Thin forwarder to MyView::SetUpFrame() — creates the Frame/list-widget-item
     *        pair at the given position without making it current or adding any content.
     * @param pos position in Animation::frame_list for the new frame.
     * @return the newly created frame.
     */
    Frame *SetUpFrame(int pos);

    /**
     * @brief Ensures the per-user MarionettaPlus folders (documents root, libraries,
     *        temp render, animations, stick figures, video export) exist, creating any
     *        that are missing.
     */
    void CreatePaths();

    /**
     * @brief Rebuilds the library list widget from every *.marlib file found in the
     *        libraries folder.
     */
    void DetectLibraries();

    /**
     * @brief Writes the current canvas size/fps/handle size/autosave interval/FFmpeg
     *        path/terms-accepted state to config.json at the given path.
     * @param path destination file path.
     */
    void CreateJson(QString path);

    /**
     * @brief Loads settings from config.json at the given path into the corresponding
     *        globals; if any expected key is missing (e.g. an older config format), the
     *        file is discarded and recreated via CreateJson() instead.
     * @param path source file path.
     */
    void ReadJson(QString path);

    /**
     * @brief Convenience wrapper: re-writes config.json at its standard location with
     *        the current settings (CreateJson()).
     */
    void WriteJson();

    /**
     * @brief Shows the terms-and-conditions dialog; declining sets `terms_accepted = 0`
     *        (the constructor then exits the process) rather than closing just this
     *        dialog.
     */
    void TermsAndConditions();
private slots:
    void on_drawBtn_clicked();

    void on_cursor_clicked();

    void on_addStickBtn_clicked();

    void on_thicknessSpinBox_valueChanged(int arg1);

    void on_stickLayerView_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);

    void on_deleteStickFigureBtn_clicked();

    void on_moveLayerUpBtn_clicked();

    void on_moveLayerDownBtn_clicked();

    void on_drawCircleBtn_clicked();

    void on_chooseColorBtn_clicked();

    void on_undoBtn_clicked();

    void on_redoBtn_clicked();

    void on_addFrameBtn_clicked();

    void on_copyFrame_clicked();

    void on_onionSkinSpinBox_valueChanged(const QString &arg1);

    void on_deleteFrameBtn_clicked();

    void on_frameListWidget_itemClicked(QListWidgetItem *item);

    void on_stickLayerView_itemDoubleClicked(QListWidgetItem *item);

    void on_PlayButton_clicked();

    void on_zoomInBtn_clicked();

    void on_zoomOutBtn_clicked();

    void on_resetZoomBtn_clicked();

    void on_drawImageBtn_clicked();

    void on_imgHOffsetSlider_sliderReleased();

    void on_imgVOfsetSlider_sliderReleased();

    void on_imgWSlider_sliderReleased();

    void on_imgHSlider_sliderReleased();

    void on_imgRotationSlider_sliderReleased();

    void on_saveLibraryBtn_clicked();

    void on_addItemToLibBtn_clicked();

    void on_loadLibraryBtn_clicked();

    void on_refreshLibrariesBtn_clicked();

    void on_libraryList_currentRowChanged(int current_row);


    void on_removeFromLibraryBtn_clicked();

    void on_addItemFromLibraryToSceneBtn_clicked();

    void on_saveCurrentLibraryBtn_clicked();

    void on_imgHOffsetSlider_valueChanged(int value);

    void on_imgVOfsetSlider_valueChanged(int value);

    void on_imgWSlider_valueChanged(int value);

    void on_imgHSlider_valueChanged(int value);

    void on_imgRotationSlider_valueChanged(int value);

    void on_imgHOffsetSpinbox_valueChanged(int arg1);

    void on_imgVOffsetSpinbox_valueChanged(int arg1);

    void on_imgWidthSpinbox_valueChanged(int arg1);

    void on_imgHeightSpinbox_valueChanged(int arg1);

    void on_imgRotationSpinbox_valueChanged(int arg1);

    void on_setStickColorBtn_clicked();

    void on_setAllSticksColorBtn_clicked();

    void on_setPenThicknessBtn_clicked();

    void on_setPenThicknessAllBtn_clicked();

    void on_selectBrushColorBtn_clicked();

    void on_setAllBrushColorBtn_clicked();

    void on_setBrushColorBtn_clicked();

    void on_manipulateBtn_clicked();

    void on_penOpacitySlider_valueChanged(int value);

    void on_fillOpacitySlider_valueChanged(int value);

    void on_penOpacitySpinbox_valueChanged(int arg1);

    void on_fillOpacitySpinbox_valueChanged(int arg1);

    void on_rotateStickFigureBtn_clicked();

    void on_scaleStickFigureBtn_clicked();

    void on_stickFigureScaleSpinbox_editingFinished();

    void on_stickFigureRotationSpinbox_editingFinished();

    void on_moveSceneBtn_clicked();

    void on_scaleSceneBtn_clicked();

    void on_rotateSceneBtn_clicked();

    void on_verticalFlipBtn_clicked();

    void on_HorizontalFlipBtn_clicked();

    void on_copyStickFigureBtn_clicked();

    void on_pasteStickFigureBtn_clicked();

    void on_joinBtn_clicked();

    void on_splitBtn_clicked();

    void on_joinStick_clicked();

    void on_splitStick_clicked();


    void on_setMasterNodeBtn_clicked();

    void on_createNewLibraryBtn_clicked();

    void on_setAllDepthBtn_clicked();

    void on_setDepthBtn_clicked();

    void on_depthSpinbox_valueChanged(double arg1);

    void on_depthSlider_valueChanged(int value);



    void on_drawRectBtn_clicked();

    void on_pushButton_clicked();

    void on_drawTaperBtn_clicked();

    void on_drawTrapezoidBtn_clicked();

    void on_addImgBtn_clicked();

    void on_removeImgBtn_clicked();

    void on_setImgBtn_clicked();

    void on_moveStickFigureToCenterBtn_clicked();

    void on_pushToTopBtn_clicked();

    void on_pushToButtonBtn_clicked();

    void on_elongateBtn_clicked();

    void on_elongateSpinbox_editingFinished();

    void on_pushStickFigureToTop_clicked();

    void on_pushStickFigureToBottom_clicked();

    void on_moveStickUp_clicked();

    void on_moveStickDown_clicked();

    void on_deleteLibrary_clicked();

    void on_cpyFrame_clicked();

    void on_pasteFrame_clicked();


    void on_lineThicknessSlider_sliderMoved(int position);

    void on_clampToolBtn_clicked();

    void on_addPolyBtn_clicked();

    void on_multiLineBtn_clicked();

private:
    Ui::Widget *ui;

    /**
     * @brief Per-slider "drag in progress" latches: the first valueChanged() of a drag
     *        snapshots undo and raises its flag, the matching sliderReleased() lowers it,
     *        so a whole drag coalesces into ONE undoable edit. Was a file-scope global
     *        struct in widget.cpp — only Widget's own slots touch it, so it lives here now.
     */
    struct {
        bool sliderHOffset  = false;
        bool sliderVOffset  = false;
        bool sliderWScale   = false;
        bool sliderHScale   = false;
        bool sliderRot      = false;
        bool vertexWidth    = false; ///< vertex-width slider of the POLYGON/MULTILINE panel
        bool vertexCurve    = false; ///< corner-curvature slider of the same panel
    } sliderFlags;
};
#endif // WIDGET_H
