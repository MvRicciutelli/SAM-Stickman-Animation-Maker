#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QListWidget>
#include "myview.h"
QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class QCheckBox;
class QTabBar;
class GradientDialog; // gradientdialog.h — opened by ChooseStyle() for the two gradient kinds
class PatternLibrary; // patternlibrary.h — the floating .pat picker, likewise
class TextureOptionsDialog; // textureoptionsdialog.h — the image fill's numeric options, likewise
class BrushStrokeDialog; // brushpicker.h — the brush border's tip and dynamics, likewise

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
     *        default library.
     * @param parent parent widget (Qt ownership).
     */
    Widget(QWidget *parent = nullptr);
    ~Widget();
    MyView *view; ///< the animation canvas (myview.h); owns the document being edited

    /**
     * @brief The things a fill or a stroke can be, i.e. the entries of the two style
     *        type combo boxes at the top of the Style column.
     *
     * Every kind here is something a QBrush can already be, which is the reason this
     * feature adds no field to StickData and no code to its serialization: a pattern and
     * an image are both Qt::TexturePattern, the two gradients are
     * Qt::LinearGradientPattern/Qt::RadialGradientPattern, and QDataStream's own QBrush
     * operators already write the tile, the stops, the spread and the coordinate mode.
     *
     * @note These are combo-box UserData values only — unlike StickData::StickType they
     * never reach a file, so they can be renumbered freely.
     * @note STYLE_PATTERN and STYLE_IMAGE are indistinguishable once applied (both are
     * Qt::TexturePattern), so a combo re-synced from a brush ALONE could only ever read
     * "Pattern" for either. That is why ApplyStyleBrush() takes the kind as an argument: the
     * one thing the brush cannot say is which of the two choosers the user came from, and
     * having the combo jump to "Pattern" right after picking an image made it look as though
     * the pick had gone to the wrong place.
     */
    enum StyleKind {
        STYLE_NONE,     ///< Qt::NoBrush: nothing painted (for a stroke, Qt::NoPen)
        STYLE_SOLID,    ///< a plain colour — the historical behaviour and the default
        STYLE_PATTERN,  ///< a tile picked from a .pat library in PatternLibrary
        STYLE_LINEAR,   ///< a QLinearGradient built in GradientDialog
        STYLE_RADIAL,   ///< a QRadialGradient built in GradientDialog
        STYLE_IMAGE,    ///< a tile read from any image file on disk
        /**
         * @brief BORDER ONLY: the outline is stamped dab by dab with a brush tip
         *        (StickData::stroke_brush), instead of being traced by the pen.
         *
         * The one entry that is NOT a QBrush and so the one that does add a field to StickData:
         * a QPen has no way to express "walk this path with this tip at this spacing". It is
         * absent from the Fill combo because a fill has no path to walk, and it leaves the pen
         * itself a plain solid colour — that colour, the pen's width and the pen's opacity are
         * what the dabs are stamped with, so the existing colour/thickness/opacity controls keep
         * working on it (see BrushStroke, abrbrush.h).
         */
        STYLE_BRUSH
    };

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
     * @brief Ensures the per-user SAM folders (documents root, libraries,
     *        temp render, animations, stick figures, video export) exist, creating any
     *        that are missing.
     */
    void CreatePaths();

    /**
     * @brief Rebuilds the library list widget from every *.samlib file found in the
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

    /** @brief Prompts for a name and stores it on the double-clicked frame. */
    void on_frameListWidget_itemDoubleClicked(QListWidgetItem *item);

    /**
     * @brief Asks how many inbetweens to generate and against which neighbour frame,
     *        then runs MyView::GenerateInbetweens() and reports any refusal.
     */
    void on_generateInbetweenBtn_clicked();

    void on_stickLayerView_itemDoubleClicked(QListWidgetItem *item);

    void on_PlayButton_clicked();

    void on_zoomInBtn_clicked();

    void on_resetZoomBtn_clicked();

    void on_rotateViewBtn_clicked();

    void on_panViewBtn_clicked();

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

    /** @brief Prompts for a new name and renames the double-clicked library entry. */
    void on_currentLibrary_itemDoubleClicked(QListWidgetItem *item);

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
    void on_penOpacitySlider_sliderReleased();
    void on_fillOpacitySlider_sliderReleased();

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

    void on_drawFreehandBtn_clicked();

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

    void on_addIkBtn_clicked();

    void on_drawTextBtn_clicked();

private:
    Ui::Widget *ui;

    /**
     * @brief The frames selected in the frame list, in frame order.
     *
     * The list is a ContiguousSelection (set in the constructor), so this is always one
     * unbroken block — shift-clicking a row extends it without changing which frame is
     * displayed. Rows are walked in list order rather than through selectedItems(), which
     * does not promise one; the list is kept parallel to Animation::frame_list by
     * Animation::SetupFrame()/MyView::DeleteFrame(). Falls back to the current frame when
     * nothing is selected, so every caller can treat it as "the frames to act on".
     */
    QList<Frame *> SelectedFrames() const;

    /**
     * @brief The copy-frame buffer: one FrameData per copied frame, in frame order.
     *
     * Pure data since refactor Phase 2b, and a member rather than the file-scope global it
     * used to be (only this class touches it — the same move the safety sweep made for the
     * slider latches). Empty until the first copy; owned here and freed in the destructor,
     * while a paste hands the undo stack Clone()s so the buffer survives repeated pastes.
     */
    QList<FrameData *> frame_copy_buffer;

    /**
     * @brief Builds fill_type_combo/stroke_type_combo and inserts each at the top of its
     *        group ("Fill" and "Border") in the Style column.
     *
     * Built in code rather than in widget.ui for the same reason the draw/text/vertex
     * panels are: an inserted widget is a two-line change here instead of a hundred lines
     * of generated XML.
     */
    void BuildStyleCombos();

    /**
     * @brief Re-parents the Style column into a two-tab widget ("Paint" = Fill + Border +
     *        the "Display options" group of texture check boxes, "Depth" = the Depth group).
     *
     * Must run after BuildStyleCombos(), which creates the four check boxes it groups.
     */
    void BuildStyleTabs();

    /// @brief The parameter set the "Stick Transform" tab edits on @p cs: local_xf or global_xf.
    StickXf &EditedXf(StickData *cs) const;
    /// @brief Refreshes what a Stick Transform edit on @p cs moved: @p cs, plus its descendants for a global one.
    void RefreshTransformed(StickData *cs);
    /// @brief The two flip check boxes: sets the edited set's flip as one undo step.
    void SetTransformFlip(bool horizontal, bool on);
    QTabBar *transform_mode_bar = nullptr;  ///< "Local" | "Global" above the Stick Transform sliders
    QCheckBox *flip_h_check = nullptr;      ///< Stick Transform: mirror across the stick
    QCheckBox *flip_v_check = nullptr;      ///< Stick Transform: mirror along the stick

    /**
     * @brief Puts the tools column, the canvas, the Style column and the layers column into
     *        one horizontal splitter, each side column inside its own scroll area, so the
     *        columns can be resized and still be used on a short or narrow screen.
     *
     * Must run after BuildStyleTabs(), which fills the Style column.
     */
    void BuildResizableSidebars();

    /**
     * @brief Opens the chooser for one StyleKind and makes the result the current fill or
     *        stroke.
     * @param for_fill true → the fill (view->my_brush), false → the stroke (the brush
     *        inside view->my_pen).
     * @param kind one of the StyleKind values, normally the combo's current UserData.
     * @param replace true → the user is ASSIGNING this kind (they picked it from the type
     *        combo), so the previous paint is replaced whole; false (the default) → they are
     *        EDITING the current one through its swatch button, which re-opens the options of
     *        whatever is already there. The distinction only bites for STYLE_IMAGE: a texture
     *        is a texture whatever it came from, so without it "Image" on a stick that already
     *        had a pattern (or another image) went straight to the placement options and never
     *        asked which picture, leaving the old tile in place.
     *
     * Nothing is written to a stick here. Like every other control in the Style column
     * this only updates the "current" pen/brush, which the Apply buttons
     * (on_setBrushColorBtn_clicked() and friends → MyView::SetGraphics()) then copy onto
     * the selected stick or the whole figure. The pattern branch is the one that does not
     * finish synchronously — see pattern_target_is_fill.
     */
    void ChooseStyle(bool for_fill, int kind, bool replace = false);

    /**
     * @brief Makes @p brush the current fill or stroke, then re-syncs its swatch and combo.
     * @param for_fill see ChooseStyle().
     * @param brush the brush to adopt; Qt::NoBrush becomes Qt::NoPen on the stroke side.
     * @param kind which StyleKind the brush came from, or -1 (the default) to work it out
     *        from the brush itself. Only ever passed explicitly for STYLE_IMAGE, the one
     *        kind a QBrush cannot be told apart from another (see StyleKind).
     *
     * Split out of ChooseStyle() because PatternLibrary is a non-modal palette: it calls
     * back here on every tile click, long after ChooseStyle() has returned.
     */
    void ApplyStyleBrush(bool for_fill, const QBrush &brush, int kind = -1);

    /**
     * @brief Repaints one of the two 20x20 swatch buttons (currentBrushColorBtn /
     *        currentColorBtn) with the brush it stands for.
     * @param for_fill see ChooseStyle().
     *
     * Draws into a button icon instead of setting the `background-color` stylesheet the
     * solid-colour handlers used to write, because a stylesheet colour cannot show a
     * gradient or a tile. Qt::NoBrush is drawn as the usual empty box with a diagonal.
     */
    void RefreshStyleSwatch(bool for_fill);

    /**
     * @brief Sets the current fill's or stroke's opacity, for both kinds of paint it can mean
     *        something for.
     * @param for_fill see ChooseStyle().
     * @param percent 0..100, straight from the slider.
     * @param live also apply it to the SELECTED STICK at once (true, what the sliders pass);
     *        false updates only the current paint, which is what ApplyStyleBrush() wants — it is
     *        adopting a brush, not editing the stick, and the Apply buttons carry it across.
     *
     * Shared by the two opacity sliders and by the choosers, so a freshly picked pattern or
     * image arrives at the opacity the slider already shows instead of jumping back to opaque.
     * See the implementation for where the alpha is stored per brush kind, and why a gradient
     * is left out.
     */
    void SetPaintOpacity(bool for_fill, int percent, bool live = true);

    /**
     * @brief The floating .pat picker, created on first use and kept for the session.
     *
     * Not a modal step: it stays open beside the canvas so several sticks can be given
     * tiles in a row, which is also why picking is asynchronous.
     */
    PatternLibrary *pattern_library = nullptr;

    /**
     * @brief The two gradient editors, one per kind, created on first use and kept alive
     *        so their stops and geometry survive from one opening to the next.
     */
    GradientDialog *linear_gradient_dialog = nullptr;
    GradientDialog *radial_gradient_dialog = nullptr; ///< @see linear_gradient_dialog

    /**
     * @brief The image fill's numeric options (scale/offset/angle/tile), created on first use.
     *
     * Unlike the gradient editors it does not need to be kept alive to remember anything —
     * TextureOptionsDialog::Seed() re-reads the brush on every opening — but it is, for the
     * same reason and at the same cost as those two.
     */
    TextureOptionsDialog *texture_options_dialog = nullptr;

    /**
     * @brief The "Brush" border's tip picker and dynamics, created on first use.
     *
     * Kept alive like the three above, and for one reason of its own beyond theirs: it embeds a
     * second BrushPicker, which scans the Brushes folder and decodes .abr collections, so
     * rebuilding it per opening would re-read the library every time.
     */
    BrushStrokeDialog *brush_stroke_dialog = nullptr;

    /**
     * @brief Which target the currently open pattern_library is picking for.
     *
     * Latched by ChooseStyle() at the moment the window is raised, because
     * PatternLibrary::patternChosen() arrives on a later click when that argument is long
     * gone.
     */
    bool pattern_target_is_fill = true;

    /**
     * @brief Side, in scene pixels, a texture tile is scaled to paint at.
     *
     * Owned here rather than by PatternLibrary because it governs BOTH texture sources: the
     * pattern window's "Tile size" spin box writes it through
     * PatternLibrary::patternChosen(), and the Image branch of ChooseStyle() reads it, so
     * the last size the user settled on carries over instead of an image fill silently
     * using a different default. See TextureBrush() in widget.cpp for why any of it is
     * needed.
     */
    int texture_tile_size = 48;

    /**
     * @brief Raised while refresh_text_panel() is pushing a stick's properties into the
     *        text panel, so the controls' own handlers can tell that echo from real user
     *        input (same role blockSignals() plays for the IK checkbox).
     */
    bool text_panel_sync = false;

    /**
     * @brief True once the current text-panel editing session has taken its undo
     *        snapshot: typing in the content box coalesces into ONE undo entry instead of
     *        one per keystroke. Reset by every panel refresh (i.e. every selection change).
     */
    bool text_undo_taken = false;

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
        bool penOpacity     = false; ///< stroke-opacity slider of the Style column
        bool fillOpacity    = false; ///< fill-opacity slider of the same
    } sliderFlags;
};
#endif // WIDGET_H
