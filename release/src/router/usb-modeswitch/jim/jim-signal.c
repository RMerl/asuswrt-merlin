
/*
 * jim-signal.c
 *
 */

#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "jim.h"
#include "jimautoconf.h"
#include "jim-subcmd.h"
#include "jim-signal.h"

#define MAX_SIGNALS (sizeof(jim_wide) * 8)

static jim_wide *sigloc;
static jim_wide sigsblocked;
static struct sigaction *sa_old;
static int signal_handling[MAX_SIGNALS];

/* Make sure to do this as a wide, not int */
#define sig_to_bit(SIG) ((jim_wide)1 << (SIG))

static void signal_handler(int sig)
{
    /* We just remember which signals occurred. Jim_Eval() will
     * notice this as soon as it can and throw an error
     */
    *sigloc |= sig_to_bit(sig);
}

static void signal_ignorer(int sig)
{
    /* We just remember which signals occurred */
    sigsblocked |= sig_to_bit(sig);
}

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
#define CHECK_SIG(NAME) if (sig == NAME) return #NAME

const char *Jim_SignalId(int sig)
{
    CHECK_SIG(SIGABRT);
    CHECK_SIG(SIGALRM);
    CHECK_SIG(SIGBUS);
    CHECK_SIG(SIGCHLD);
    CHECK_SIG(SIGCONT);
    CHECK_SIG(SIGFPE);
    CHECK_SIG(SIGHUP);
    CHECK_SIG(SIGILL);
    CHECK_SIG(SIGINT);
#ifdef SIGIO
    CHECK_SIG(SIGIO);
#endif
    CHECK_SIG(SIGKILL);
    CHECK_SIG(SIGPIPE);
    CHECK_SIG(SIGPROF);
    CHECK_SIG(SIGQUIT);
    CHECK_SIG(SIGSEGV);
    CHECK_SIG(SIGSTOP);
    CHECK_SIG(SIGSYS);
    CHECK_SIG(SIGTERM);
    CHECK_SIG(SIGTRAP);
    CHECK_SIG(SIGTSTP);
    CHECK_SIG(SIGTTIN);
    CHECK_SIG(SIGTTOU);
    CHECK_SIG(SIGURG);
    CHECK_SIG(SIGUSR1);
    CHECK_SIG(SIGUSR2);
    CHECK_SIG(SIGVTALRM);
    CHECK_SIG(SIGWINCH);
    CHECK_SIG(SIGXCPU);
    CHECK_SIG(SIGXFSZ);
#ifdef SIGPWR
    CHECK_SIG(SIGPWR);
#endif
#ifdef SIGCLD
    CHECK_SIG(SIGCLD);
#endif
#ifdef SIGEMT
    CHECK_SIG(SIGEMT);
#endif
#ifdef SIGLOST
    CHECK_SIG(SIGLOST);
#endif
#ifdef SIGPOLL
    CHECK_SIG(SIGPOLL);
#endif
#ifdef SIGINFO
    CHECK_SIG(SIGINFO);
#endif
    return "unknown signal";
}

const char *Jim_SignalName(int sig)
{
#ifdef HAVE_SYS_SIGLIST
    if (sig >= 0 && sig < NSIG) {
        return sys_siglist[sig];
    }
#endif
    return Jim_SignalId(sig);
}

/**
 * Given the name of a signal, returns the signal value if found,
 * or returns -1 (and sets an error) if not found.
 * We accept -SIGINT, SIGINT, INT or any lowercase version or a number,
 * either positive or negative.
 */
static int find_signal_by_name(Jim_Interp *interp, const char *name)
{
    int i;
    const char *pt = name;

    /* Remove optional - and SIG from the front of the name */
    if (*pt == '-') {
        pt++;
    }
    if (strncasecmp(name, "sig", 3) == 0) {
        pt += 3;
    }
    if (isdigit(UCHAR(pt[0]))) {
        i = atoi(pt);
        if (i > 0 && i < MAX_SIGNALS) {
            return i;
        }
    }
    else {
        for (i = 1; i < MAX_SIGNALS; i++) {
            /* Jim_SignalId() returns names such as SIGINT, and
             * returns "unknown signal id" if unknown, so this will work
             */
            if (strcasecmp(Jim_SignalId(i) + 3, pt) == 0) {
                return i;
            }
        }
    }
    Jim_SetResultString(interp, "unknown signal ", -1);
    Jim_AppendString(interp, Jim_GetResult(interp), name, -1);

    return -1;
}

#define SIGNAL_ACTION_HANDLE 1
#define SIGNAL_ACTION_IGNORE -1
#define SIGNAL_ACTION_DEFAULT 0

static int do_signal_cmd(Jim_Interp *interp, int action, int argc, Jim_Obj *const *argv)
{
    struct sigaction sa;
    int i;

    if (argc == 0) {
        Jim_SetResult(interp, Jim_NewListObj(interp, NULL, 0));
        for (i = 1; i < MAX_SIGNALS; i++) {
            if (signal_handling[i] == action) {
                /* Add signal name to the list  */
                Jim_ListAppendElement(interp, Jim_GetResult(interp),
                    Jim_NewStringObj(interp, Jim_SignalId(i), -1));
            }
        }
        return JIM_OK;
    }

    /* Catch all the signals we care about */
    if (action != SIGNAL_ACTION_DEFAULT) {
        sa.sa_flags = 0;
        sigemptyset(&sa.sa_mask);
        if (action == SIGNAL_ACTION_HANDLE) {
            sa.sa_handler = signal_handler;
        }
        else {
            sa.sa_handler = signal_ignorer;
        }
    }

    /* Iterate through the provided signals */
    for (i = 0; i < argc; i++) {
        int sig = find_signal_by_name(interp, Jim_String(argv[i]));

        if (sig < 0) {
            return JIM_ERR;
        }
        if (action != signal_handling[sig]) {
            /* Need to change the action for this signal */
            switch (action) {
                case SIGNAL_ACTION_HANDLE:
                case SIGNAL_ACTION_IGNORE:
                    if (signal_handling[sig] == SIGNAL_ACTION_DEFAULT) {
                        if (!sa_old) {
                            /* Allocate the structure the first time through */
                            sa_old = Jim_Alloc(sizeof(*sa_old) * MAX_SIGNALS);
                        }
                        sigaction(sig, &sa, &sa_old[sig]);
                    }
                    else {
                        sigaction(sig, &sa, 0);
                    }
                    break;

                case SIGNAL_ACTION_DEFAULT:
                    /* Restore old handler */
                    if (sa_old) {
                        sigaction(sig, &sa_old[sig], 0);
                    }
            }
            signal_handling[sig] = action;
        }
    }

    return JIM_OK;
}

static int signal_cmd_handle(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return do_signal_cmd(interp, SIGNAL_ACTION_HANDLE, argc, argv);
}

static int signal_cmd_ignore(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return do_signal_cmd(interp, SIGNAL_ACTION_IGNORE, argc, argv);
}

static int signal_cmd_default(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return do_signal_cmd(interp, SIGNAL_ACTION_DEFAULT, argc, argv);
}

static int signal_set_sigmask_result(Jim_Interp *interp, jim_wide sigmask)
{
    int i;
    Jim_Obj *listObj = Jim_NewListObj(interp, NULL, 0);

    for (i = 0; i < MAX_SIGNALS; i++) {
        if (sigmask & sig_to_bit(i)) {
            Jim_ListAppendElement(interp, listObj, Jim_NewStringObj(interp, Jim_SignalId(i), -1));
        }
    }
    Jim_SetResult(interp, listObj);
    return JIM_OK;
}

static int signal_cmd_check(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int clear = 0;
    jim_wide mask = 0;
    jim_wide blocked;

    if (argc > 0 && Jim_CompareStringImmediate(interp, argv[0], "-clear")) {
        clear++;
    }
    if (argc > clear) {
        int i;

        /* Signals specified */
        for (i = clear; i < argc; i++) {
            int sig = find_signal_by_name(interp, Jim_String(argv[i]));

            if (sig < 0 || sig >= MAX_SIGNALS) {
                return -1;
            }
            mask |= sig_to_bit(sig);
        }
    }
    else {
        /* No signals specified, so check/clear all */
        mask = ~mask;
    }

    if ((sigsblocked & mask) == 0) {
        /* No matching signals, so empty result and nothing to do */
        return JIM_OK;
    }
    /* Be careful we don't have a race condition where signals are cleared but not returned */
    blocked = sigsblocked & mask;
    if (clear) {
        sigsblocked &= ~blocked;
    }
    /* Set the result */
    signal_set_sigmask_result(interp, blocked);
    return JIM_OK;
}

static int signal_cmd_throw(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int sig = SIGINT;

    if (argc == 1) {
        if ((sig = find_signal_by_name(interp, Jim_String(argv[0]))) < 0) {
            return JIM_ERR;
        }
    }

    /* If the signal is ignored (blocked) ... */
    if (signal_handling[sig] == SIGNAL_ACTION_IGNORE) {
        sigsblocked |= sig_to_bit(sig);
        return JIM_OK;
    }

    /* Just set the signal */
    interp->sigmask |= sig_to_bit(sig);

    /* Set the canonical name of the signal as the result */
    Jim_SetResultString(interp, Jim_SignalId(sig), -1);

    /* And simply say we caught the signal */
    return JIM_SIGNAL;
}

/*
 *-----------------------------------------------------------------------------
 *
 * Jim_SignalCmd --
 *     Implements the TCL signal command:
 *         signal handle|ignore|default|throw ?signals ...?
 *         signal throw signal
 *
 *     Specifies which signals are handled by Tcl code.
 *     If the one of the given signals is caught, it causes a JIM_SIGNAL
 *     exception to be thrown which can be caught by catch.
 *
 *     Use 'signal ignore' to ignore the signal(s)
 *     Use 'signal default' to go back to the default behaviour
 *     Use 'signal throw signal' to raise the given signal
 *
 *     If no arguments are given, returns the list of signals which are being handled
 *
 * Results:
 *      Standard TCL results.
 *
 *-----------------------------------------------------------------------------
 */
static const jim_subcmd_type signal_command_table[] = {
    {   .cmd = "handle",
        .args = "?signals ...?",
        .function = signal_cmd_handle,
        .minargs = 0,
        .maxargs = -1,
        .description = "Lists handled signals, or adds to handled signals"
    },
    {   .cmd = "ignore",
        .args = "?signals ...?",
        .function = signal_cmd_ignore,
        .minargs = 0,
        .maxargs = -1,
        .description = "Lists ignored signals, or adds to ignored signals"
    },
    {   .cmd = "default",
        .args = "?signals ...?",
        .function = signal_cmd_default,
        .minargs = 0,
        .maxargs = -1,
        .description = "Lists defaulted signals, or adds to defaulted signals"
    },
    {   .cmd = "check",
        .args = "?-clear? ?signals ...?",
        .function = signal_cmd_check,
        .minargs = 0,
        .maxargs = -1,
        .description = "Returns ignored signals which have occurred, and optionally clearing them"
    },
    {   .cmd = "throw",
        .args = "?signal?",
        .function = signal_cmd_throw,
        .minargs = 0,
        .maxargs = 1,
        .description = "Raises the given signal (default SIGINT)"
    },
    { 0 }
};

static int Jim_AlarmCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int ret;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "seconds");
        return JIM_ERR;
    }
    else {
#ifdef HAVE_UALARM
        double t;

        ret = Jim_GetDouble(interp, argv[1], &t);
        if (ret == JIM_OK) {
            if (t < 1) {
                ualarm(t * 1e6, 0);
            }
            else {
                alarm(t);
            }
        }
#else
        long t;

        ret = Jim_GetLong(interp, argv[1], &t);
        if (ret == JIM_OK) {
            alarm(t);
        }
#endif
    }

    return ret;
}

static int Jim_SleepCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int ret;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "seconds");
        return JIM_ERR;
    }
    else {
        double t;

        ret = Jim_GetDouble(interp, argv[1], &t);
        if (ret == JIM_OK) {
#ifdef HAVE_USLEEP
            if (t < 1) {
                usleep(t * 1e6);
            }
            else
#endif
                sleep(t);
        }
    }

    return ret;
}

static int Jim_KillCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int sig;
    long pid;
    Jim_Obj *pidObj;
    const char *signame;

    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "?SIG|-0? pid");
        return JIM_ERR;
    }

    if (argc == 2) {
        signame = "SIGTERM";
        pidObj = argv[1];
    }
    else {
        signame = Jim_String(argv[1]);
        pidObj = argv[2];
    }

    /* Special 'kill -0 pid' to determine if a pid exists */
    if (strcmp(signame, "-0") == 0 || strcmp(signame, "0") == 0) {
        sig = 0;
    }
    else {
        sig = find_signal_by_name(interp, signame);
        if (sig < 0) {
            return JIM_ERR;
        }
    }

    if (Jim_GetLong(interp, pidObj, &pid) != JIM_OK) {
        return JIM_ERR;
    }

    if (kill(pid, sig) == 0) {
        return JIM_OK;
    }

    Jim_SetResultString(interp, "kill: Failed to deliver signal", -1);
    return JIM_ERR;
}

int Jim_signalInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "signal", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    /* Teach the jim core how to set a result from a sigmask */
    interp->signal_set_result = signal_set_sigmask_result;

    /* Make sure we know where to store the signals which occur */
    sigloc = &interp->sigmask;

    Jim_CreateCommand(interp, "signal", Jim_SubCmdProc, (void *)signal_command_table, NULL);
    Jim_CreateCommand(interp, "alarm", Jim_AlarmCmd, 0, 0);
    Jim_CreateCommand(interp, "kill", Jim_KillCmd, 0, 0);

    /* Sleep is slightly dubious here */
    Jim_CreateCommand(interp, "sleep", Jim_SleepCmd, 0, 0);
    return JIM_OK;
}
