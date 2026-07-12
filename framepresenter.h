#ifndef FRAMEPRESENTER_H
#define FRAMEPRESENTER_H
#include <QGraphicsScene>
#include <stickdata.h>
#include <stick.h>

/**
 * @brief The view-lifecycle boundary for individual sticks (REFACTOR_PLAN.md Phase 2c).
 *
 * A small, stateless pair of functions rather than the stateful "owns the whole
 * frame's presentation" class the plan originally sketched — most of that job
 * (tearing down/rebuilding a frame's list-widget entries, onion skins, icons) already
 * lives in `MyView::MoveToFrame()` and stayed there; what genuinely needed a single,
 * consistent entry point was just "how does one `StickData` get a `stick` view, and
 * how does it lose one," which is what these two functions are.
 */
namespace FramePresenter {

/**
 * @brief Gives a StickData an on-screen presence.
 * @param stick_data record to present; must not already have a view (`stick_data->view`
 *        is expected null — call `DetachView()` first if it doesn't).
 * @param scene scene to add the new view to.
 * @return the new view; also reachable afterwards via `stick_data->view`.
 *
 * Constructs a `stick` wrapping `stick_data` (wiring `stick_data->view` as a side
 * effect of the constructor) and adds it to `scene`. Does not add the endpoint
 * handles to the scene — callers that want them visible/interactive call
 * `MyView::AddHandles()` separately, exactly as before this phase, since not every
 * attach wants them (render clones for onion skins/icons never do).
 */
StickGraphicsItem *AttachView(StickData *stick_data, QGraphicsScene *scene);

/**
 * @brief Removes a StickData's on-screen presence, if it has one.
 * @param stick_data record to detach; safe to call even if `stick_data->view` is
 *        already null (no-op).
 * @param scene scene to remove the view from before deleting it.
 *
 * Mirrors `AttachView()`: removes the view from `scene` and deletes it (the
 * destructor clears `stick_data->view` back to null). Does not touch the endpoint
 * handles' scene membership — callers call `MyView::RemoveHandles()` first, same
 * division of labor as `AttachView()`.
 */
void DetachView(StickData *stick_data, QGraphicsScene *scene);

}

#endif // FRAMEPRESENTER_H
