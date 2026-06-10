// Take Control — background frame thumbnails for clips.
#ifndef TC_THUMBS_H__
#define TC_THUMBS_H__

#include "tc_core.h"

namespace tc
{

// Returns a cached thumbnail for (takeName, frame), or nullptr while it is
// still rendering. The first call enqueues a job: the take is isolated into
// its own document (on the main thread, cheap) and rendered at thumb size by
// a background worker using the Standard renderer.
// The returned bitmap is owned by the cache — draw it, do not free it.
BaseBitmap* TCThumbGet(BaseDocument* doc, const String& takeName, Int32 frame);

// monotonically increasing counter; bumped whenever a thumbnail finishes
UInt32 TCThumbVersion();

// drops all cached thumbnails and pending jobs (toolbar Refresh)
void TCThumbClear();

// stops the worker thread (PluginEnd)
void TCThumbShutdown();

static const maxon::Int32 TC_THUMB_W = 160;
static const maxon::Int32 TC_THUMB_H = 90;

} // namespace tc

#endif // TC_THUMBS_H__
