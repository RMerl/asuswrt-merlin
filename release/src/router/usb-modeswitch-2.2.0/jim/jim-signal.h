#ifndef JIM_SIGNAL_H
#define JIM_SIGNAL_H

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SignalId --
 *
 *      Return a textual identifier for a signal number.
 *
 * Results:
 *      This procedure returns a machine-readable textual identifier
 *      that corresponds to sig.  The identifier is the same as the
 *      #define name in signal.h.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
const char *Jim_SignalId(int sig);
const char *Jim_SignalName(int sig);

#endif
