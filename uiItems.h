#ifndef UIITEMS_H
#define UIITEMS_H

#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QSlider>
#include <QSpinBox>
#include "advancedcirclewidget.h"
#include "advancedlinewidget.h"
#include "advancedtab.h"
#include "advancedtaperwidget.h"
#include <QDir>
extern QSpinBox* fpsSpinBox;
extern QSlider* imgRotationSlider;
extern QSpinBox * onionSkinSB;
extern QSpinBox * imgHOffsetSpinbox;
extern QSpinBox * imgVOffsetSpinbox;
extern QSpinBox * imgWidthSpinbox;
extern QSpinBox * imgHeightSpinbox;
extern QSpinBox * imgRotationSpinbox;
extern QSpinBox * penOpacitySpinbox;
extern QSpinBox * brushOpacitySpinbox;
extern QDoubleSpinBox * elongateSpinbox;
extern QSlider * penOpacitySlider;
extern QSlider * brushOpacitySlider;
extern QSlider * depthSlider;
extern QSpinBox *stickFigureScaleSpinbox;
extern QSpinBox *stickFigureRotationSpinbox;
extern QDoubleSpinBox * depthSpinbox;
extern QListWidget * myStickFigureWidgetList;
extern QListWidget * myFrameWidgetList;
extern QListWidget * myLibraryListWidget ;
extern QListWidget * myCurrentLibraryWidget;
extern QListWidget * imgListWidget;
extern QLabel * dbgLbl;
extern QSlider* imgHOffsetSlider;
extern QSlider* imgVOffsetSlider;
extern QSlider* imgWidthSlider;
extern QSlider* imgHeightSlider;
extern QSlider* taperWSlider;
extern QSlider* advancedRectSlider;
extern QSlider* editLineThicknessSlider;
extern QSlider* taperHSlider;
extern QCheckBox* hardTopCheck;
extern QCheckBox* hardBottomCheck;
extern AdvancedTab* advanced_rect_tab;
extern QTabWidget* myTabWidget;
extern QWidget * advancedImgTab;
extern advancedLineWidget* advancedLineTab;
extern advancedCircleWidget* advancedCircleTab;
extern advancedTaperWidget* advancedTaperTab;
extern QSpinBox * lineEditThicknessWidthSpinbox;
extern QSpinBox* lineEditThicknessWidthSpinbox;
extern QSlider*  circleSquashSlider;
extern QSpinBox * circleSquashSpinbox;

extern QDir tempRenderFolder;
extern QDir myAnimationsFolder;
extern QDir myStickFiguresFolder;
#endif // UIITEMS_H
