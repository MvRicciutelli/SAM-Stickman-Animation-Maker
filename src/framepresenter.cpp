#include "framepresenter.h"

namespace FramePresenter {

/**
 * @details The whole point of this function: one place that pairs a `new StickGraphicsItem(...)`
 * with a `scene->addItem(...)`, so nothing else in the codebase constructs a stick
 * view without also attaching it (or forgets to).
 */
StickGraphicsItem *AttachView(StickData *stick_data, QGraphicsScene *scene)
{
    StickGraphicsItem *view = new StickGraphicsItem(stick_data);
    scene->addItem(view);
    return view;
}

/**
 * @details `delete` alone would suffice for correctness (a QGraphicsItem auto-detaches
 * from its scene in its own destructor — see CLAUDE.md), but removing first avoids the
 * scene repainting/re-indexing for an item about to disappear anyway.
 */
void DetachView(StickData *stick_data, QGraphicsScene *scene)
{
    if(!stick_data->view)
        return;
    scene->removeItem(stick_data->view);
    delete stick_data->view;
}

}
