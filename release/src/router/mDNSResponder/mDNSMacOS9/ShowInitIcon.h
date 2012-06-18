#ifndef __ShowInitIcon__
#define __ShowInitIcon__

#include <Types.h>

// Usage: pass the ID of your icon family (ICN#/icl4/icl8) to have it drawn in the right spot.
// If 'advance' is true, the next INIT icon will be drawn to the right of your icon. If it is false, the next INIT icon will overwrite
// yours. You can use it to create animation effects by calling ShowInitIcon several times with 'advance' set to false.

#ifdef __cplusplus
extern "C" {
#endif

pascal void ShowInitIcon (short iconFamilyID, Boolean advance);

#ifdef __cplusplus
}
#endif

#endif /* __ShowInitIcon__ */
