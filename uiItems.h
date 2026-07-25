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
extern QSpinBox* fps_spin_box;
extern QSlider* img_rotation_slider;
extern QSpinBox * onion_skin_sb;
extern QSpinBox * img_h_offset_spinbox;
extern QSpinBox * img_v_offset_spinbox;
extern QSpinBox * img_width_spinbox;
extern QSpinBox * img_height_spinbox;
extern QSpinBox * img_rotation_spinbox;
extern QSpinBox * pen_opacity_spinbox;
extern QSpinBox * brush_opacity_spinbox;
extern QDoubleSpinBox * elongate_spinbox;
extern QSlider * pen_opacity_slider;
extern QSlider * brush_opacity_slider;
extern QSlider * depth_slider;
extern QSpinBox *stick_figure_scale_spinbox;
extern QSpinBox *stick_figure_rotation_spinbox;
extern QDoubleSpinBox * depth_spinbox;
extern QListWidget * my_stick_figure_widget_list;
extern QListWidget * my_frame_widget_list;
extern QListWidget * my_library_list_widget ;
extern QListWidget * my_current_library_widget;
extern QListWidget * img_list_widget;
extern QLabel * dbg_lbl;
extern QSlider* img_h_offset_slider;
extern QSlider* img_v_offset_slider;
extern QSlider* img_width_slider;
extern QSlider* img_height_slider;
extern QSlider* taper_w_slider;
extern QSlider* advanced_rect_slider;
extern QSlider* edit_line_thickness_slider;
extern QSlider* taper_h_slider;
extern QCheckBox* hard_top_check;
extern QCheckBox* hard_bottom_check;
extern AdvancedTab* advanced_rect_tab;
extern QTabWidget* my_tab_widget;
extern QWidget * advanced_img_tab;
extern QWidget * advanced_vertex_tab;   ///< POLYGON/MULTILINE vertex panel (built in Widget's ctor)
extern QWidget * advanced_ik_tab;       ///< "IK properties" panel, shown only when the selected stick has an IK role (built in Widget's ctor)
extern QCheckBox * ik_active_check;     ///< the "Chain active" toggle inside advanced_ik_tab (IK_TARGET_ON <-> IK_TARGET_OFF)
extern QWidget * vertex_width_row;      ///< the "Vertex width" row inside it (hidden for POLYGON)
extern QSlider * vertex_width_slider;
extern QSlider * vertex_curve_slider;
extern AdvancedLineWidget* advanced_line_tab;
extern AdvancedCircleWidget* advanced_circle_tab;
extern AdvancedTaperWidget* advanced_taper_tab;
extern QSpinBox * line_edit_thickness_width_spinbox;
extern QSlider*  circle_squash_slider;
extern QSpinBox * circle_squash_spinbox;

extern QDir temp_render_folder;
extern QDir my_animations_folder;
extern QDir my_stick_figures_folder;
#endif // UIITEMS_H
