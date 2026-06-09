// Take Control — modal take/clip settings window.
#ifndef TC_SETTINGS_H__
#define TC_SETTINGS_H__

#include "tc_core.h"

namespace tc
{

// Opens the modal settings window. clipId may be NOTOK for take-only mode.
// Returns true if something was changed (applied with undo).
Bool TCOpenSettings(Int32 clipId, const String& takeName);

} // namespace tc

#endif // TC_SETTINGS_H__
