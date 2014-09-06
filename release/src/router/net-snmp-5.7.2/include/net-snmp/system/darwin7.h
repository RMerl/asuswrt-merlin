#include <stdint.h>
#include "freebsd4.h"
#define darwin darwin
/*
 * Although Darwin does have an fstab.h file, getfsfile etc. always return null.
 * At least, as of 5.3.
 */
#undef HAVE_FSTAB_H

#define MBSTAT_SYMBOL "mbstat"
#undef TOTAL_MEMORY_SYMBOL

#define SWAPFILE_DIR "/private/var/vm"
#define SWAPFILE_PREFIX "swapfile"

