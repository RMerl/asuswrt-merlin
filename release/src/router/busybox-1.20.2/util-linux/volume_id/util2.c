/* util.c is now split apart, for the benefit of Tomato.  So it can
 * pull in just enough busybox code to read disc labels, without
 * dragging in other un-needed stuff.
 * It would be better if Tomato could use "busybox.so", but busybox
 * can't currently build a shared .so configuration. */
#define UTIL2
#include "util.c"
