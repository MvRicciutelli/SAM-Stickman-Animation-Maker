#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QListWidget>
#include "myview.h"
QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();
    MyView *view;

    StickFigure* addStick();
    void AddFrame();
    Frame *SetUpFrame(int pos);
    void createPaths();
    void detectLibraries();
    void createJson(QString path);
    void readJson(QString path);
    void writeJson();
    void termsAndConditions();
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

    void on_libraryList_currentRowChanged(int currentRow);


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

private:
    Ui::Widget *ui;
};
#endif // WIDGET_H
