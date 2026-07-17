#ifndef ANIMATIONOPTIONS_H
#define ANIMATIONOPTIONS_H

#include <QWidget>
#include <QObject>

// --- How existing frames follow a canvas-size change; the chosen value travels through
// --- DialogOut -> MyMainWindow::GetAnimationParams() -> MyView::SizeChange().
#define NOREPO 0    ///< leave figures where they are
#define REPO 1      ///< recentre figures on the new canvas
#define REPOSCALE 2 ///< recentre AND rescale figures to the new size
namespace Ui {
class AnimationOptions;
}

/**
 * @brief The Animation Options dialog: edits fps, canvas size, handle size and the
 *        autosave interval, and asks how to fit existing frames to a new size.
 *
 * Opened (single-instance) by MyMainWindow::on_actionAnimation_Options_triggered(),
 * pre-filled with the current values passed to the constructor. Pressing OK stores the
 * edited values on the public fields and emits DialogOut(option_val); MyMainWindow::
 * GetAnimationParams() reads the fields and applies them, then deletes the dialog.
 */
class AnimationOptions : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Builds the dialog pre-filled with the current settings.
     * @param parent parent widget.
     * @param W current canvas width. @param H current canvas height.
     * @param FPS current frames-per-second.
     * @param handles current endpoint-handle size.
     * @param auto_save_input current autosave interval (operations between autosaves).
     */
    explicit AnimationOptions(QWidget *parent = nullptr, int W =800, int H = 600, int FPS = 24,int handles = 10, int auto_save_input = 50);
    ~AnimationOptions();
    int width = 1;              ///< chosen canvas width (read by GetAnimationParams on OK)
    int height = 1;             ///< chosen canvas height
    int fps = 24;               ///< chosen frames-per-second
    int auto_save_interval = 50; ///< chosen autosave interval
    int handle_size = 10;       ///< chosen endpoint-handle size
private slots:
    void on_okBtn_clicked();

    void on_widthSpinbox_valueChanged(int arg1);

    void on_heightSpinbox_valueChanged(int arg1);

    void on_widthSpinbox_editingFinished();

    void on_heightSpinbox_editingFinished();

    void on_handlesSizeSpinBox_valueChanged(int arg1);

private:
    Ui::AnimationOptions *ui;
signals:
    /**
     * @brief Emitted on OK (after the confirm prompt) with the chosen reposition mode.
     * @param option_val NOREPO / REPO / REPOSCALE, forwarded to MyView::SizeChange().
     */
    void DialogOut(int option_val);
};

#endif // ANIMATIONOPTIONS_H
