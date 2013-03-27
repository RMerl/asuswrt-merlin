#ifndef DECODE_H
#define DECODE_H

#undef WITH_PROFILE_MODEL_P

#ifdef WANT_ISA_COMPACT
#include "decode-compact.h"
#include "defs-compact.h"
#endif /* WANT_ISA_COMPACT */

#ifdef WANT_ISA_MEDIA
#include "decode-media.h"
#include "defs-media.h"
#endif /* WANT_ISA_MEDIA */

#endif /* DECODE_H */
