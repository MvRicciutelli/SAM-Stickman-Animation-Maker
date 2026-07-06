// Definitions for the globals that in the app live in widget.cpp / mymainwindow.cpp, which are
// not linked into the test binary. Types must match the app's definitions exactly (they link by
// name only). The widget pointers are instantiated by the fixture in tst_characterization.cpp.
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QImage>
#include <QTabWidget>
#include "uiItems.h"
#include "myview.h"

// --- owned by widget.cpp in the app ---
QGraphicsScene *scene = nullptr;
QGraphicsRectItem *myRect = nullptr;
QGraphicsRectItem *limitRect = nullptr;
QPointF onionOffset;
myView *V = nullptr;
int W = 720;
int H = 480;
int HANDLESIZE = 10;
int autoSaveInterval = 50;
bool playBack = false;
bool copyFlag = false;
bool switchFrameFlag = false;
float zoomLvl = 1;
QImage *imageDrawBuffer = nullptr;
QString imageNameBuffer;

// --- owned by mymainwindow.cpp in the app ---
int opCount = 0;
bool renderFlag = false;
QString animationPath = "";
bool loadFile = false;

// --- uiItems.h externs (owned by widget.cpp in the app) ---
QSpinBox *fpsSpinBox = nullptr;
QSlider *imgRotationSlider = nullptr;
QSpinBox *onionSkinSB = nullptr;
QSpinBox *imgHOffsetSpinbox = nullptr;
QSpinBox *imgVOffsetSpinbox = nullptr;
QSpinBox *imgWidthSpinbox = nullptr;
QSpinBox *imgHeightSpinbox = nullptr;
QSpinBox *imgRotationSpinbox = nullptr;
QSpinBox *penOpacitySpinbox = nullptr;
QSpinBox *brushOpacitySpinbox = nullptr;
QDoubleSpinBox *elongateSpinbox = nullptr;
QSlider *penOpacitySlider = nullptr;
QSlider *brushOpacitySlider = nullptr;
QSlider *depthSlider = nullptr;
QSpinBox *stickFigureScaleSpinbox = nullptr;
QSpinBox *stickFigureRotationSpinbox = nullptr;
QDoubleSpinBox *depthSpinbox = nullptr;
QListWidget *myStickFigureWidgetList = nullptr;
QListWidget *myFrameWidgetList = nullptr;
QListWidget *myLibraryListWidget = nullptr;
QListWidget *myCurrentLibraryWidget = nullptr;
QListWidget *imgListWidget = nullptr;
QLabel *dbgLbl = nullptr;
QSlider *imgHOffsetSlider = nullptr;
QSlider *imgVOffsetSlider = nullptr;
QSlider *imgWidthSlider = nullptr;
QSlider *imgHeightSlider = nullptr;
QSlider *taperWSlider = nullptr;
QSlider *advancedRectSlider = nullptr;
QSlider *editLineThicknessSlider = nullptr;
QSlider *taperHSlider = nullptr;
QCheckBox *hardTopCheck = nullptr;
QCheckBox *hardBottomCheck = nullptr;
advancedTab *advancedRectTab = nullptr;
QTabWidget *myTabWidget = nullptr;
QWidget *advancedImgTab = nullptr;
advancedLineWidget *advancedLineTab = nullptr;
advancedCircleWidget *advancedCircleTab = nullptr;
advancedTaperWidget *advancedTaperTab = nullptr;
QSpinBox *lineEditThicknessWidthSpinbox = nullptr;
QSlider *circleSquashSlider = nullptr;
QSpinBox *circleSquashSpinbox = nullptr;
QDir tempRenderFolder;
QDir myAnimationsFolder;
QDir myStickFiguresFolder;
