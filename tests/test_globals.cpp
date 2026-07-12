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
QGraphicsRectItem *my_rect = nullptr;
QGraphicsRectItem *limit_rect = nullptr;
QPointF onion_offset;
MyView *V = nullptr;
int W = 720;
int H = 480;
int handle_size = 10;
int auto_save_interval = 50;
bool play_back = false;
float zoom_lvl = 1;
QImage *image_draw_buffer = nullptr;
QString image_name_buffer;

// --- owned by mymainwindow.cpp in the app ---
int op_count = 0;
bool render_flag = false;
QString animation_path = "";
bool load_file = false;

// --- uiItems.h externs (owned by widget.cpp in the app) ---
QSpinBox *fps_spin_box = nullptr;
QSlider *img_rotation_slider = nullptr;
QSpinBox *onion_skin_sb = nullptr;
QSpinBox *img_h_offset_spinbox = nullptr;
QSpinBox *img_v_offset_spinbox = nullptr;
QSpinBox *img_width_spinbox = nullptr;
QSpinBox *img_height_spinbox = nullptr;
QSpinBox *img_rotation_spinbox = nullptr;
QSpinBox *pen_opacity_spinbox = nullptr;
QSpinBox *brush_opacity_spinbox = nullptr;
QDoubleSpinBox *elongate_spinbox = nullptr;
QSlider *pen_opacity_slider = nullptr;
QSlider *brush_opacity_slider = nullptr;
QSlider *depth_slider = nullptr;
QSpinBox *stick_figure_scale_spinbox = nullptr;
QSpinBox *stick_figure_rotation_spinbox = nullptr;
QDoubleSpinBox *depth_spinbox = nullptr;
QListWidget *my_stick_figure_widget_list = nullptr;
QListWidget *my_frame_widget_list = nullptr;
QListWidget *my_library_list_widget = nullptr;
QListWidget *my_current_library_widget = nullptr;
QListWidget *img_list_widget = nullptr;
QLabel *dbg_lbl = nullptr;
QSlider *img_h_offset_slider = nullptr;
QSlider *img_v_offset_slider = nullptr;
QSlider *img_width_slider = nullptr;
QSlider *img_height_slider = nullptr;
QSlider *taper_w_slider = nullptr;
QSlider *advanced_rect_slider = nullptr;
QSlider *edit_line_thickness_slider = nullptr;
QSlider *taper_h_slider = nullptr;
QCheckBox *hard_top_check = nullptr;
QCheckBox *hard_bottom_check = nullptr;
AdvancedTab *advanced_rect_tab = nullptr;
QTabWidget *my_tab_widget = nullptr;
QWidget *advanced_img_tab = nullptr;
AdvancedLineWidget *advanced_line_tab = nullptr;
AdvancedCircleWidget *advanced_circle_tab = nullptr;
AdvancedTaperWidget *advanced_taper_tab = nullptr;
QSpinBox *line_edit_thickness_width_spinbox = nullptr;
QSlider *circle_squash_slider = nullptr;
QSpinBox *circle_squash_spinbox = nullptr;
QDir temp_render_folder;
QDir my_animations_folder;
QDir my_stick_figures_folder;
