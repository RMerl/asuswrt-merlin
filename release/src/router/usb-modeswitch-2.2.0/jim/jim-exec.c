/*
 * (c) 2008 Steve Bennett <steveb@workware.net.au>
 *
 * Implements the exec command for Jim
 *
 * Based on code originally from Tcl 6.7 by John Ousterhout.
 * From that code:
 *
 * The Tcl_Fork and Tcl_WaitPids procedures are based on code
 * contributed by Karl Lehenbauer, Mark Diekhans and Peter
 * da Silva.
 *
 * Copyright 1987-1991 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#include <string.h>
#include <ctype.h>

#include "jim.h"
#include "jimautoconf.h"

#if (!defined(HAVE_VFORK) || !defined(HAVE_WAITPID)) && !defined(__MINGW32__)
/* Poor man's implementation of exec with system()
 * The system() call *may* do command line redirection, etc.
 * The standard output is not available.
 * Can't redirect filehandles.
 */
static int Jim_ExecCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *cmdlineObj = Jim_NewEmptyStringObj(interp);
    int i, j;
    int rc;

    /* Create a quoted command line */
    for (i = 1; i < argc; i++) {
        int len;
        const char *arg = Jim_GetString(argv[i], &len);

        if (i > 1) {
            Jim_AppendString(interp, cmdlineObj, " ", 1);
        }
        if (strpbrk(arg, "\\\" ") == NULL) {
            /* No quoting required */
            Jim_AppendString(interp, cmdlineObj, arg, len);
            continue;
        }

        Jim_AppendString(interp, cmdlineObj, "\"", 1);
        for (j = 0; j < len; j++) {
            if (arg[j] == '\\' || arg[j] == '"') {
                Jim_AppendString(interp, cmdlineObj, "\\", 1);
            }
            Jim_AppendString(interp, cmdlineObj, &arg[j], 1);
        }
        Jim_AppendString(interp, cmdlineObj, "\"", 1);
    }
    rc = system(Jim_String(cmdlineObj));

    Jim_FreeNewObj(interp, cmdlineObj);

    if (rc) {
        Jim_Obj *errorCode = Jim_NewListObj(interp, NULL, 0);
        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, "CHILDSTATUS", -1));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, 0));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, rc));
        Jim_SetGlobalVariableStr(interp, "errorCode", errorCode);
        return JIM_ERR;
    }

    return JIM_OK;
}

int Jim_execInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "exec", "1.0", JIM_ERRMSG))
        return JIM_ERR;
    Jim_CreateCommand(interp, "exec", Jim_ExecCmd, NULL, NULL);
    return JIM_OK;
}
#else
/* Full exec implementation for unix and mingw */

#include <errno.h>
#include <signal.h>

#define XXX printf("@%s:%d\n", __FILE__, __LINE__); fflush(stdout);

#if defined(__MINGW32__)
    /* XXX: Should we use this implementation for cygwin too? */
    #include <fcntl.h>

    typedef HANDLE fdtype;
    typedef HANDLE pidtype;
    #define JIM_BAD_FD INVALID_HANDLE_VALUE
    #define JIM_BAD_PID INVALID_HANDLE_VALUE
    #define JimCloseFd CloseHandle

    #define WIFEXITED(STATUS) 1
    #define WEXITSTATUS(STATUS) (STATUS)
    #define WIFSIGNALED(STATUS) 0
    #define WTERMSIG(STATUS) 0
    #define WNOHANG 1

    static fdtype JimFileno(FILE *fh);
    static pidtype JimWaitPid(pidtype pid, int *status, int nohang);
    static fdtype JimDupFd(fdtype infd);
    static fdtype JimOpenForRead(const char *filename);
    static FILE *JimFdOpenForRead(fdtype fd);
    static int JimPipe(fdtype pipefd[2]);
    static pidtype JimStartWinProcess(Jim_Interp *interp, char **argv, char *env,
        fdtype inputId, fdtype outputId, fdtype errorId);
    static int JimErrno(void);
#else
    #include "jim-signal.h"
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/wait.h>

    typedef int fdtype;
    typedef int pidtype;
    #define JimPipe pipe
    #define JimErrno() errno
    #define JIM_BAD_FD -1
    #define JIM_BAD_PID -1
    #define JimFileno fileno
    #define JimReadFd read
    #define JimCloseFd close
    #define JimWaitPid waitpid
    #define JimDupFd dup
    #define JimFdOpenForRead(FD) fdopen((FD), "r")
    #define JimOpenForRead(NAME) open((NAME), O_RDONLY, 0)
#endif

static const char *JimStrError(void);
static char **JimSaveEnv(char **env);
static void JimRestoreEnv(char **env);
static int JimCreatePipeline(Jim_Interp *interp, int argc, Jim_Obj *const *argv,
    pidtype **pidArrayPtr, fdtype *inPipePtr, fdtype *outPipePtr, fdtype *errFilePtr);
static void JimDetachPids(Jim_Interp *interp, int numPids, const pidtype *pidPtr);
static int JimCleanupChildren(Jim_Interp *interp, int numPids, pidtype *pidPtr, fdtype errorId);
static fdtype JimCreateTemp(Jim_Interp *interp, const char *contents);
static fdtype JimOpenForWrite(const char *filename, int append);
static int JimRewindFd(fdtype fd);

static void Jim_SetResultErrno(Jim_Interp *interp, const char *msg)
{
    Jim_SetResultFormatted(interp, "%s: %s", msg, JimStrError());
}

static const char *JimStrError(void)
{
    return strerror(JimErrno());
}

static void Jim_RemoveTrailingNewline(Jim_Obj *objPtr)
{
    int len;
    const char *s = Jim_GetString(objPtr, &len);

    if (len > 0 && s[len - 1] == '\n') {
        objPtr->length--;
        objPtr->bytes[objPtr->length] = '\0';
    }
}

/**
 * Read from 'fd', append the data to strObj and close 'fd'.
 * Returns JIM_OK if OK, or JIM_ERR on error.
 */
static int JimAppendStreamToString(Jim_Interp *interp, fdtype fd, Jim_Obj *strObj)
{
    char buf[256];
    FILE *fh = JimFdOpenForRead(fd);
    if (fh == NULL) {
        return JIM_ERR;
    }

    while (1) {
        int retval = fread(buf, 1, sizeof(buf), fh);
        if (retval > 0) {
            Jim_AppendString(interp, strObj, buf, retval);
        }
        if (retval != sizeof(buf)) {
            break;
        }
    }
    Jim_RemoveTrailingNewline(strObj);
    fclose(fh);
    return JIM_OK;
}

/*
 * If the last character of the result is a newline, then remove
 * the newline character (the newline would just confuse things).
 *
 * Note: Ideally we could do this by just reducing the length of stringrep
 *       by 1, but there is no API for this :-(
 */
static void JimTrimTrailingNewline(Jim_Interp *interp)
{
    int len;
    const char *p = Jim_GetString(Jim_GetResult(interp), &len);

    if (len > 0 && p[len - 1] == '\n') {
        Jim_SetResultString(interp, p, len - 1);
    }
}

/**
 * Builds the environment array from $::env
 *
 * If $::env is not set, simply returns environ.
 *
 * Otherwise allocates the environ array from the contents of $::env
 *
 * If the exec fails, memory can be freed via JimFreeEnv()
 */
static char **JimBuildEnv(Jim_Interp *interp)
{
#if defined(jim_ext_tclcompat)
    int i;
    int size;
    int num;
    int n;
    char **envptr;
    char *envdata;

    Jim_Obj *objPtr = Jim_GetGlobalVariableStr(interp, "env", JIM_NONE);

    if (!objPtr) {
        return Jim_GetEnviron();
    }

    /* We build the array as a single block consisting of the pointers followed by
     * the strings. This has the advantage of being easy to allocate/free and being
     * compatible with both unix and windows
     */

    /* Calculate the required size */
    num = Jim_ListLength(interp, objPtr);
    if (num % 2) {
        num--;
    }
    size = Jim_Length(objPtr);
    /* We need one \0 and one equal sign for each element.
     * A list has at least one space for each element except the first.
     * We only need one extra char for the extra null terminator.
     */
    size++;

    envptr = Jim_Alloc(sizeof(*envptr) * (num / 2 + 1) + size);
    envdata = (char *)&envptr[num / 2 + 1];

    n = 0;
    for (i = 0; i < num; i += 2) {
        const char *s1, *s2;
        Jim_Obj *elemObj;

        Jim_ListIndex(interp, objPtr, i, &elemObj, JIM_NONE);
        s1 = Jim_String(elemObj);
        Jim_ListIndex(interp, objPtr, i + 1, &elemObj, JIM_NONE);
        s2 = Jim_String(elemObj);

        envptr[n] = envdata;
        envdata += sprintf(envdata, "%s=%s", s1, s2);
        envdata++;
        n++;
    }
    envptr[n] = NULL;
    *envdata = 0;

    return envptr;
#else
    return Jim_GetEnviron();
#endif
}

/**
 * Frees the environment allocated by JimBuildEnv()
 *
 * Must pass original_environ.
 */
static void JimFreeEnv(char **env, char **original_environ)
{
#ifdef jim_ext_tclcompat
    if (env != original_environ) {
        Jim_Free(env);
    }
#endif
}

/*
 * Create error messages for unusual process exits.  An
 * extra newline gets appended to each error message, but
 * it gets removed below (in the same fashion that an
 * extra newline in the command's output is removed).
 */
static int JimCheckWaitStatus(Jim_Interp *interp, pidtype pid, int waitStatus)
{
    Jim_Obj *errorCode = Jim_NewListObj(interp, NULL, 0);
    int rc = JIM_ERR;

    if (WIFEXITED(waitStatus)) {
        if (WEXITSTATUS(waitStatus) == 0) {
            Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, "NONE", -1));
            rc = JIM_OK;
        }
        else {
            Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, "CHILDSTATUS", -1));
            Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, (long)pid));
            Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, WEXITSTATUS(waitStatus)));
        }
    }
    else {
        const char *type;
        const char *action;

        if (WIFSIGNALED(waitStatus)) {
            type = "CHILDKILLED";
            action = "killed";
        }
        else {
            type = "CHILDSUSP";
            action = "suspended";
        }

        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, type, -1));

#ifdef jim_ext_signal
        Jim_SetResultFormatted(interp, "child %s by signal %s", action, Jim_SignalId(WTERMSIG(waitStatus)));
        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, Jim_SignalId(WTERMSIG(waitStatus)), -1));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, pid));
        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, Jim_SignalName(WTERMSIG(waitStatus)), -1));
#else
        Jim_SetResultFormatted(interp, "child %s by signal %d", action, WTERMSIG(waitStatus));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, WTERMSIG(waitStatus)));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, (long)pid));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, WTERMSIG(waitStatus)));
#endif
    }
    Jim_SetGlobalVariableStr(interp, "errorCode", errorCode);
    return rc;
}

/*
 * Data structures of the following type are used by JimFork and
 * JimWaitPids to keep track of child processes.
 */

struct WaitInfo
{
    pidtype pid;                    /* Process id of child. */
    int status;                 /* Status returned when child exited or suspended. */
    int flags;                  /* Various flag bits;  see below for definitions. */
};

struct WaitInfoTable {
    struct WaitInfo *info;
    int size;
    int used;
};

/*
 * Flag bits in WaitInfo structures:
 *
 * WI_DETACHED -        Non-zero means no-one cares about the
 *                      process anymore.  Ignore it until it
 *                      exits, then forget about it.
 */

#define WI_DETACHED 2

#define WAIT_TABLE_GROW_BY 4

static void JimFreeWaitInfoTable(struct Jim_Interp *interp, void *privData)
{
    struct WaitInfoTable *table = privData;

    Jim_Free(table->info);
    Jim_Free(table);
}

static struct WaitInfoTable *JimAllocWaitInfoTable(void)
{
    struct WaitInfoTable *table = Jim_Alloc(sizeof(*table));
    table->info = NULL;
    table->size = table->used = 0;

    return table;
}

/*
 * The main [exec] command
 */
static int Jim_ExecCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    fdtype outputId;               /* File id for output pipe.  -1
                                 * means command overrode. */
    fdtype errorId;                /* File id for temporary file
                                 * containing error output. */
    pidtype *pidPtr;
    int numPids, result;

    /*
     * See if the command is to be run in background;  if so, create
     * the command, detach it, and return.
     */
    if (argc > 1 && Jim_CompareStringImmediate(interp, argv[argc - 1], "&")) {
        Jim_Obj *listObj;
        int i;

        argc--;
        numPids = JimCreatePipeline(interp, argc - 1, argv + 1, &pidPtr, NULL, NULL, NULL);
        if (numPids < 0) {
            return JIM_ERR;
        }
        /* The return value is a list of the pids */
        listObj = Jim_NewListObj(interp, NULL, 0);
        for (i = 0; i < numPids; i++) {
            Jim_ListAppendElement(interp, listObj, Jim_NewIntObj(interp, (long)pidPtr[i]));
        }
        Jim_SetResult(interp, listObj);
        JimDetachPids(interp, numPids, pidPtr);
        Jim_Free(pidPtr);
        return JIM_OK;
    }

    /*
     * Create the command's pipeline.
     */
    numPids =
        JimCreatePipeline(interp, argc - 1, argv + 1, &pidPtr, NULL, &outputId, &errorId);

    if (numPids < 0) {
        return JIM_ERR;
    }

    /*
     * Read the child's output (if any) and put it into the result.
     */
    Jim_SetResultString(interp, "", 0);

    result = JIM_OK;
    if (outputId != JIM_BAD_FD) {
        result = JimAppendStreamToString(interp, outputId, Jim_GetResult(interp));
        if (result < 0) {
            Jim_SetResultErrno(interp, "error reading from output pipe");
        }
    }

    if (JimCleanupChildren(interp, numPids, pidPtr, errorId) != JIM_OK) {
        result = JIM_ERR;
    }
    return result;
}

static void JimReapDetachedPids(struct WaitInfoTable *table)
{
    struct WaitInfo *waitPtr;
    int count;

    if (!table) {
        return;
    }

    for (waitPtr = table->info, count = table->used; count > 0; waitPtr++, count--) {
        if (waitPtr->flags & WI_DETACHED) {
            int status;
            pidtype pid = JimWaitPid(waitPtr->pid, &status, WNOHANG);
            if (pid != JIM_BAD_PID) {
                if (waitPtr != &table->info[table->used - 1]) {
                    *waitPtr = table->info[table->used - 1];
                }
                table->used--;
            }
        }
    }
}

/**
 * Does waitpid() on the given pid, and then removes the
 * entry from the wait table.
 *
 * Returns the pid if OK and updates *statusPtr with the status,
 * or JIM_BAD_PID if the pid was not in the table.
 */
static pidtype JimWaitForProcess(struct WaitInfoTable *table, pidtype pid, int *statusPtr)
{
    int i;

    /* Find it in the table */
    for (i = 0; i < table->used; i++) {
        if (pid == table->info[i].pid) {
            /* wait for it */
            JimWaitPid(pid, statusPtr, 0);

            /* Remove it from the table */
            if (i != table->used - 1) {
                table->info[i] = table->info[table->used - 1];
            }
            table->used--;
            return pid;
        }
    }

    /* Not found */
    return JIM_BAD_PID;
}

/*
 *----------------------------------------------------------------------
 *
 * JimDetachPids --
 *
 *  This procedure is called to indicate that one or more child
 *  processes have been placed in background and are no longer
 *  cared about.  These children can be cleaned up with JimReapDetachedPids().
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

static void JimDetachPids(Jim_Interp *interp, int numPids, const pidtype *pidPtr)
{
    int j;
    struct WaitInfoTable *table = Jim_CmdPrivData(interp);

    for (j = 0; j < numPids; j++) {
        /* Find it in the table */
        int i;
        for (i = 0; i < table->used; i++) {
            if (pidPtr[j] == table->info[i].pid) {
                table->info[i].flags |= WI_DETACHED;
                break;
            }
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * JimCreatePipeline --
 *
 *  Given an argc/argv array, instantiate a pipeline of processes
 *  as described by the argv.
 *
 * Results:
 *  The return value is a count of the number of new processes
 *  created, or -1 if an error occurred while creating the pipeline.
 *  *pidArrayPtr is filled in with the address of a dynamically
 *  allocated array giving the ids of all of the processes.  It
 *  is up to the caller to free this array when it isn't needed
 *  anymore.  If inPipePtr is non-NULL, *inPipePtr is filled in
 *  with the file id for the input pipe for the pipeline (if any):
 *  the caller must eventually close this file.  If outPipePtr
 *  isn't NULL, then *outPipePtr is filled in with the file id
 *  for the output pipe from the pipeline:  the caller must close
 *  this file.  If errFilePtr isn't NULL, then *errFilePtr is filled
 *  with a file id that may be used to read error output after the
 *  pipeline completes.
 *
 * Side effects:
 *  Processes and pipes are created.
 *
 *----------------------------------------------------------------------
 */
static int
JimCreatePipeline(Jim_Interp *interp, int argc, Jim_Obj *const *argv, pidtype **pidArrayPtr,
    fdtype *inPipePtr, fdtype *outPipePtr, fdtype *errFilePtr)
{
    pidtype *pidPtr = NULL;         /* Points to malloc-ed array holding all
                                 * the pids of child processes. */
    int numPids = 0;            /* Actual number of processes that exist
                                 * at *pidPtr right now. */
    int cmdCount;               /* Count of number of distinct commands
                                 * found in argc/argv. */
    const char *input = NULL;   /* Describes input for pipeline, depending
                                 * on "inputFile".  NULL means take input
                                 * from stdin/pipe. */

#define FILE_NAME   0           /* input/output: filename */
#define FILE_APPEND 1           /* output only:  filename, append */
#define FILE_HANDLE 2           /* input/output: filehandle */
#define FILE_TEXT   3           /* input only:   input is actual text */

    int inputFile = FILE_NAME;  /* 1 means input is name of input file.
                                 * 2 means input is filehandle name.
                                 * 0 means input holds actual
                                 * text to be input to command. */

    int outputFile = FILE_NAME; /* 0 means output is the name of output file.
                                 * 1 means output is the name of output file, and append.
                                 * 2 means output is filehandle name.
                                 * All this is ignored if output is NULL
                                 */
    int errorFile = FILE_NAME;  /* 0 means error is the name of error file.
                                 * 1 means error is the name of error file, and append.
                                 * 2 means error is filehandle name.
                                 * All this is ignored if error is NULL
                                 */
    const char *output = NULL;  /* Holds name of output file to pipe to,
                                 * or NULL if output goes to stdout/pipe. */
    const char *error = NULL;   /* Holds name of stderr file to pipe to,
                                 * or NULL if stderr goes to stderr/pipe. */
    fdtype inputId = JIM_BAD_FD;
                                 /* Readable file id input to current command in
                                 * pipeline (could be file or pipe).  JIM_BAD_FD
                                 * means use stdin. */
    fdtype outputId = JIM_BAD_FD;
                                 /* Writable file id for output from current
                                 * command in pipeline (could be file or pipe).
                                 * JIM_BAD_FD means use stdout. */
    fdtype errorId = JIM_BAD_FD;
                                 /* Writable file id for all standard error
                                 * output from all commands in pipeline.  JIM_BAD_FD
                                 * means use stderr. */
    fdtype lastOutputId = JIM_BAD_FD;
                                 /* Write file id for output from last command
                                 * in pipeline (could be file or pipe).
                                 * -1 means use stdout. */
    fdtype pipeIds[2];           /* File ids for pipe that's being created. */
    int firstArg, lastArg;      /* Indexes of first and last arguments in
                                 * current command. */
    int lastBar;
    int i;
    pidtype pid;
    char **save_environ;
    struct WaitInfoTable *table = Jim_CmdPrivData(interp);

    /* Holds the args which will be used to exec */
    char **arg_array = Jim_Alloc(sizeof(*arg_array) * (argc + 1));
    int arg_count = 0;

    JimReapDetachedPids(table);

    if (inPipePtr != NULL) {
        *inPipePtr = JIM_BAD_FD;
    }
    if (outPipePtr != NULL) {
        *outPipePtr = JIM_BAD_FD;
    }
    if (errFilePtr != NULL) {
        *errFilePtr = JIM_BAD_FD;
    }
    pipeIds[0] = pipeIds[1] = JIM_BAD_FD;

    /*
     * First, scan through all the arguments to figure out the structure
     * of the pipeline.  Count the number of distinct processes (it's the
     * number of "|" arguments).  If there are "<", "<<", or ">" arguments
     * then make note of input and output redirection and remove these
     * arguments and the arguments that follow them.
     */
    cmdCount = 1;
    lastBar = -1;
    for (i = 0; i < argc; i++) {
        const char *arg = Jim_String(argv[i]);

        if (arg[0] == '<') {
            inputFile = FILE_NAME;
            input = arg + 1;
            if (*input == '<') {
                inputFile = FILE_TEXT;
                input++;
            }
            else if (*input == '@') {
                inputFile = FILE_HANDLE;
                input++;
            }

            if (!*input && ++i < argc) {
                input = Jim_String(argv[i]);
            }
        }
        else if (arg[0] == '>') {
            int dup_error = 0;

            outputFile = FILE_NAME;

            output = arg + 1;
            if (*output == '>') {
                outputFile = FILE_APPEND;
                output++;
            }
            if (*output == '&') {
                /* Redirect stderr too */
                output++;
                dup_error = 1;
            }
            if (*output == '@') {
                outputFile = FILE_HANDLE;
                output++;
            }
            if (!*output && ++i < argc) {
                output = Jim_String(argv[i]);
            }
            if (dup_error) {
                errorFile = outputFile;
                error = output;
            }
        }
        else if (arg[0] == '2' && arg[1] == '>') {
            error = arg + 2;
            errorFile = FILE_NAME;

            if (*error == '@') {
                errorFile = FILE_HANDLE;
                error++;
            }
            else if (*error == '>') {
                errorFile = FILE_APPEND;
                error++;
            }
            if (!*error && ++i < argc) {
                error = Jim_String(argv[i]);
            }
        }
        else {
            if (strcmp(arg, "|") == 0 || strcmp(arg, "|&") == 0) {
                if (i == lastBar + 1 || i == argc - 1) {
                    Jim_SetResultString(interp, "illegal use of | or |& in command", -1);
                    goto badargs;
                }
                lastBar = i;
                cmdCount++;
            }
            /* Either |, |& or a "normal" arg, so store it in the arg array */
            arg_array[arg_count++] = (char *)arg;
            continue;
        }

        if (i >= argc) {
            Jim_SetResultFormatted(interp, "can't specify \"%s\" as last word in command", arg);
            goto badargs;
        }
    }

    if (arg_count == 0) {
        Jim_SetResultString(interp, "didn't specify command to execute", -1);
badargs:
        Jim_Free(arg_array);
        return -1;
    }

    /* Must do this before vfork(), so do it now */
    save_environ = JimSaveEnv(JimBuildEnv(interp));

    /*
     * Set up the redirected input source for the pipeline, if
     * so requested.
     */
    if (input != NULL) {
        if (inputFile == FILE_TEXT) {
            /*
             * Immediate data in command.  Create temporary file and
             * put data into file.
             */
            inputId = JimCreateTemp(interp, input);
            if (inputId == JIM_BAD_FD) {
                goto error;
            }
        }
        else if (inputFile == FILE_HANDLE) {
            /* Should be a file descriptor */
            Jim_Obj *fhObj = Jim_NewStringObj(interp, input, -1);
            FILE *fh = Jim_AioFilehandle(interp, fhObj);

            Jim_FreeNewObj(interp, fhObj);
            if (fh == NULL) {
                goto error;
            }
            inputId = JimDupFd(JimFileno(fh));
        }
        else {
            /*
             * File redirection.  Just open the file.
             */
            inputId = JimOpenForRead(input);
            if (inputId == JIM_BAD_FD) {
                Jim_SetResultFormatted(interp, "couldn't read file \"%s\": %s", input, JimStrError());
                goto error;
            }
        }
    }
    else if (inPipePtr != NULL) {
        if (JimPipe(pipeIds) != 0) {
            Jim_SetResultErrno(interp, "couldn't create input pipe for command");
            goto error;
        }
        inputId = pipeIds[0];
        *inPipePtr = pipeIds[1];
        pipeIds[0] = pipeIds[1] = JIM_BAD_FD;
    }

    /*
     * Set up the redirected output sink for the pipeline from one
     * of two places, if requested.
     */
    if (output != NULL) {
        if (outputFile == FILE_HANDLE) {
            Jim_Obj *fhObj = Jim_NewStringObj(interp, output, -1);
            FILE *fh = Jim_AioFilehandle(interp, fhObj);

            Jim_FreeNewObj(interp, fhObj);
            if (fh == NULL) {
                goto error;
            }
            fflush(fh);
            lastOutputId = JimDupFd(JimFileno(fh));
        }
        else {
            /*
             * Output is to go to a file.
             */
            lastOutputId = JimOpenForWrite(output, outputFile == FILE_APPEND);
            if (lastOutputId == JIM_BAD_FD) {
                Jim_SetResultFormatted(interp, "couldn't write file \"%s\": %s", output, JimStrError());
                goto error;
            }
        }
    }
    else if (outPipePtr != NULL) {
        /*
         * Output is to go to a pipe.
         */
        if (JimPipe(pipeIds) != 0) {
            Jim_SetResultErrno(interp, "couldn't create output pipe");
            goto error;
        }
        lastOutputId = pipeIds[1];
        *outPipePtr = pipeIds[0];
        pipeIds[0] = pipeIds[1] = JIM_BAD_FD;
    }
    /* If we are redirecting stderr with 2>filename or 2>@fileId, then we ignore errFilePtr */
    if (error != NULL) {
        if (errorFile == FILE_HANDLE) {
            if (strcmp(error, "1") == 0) {
                /* Special 2>@1 */
                if (lastOutputId != JIM_BAD_FD) {
                    errorId = JimDupFd(lastOutputId);
                }
                else {
                    /* No redirection of stdout, so just use 2>@stdout */
                    error = "stdout";
                }
            }
            if (errorId == JIM_BAD_FD) {
                Jim_Obj *fhObj = Jim_NewStringObj(interp, error, -1);
                FILE *fh = Jim_AioFilehandle(interp, fhObj);

                Jim_FreeNewObj(interp, fhObj);
                if (fh == NULL) {
                    goto error;
                }
                fflush(fh);
                errorId = JimDupFd(JimFileno(fh));
            }
        }
        else {
            /*
             * Output is to go to a file.
             */
            errorId = JimOpenForWrite(error, errorFile == FILE_APPEND);
            if (errorId == JIM_BAD_FD) {
                Jim_SetResultFormatted(interp, "couldn't write file \"%s\": %s", error, JimStrError());
                goto error;
            }
        }
    }
    else if (errFilePtr != NULL) {
        /*
         * Set up the standard error output sink for the pipeline, if
         * requested.  Use a temporary file which is opened, then deleted.
         * Could potentially just use pipe, but if it filled up it could
         * cause the pipeline to deadlock:  we'd be waiting for processes
         * to complete before reading stderr, and processes couldn't complete
         * because stderr was backed up.
         */
        errorId = JimCreateTemp(interp, NULL);
        if (errorId == JIM_BAD_FD) {
            goto error;
        }
        *errFilePtr = JimDupFd(errorId);
    }

    /*
     * Scan through the argc array, forking off a process for each
     * group of arguments between "|" arguments.
     */

    pidPtr = Jim_Alloc(cmdCount * sizeof(*pidPtr));
    for (i = 0; i < numPids; i++) {
        pidPtr[i] = JIM_BAD_PID;
    }
    for (firstArg = 0; firstArg < arg_count; numPids++, firstArg = lastArg + 1) {
        int pipe_dup_err = 0;
        fdtype origErrorId = errorId;

        for (lastArg = firstArg; lastArg < arg_count; lastArg++) {
            if (arg_array[lastArg][0] == '|') {
                if (arg_array[lastArg][1] == '&') {
                    pipe_dup_err = 1;
                }
                break;
            }
        }
        /* Replace | with NULL for execv() */
        arg_array[lastArg] = NULL;
        if (lastArg == arg_count) {
            outputId = lastOutputId;
        }
        else {
            if (JimPipe(pipeIds) != 0) {
                Jim_SetResultErrno(interp, "couldn't create pipe");
                goto error;
            }
            outputId = pipeIds[1];
        }

        /* Now fork the child */

#ifdef __MINGW32__
        pid = JimStartWinProcess(interp, &arg_array[firstArg], save_environ ? save_environ[0] : NULL, inputId, outputId, errorId);
        if (pid == JIM_BAD_PID) {
            Jim_SetResultFormatted(interp, "couldn't exec \"%s\"", arg_array[firstArg]);
            goto error;
        }
#else
        /*
         * Disable SIGPIPE signals:  if they were allowed, this process
         * might go away unexpectedly if children misbehave.  This code
         * can potentially interfere with other application code that
         * expects to handle SIGPIPEs;  what's really needed is an
         * arbiter for signals to allow them to be "shared".
         */
        if (table->info == NULL) {
            (void)signal(SIGPIPE, SIG_IGN);
        }

        /* Need to do this befor vfork() */
        if (pipe_dup_err) {
            errorId = outputId;
        }

        /*
         * Make a new process and enter it into the table if the fork
         * is successful.
         */
        pid = vfork();
        if (pid < 0) {
            Jim_SetResultErrno(interp, "couldn't fork child process");
            goto error;
        }
        if (pid == 0) {
            /* Child */

            if (inputId != -1) dup2(inputId, 0);
            if (outputId != -1) dup2(outputId, 1);
            if (errorId != -1) dup2(errorId, 2);

            for (i = 3; (i <= outputId) || (i <= inputId) || (i <= errorId); i++) {
                close(i);
            }

            execvp(arg_array[firstArg], &arg_array[firstArg]);

            /* Need to prep an error message before vfork(), just in case */
            fprintf(stderr, "couldn't exec \"%s\"", arg_array[firstArg]);
            _exit(127);
        }
#endif

        /* parent */

        /*
         * Enlarge the wait table if there isn't enough space for a new
         * entry.
         */
        if (table->used == table->size) {
            table->size += WAIT_TABLE_GROW_BY;
            table->info = Jim_Realloc(table->info, table->size * sizeof(*table->info));
        }

        table->info[table->used].pid = pid;
        table->info[table->used].flags = 0;
        table->used++;

        pidPtr[numPids] = pid;

        /* Restore in case of pipe_dup_err */
        errorId = origErrorId;

        /*
         * Close off our copies of file descriptors that were set up for
         * this child, then set up the input for the next child.
         */

        if (inputId != JIM_BAD_FD) {
            JimCloseFd(inputId);
        }
        if (outputId != JIM_BAD_FD) {
            JimCloseFd(outputId);
        }
        inputId = pipeIds[0];
        pipeIds[0] = pipeIds[1] = JIM_BAD_FD;
    }
    *pidArrayPtr = pidPtr;

    /*
     * All done.  Cleanup open files lying around and then return.
     */

  cleanup:
    if (inputId != JIM_BAD_FD) {
        JimCloseFd(inputId);
    }
    if (lastOutputId != JIM_BAD_FD) {
        JimCloseFd(lastOutputId);
    }
    if (errorId != JIM_BAD_FD) {
        JimCloseFd(errorId);
    }
    Jim_Free(arg_array);

    JimRestoreEnv(save_environ);

    return numPids;

    /*
     * An error occurred.  There could have been extra files open, such
     * as pipes between children.  Clean them all up.  Detach any child
     * processes that have been created.
     */

  error:
    if ((inPipePtr != NULL) && (*inPipePtr != JIM_BAD_FD)) {
        JimCloseFd(*inPipePtr);
        *inPipePtr = JIM_BAD_FD;
    }
    if ((outPipePtr != NULL) && (*outPipePtr != JIM_BAD_FD)) {
        JimCloseFd(*outPipePtr);
        *outPipePtr = JIM_BAD_FD;
    }
    if ((errFilePtr != NULL) && (*errFilePtr != JIM_BAD_FD)) {
        JimCloseFd(*errFilePtr);
        *errFilePtr = JIM_BAD_FD;
    }
    if (pipeIds[0] != JIM_BAD_FD) {
        JimCloseFd(pipeIds[0]);
    }
    if (pipeIds[1] != JIM_BAD_FD) {
        JimCloseFd(pipeIds[1]);
    }
    if (pidPtr != NULL) {
        for (i = 0; i < numPids; i++) {
            if (pidPtr[i] != JIM_BAD_PID) {
                JimDetachPids(interp, 1, &pidPtr[i]);
            }
        }
        Jim_Free(pidPtr);
    }
    numPids = -1;
    goto cleanup;
}

/*
 *----------------------------------------------------------------------
 *
 * JimCleanupChildren --
 *
 *  This is a utility procedure used to wait for child processes
 *  to exit, record information about abnormal exits, and then
 *  collect any stderr output generated by them.
 *
 * Results:
 *  The return value is a standard Tcl result.  If anything at
 *  weird happened with the child processes, JIM_ERROR is returned
 *  and a message is left in interp->result.
 *
 * Side effects:
 *  If the last character of interp->result is a newline, then it
 *  is removed.  File errorId gets closed, and pidPtr is freed
 *  back to the storage allocator.
 *
 *----------------------------------------------------------------------
 */

static int JimCleanupChildren(Jim_Interp *interp, int numPids, pidtype *pidPtr, fdtype errorId)
{
    struct WaitInfoTable *table = Jim_CmdPrivData(interp);
    int result = JIM_OK;
    int i;

    for (i = 0; i < numPids; i++) {
        int waitStatus = 0;
        if (JimWaitForProcess(table, pidPtr[i], &waitStatus) != JIM_BAD_PID) {
            if (JimCheckWaitStatus(interp, pidPtr[i], waitStatus) != JIM_OK) {
                result = JIM_ERR;
            }
        }
    }
    Jim_Free(pidPtr);

    /*
     * Read the standard error file.  If there's anything there,
     * then add the file's contents to the result
     * string.
     */
    if (errorId != JIM_BAD_FD) {
        JimRewindFd(errorId);
        if (JimAppendStreamToString(interp, errorId, Jim_GetResult(interp)) != JIM_OK) {
            result = JIM_ERR;
        }
    }

    JimTrimTrailingNewline(interp);

    return result;
}

int Jim_execInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "exec", "1.0", JIM_ERRMSG))
        return JIM_ERR;
    Jim_CreateCommand(interp, "exec", Jim_ExecCmd, JimAllocWaitInfoTable(), JimFreeWaitInfoTable);
    return JIM_OK;
}

#if defined(__MINGW32__)
/* Windows-specific (mingw) implementation */

static SECURITY_ATTRIBUTES *JimStdSecAttrs(void)
{
    static SECURITY_ATTRIBUTES secAtts;

    secAtts.nLength = sizeof(SECURITY_ATTRIBUTES);
    secAtts.lpSecurityDescriptor = NULL;
    secAtts.bInheritHandle = TRUE;
    return &secAtts;
}

static int JimErrno(void)
{
    switch (GetLastError()) {
    case ERROR_FILE_NOT_FOUND: return ENOENT;
    case ERROR_PATH_NOT_FOUND: return ENOENT;
    case ERROR_TOO_MANY_OPEN_FILES: return EMFILE;
    case ERROR_ACCESS_DENIED: return EACCES;
    case ERROR_INVALID_HANDLE: return EBADF;
    case ERROR_BAD_ENVIRONMENT: return E2BIG;
    case ERROR_BAD_FORMAT: return ENOEXEC;
    case ERROR_INVALID_ACCESS: return EACCES;
    case ERROR_INVALID_DRIVE: return ENOENT;
    case ERROR_CURRENT_DIRECTORY: return EACCES;
    case ERROR_NOT_SAME_DEVICE: return EXDEV;
    case ERROR_NO_MORE_FILES: return ENOENT;
    case ERROR_WRITE_PROTECT: return EROFS;
    case ERROR_BAD_UNIT: return ENXIO;
    case ERROR_NOT_READY: return EBUSY;
    case ERROR_BAD_COMMAND: return EIO;
    case ERROR_CRC: return EIO;
    case ERROR_BAD_LENGTH: return EIO;
    case ERROR_SEEK: return EIO;
    case ERROR_WRITE_FAULT: return EIO;
    case ERROR_READ_FAULT: return EIO;
    case ERROR_GEN_FAILURE: return EIO;
    case ERROR_SHARING_VIOLATION: return EACCES;
    case ERROR_LOCK_VIOLATION: return EACCES;
    case ERROR_SHARING_BUFFER_EXCEEDED: return ENFILE;
    case ERROR_HANDLE_DISK_FULL: return ENOSPC;
    case ERROR_NOT_SUPPORTED: return ENODEV;
    case ERROR_REM_NOT_LIST: return EBUSY;
    case ERROR_DUP_NAME: return EEXIST;
    case ERROR_BAD_NETPATH: return ENOENT;
    case ERROR_NETWORK_BUSY: return EBUSY;
    case ERROR_DEV_NOT_EXIST: return ENODEV;
    case ERROR_TOO_MANY_CMDS: return EAGAIN;
    case ERROR_ADAP_HDW_ERR: return EIO;
    case ERROR_BAD_NET_RESP: return EIO;
    case ERROR_UNEXP_NET_ERR: return EIO;
    case ERROR_NETNAME_DELETED: return ENOENT;
    case ERROR_NETWORK_ACCESS_DENIED: return EACCES;
    case ERROR_BAD_DEV_TYPE: return ENODEV;
    case ERROR_BAD_NET_NAME: return ENOENT;
    case ERROR_TOO_MANY_NAMES: return ENFILE;
    case ERROR_TOO_MANY_SESS: return EIO;
    case ERROR_SHARING_PAUSED: return EAGAIN;
    case ERROR_REDIR_PAUSED: return EAGAIN;
    case ERROR_FILE_EXISTS: return EEXIST;
    case ERROR_CANNOT_MAKE: return ENOSPC;
    case ERROR_OUT_OF_STRUCTURES: return ENFILE;
    case ERROR_ALREADY_ASSIGNED: return EEXIST;
    case ERROR_INVALID_PASSWORD: return EPERM;
    case ERROR_NET_WRITE_FAULT: return EIO;
    case ERROR_NO_PROC_SLOTS: return EAGAIN;
    case ERROR_DISK_CHANGE: return EXDEV;
    case ERROR_BROKEN_PIPE: return EPIPE;
    case ERROR_OPEN_FAILED: return ENOENT;
    case ERROR_DISK_FULL: return ENOSPC;
    case ERROR_NO_MORE_SEARCH_HANDLES: return EMFILE;
    case ERROR_INVALID_TARGET_HANDLE: return EBADF;
    case ERROR_INVALID_NAME: return ENOENT;
    case ERROR_PROC_NOT_FOUND: return ESRCH;
    case ERROR_WAIT_NO_CHILDREN: return ECHILD;
    case ERROR_CHILD_NOT_COMPLETE: return ECHILD;
    case ERROR_DIRECT_ACCESS_HANDLE: return EBADF;
    case ERROR_SEEK_ON_DEVICE: return ESPIPE;
    case ERROR_BUSY_DRIVE: return EAGAIN;
    case ERROR_DIR_NOT_EMPTY: return EEXIST;
    case ERROR_NOT_LOCKED: return EACCES;
    case ERROR_BAD_PATHNAME: return ENOENT;
    case ERROR_LOCK_FAILED: return EACCES;
    case ERROR_ALREADY_EXISTS: return EEXIST;
    case ERROR_FILENAME_EXCED_RANGE: return ENAMETOOLONG;
    case ERROR_BAD_PIPE: return EPIPE;
    case ERROR_PIPE_BUSY: return EAGAIN;
    case ERROR_PIPE_NOT_CONNECTED: return EPIPE;
    case ERROR_DIRECTORY: return ENOTDIR;
    }
    return EINVAL;
}

static int JimPipe(fdtype pipefd[2])
{
    if (CreatePipe(&pipefd[0], &pipefd[1], NULL, 0)) {
        return 0;
    }
    return -1;
}

static fdtype JimDupFd(fdtype infd)
{
    fdtype dupfd;
    pidtype pid = GetCurrentProcess();

    if (DuplicateHandle(pid, infd, pid, &dupfd, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
        return dupfd;
    }
    return JIM_BAD_FD;
}

static int JimRewindFd(fdtype fd)
{
    return SetFilePointer(fd, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER ? -1 : 0;
}

#if 0
static int JimReadFd(fdtype fd, char *buffer, size_t len)
{
    DWORD num;

    if (ReadFile(fd, buffer, len, &num, NULL)) {
        return num;
    }
    if (GetLastError() == ERROR_HANDLE_EOF || GetLastError() == ERROR_BROKEN_PIPE) {
        return 0;
    }
    return -1;
}
#endif

static FILE *JimFdOpenForRead(fdtype fd)
{
    return _fdopen(_open_osfhandle((int)fd, _O_RDONLY | _O_TEXT), "r");
}

static fdtype JimFileno(FILE *fh)
{
    return (fdtype)_get_osfhandle(_fileno(fh));
}

static fdtype JimOpenForRead(const char *filename)
{
    return CreateFile(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        JimStdSecAttrs(), OPEN_EXISTING, 0, NULL);
}

static fdtype JimOpenForWrite(const char *filename, int append)
{
    return CreateFile(filename, append ? FILE_APPEND_DATA : GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        JimStdSecAttrs(), append ? OPEN_ALWAYS : CREATE_ALWAYS, 0, (HANDLE) NULL);
}

static FILE *JimFdOpenForWrite(fdtype fd)
{
    return _fdopen(_open_osfhandle((int)fd, _O_TEXT), "w");
}

static pidtype JimWaitPid(pidtype pid, int *status, int nohang)
{
    DWORD ret = WaitForSingleObject(pid, nohang ? 0 : INFINITE);
    if (ret == WAIT_TIMEOUT || ret == WAIT_FAILED) {
        /* WAIT_TIMEOUT can only happend with WNOHANG */
        return JIM_BAD_PID;
    }
    GetExitCodeProcess(pid, &ret);
    *status = ret;
    CloseHandle(pid);
    return pid;
}

static HANDLE JimCreateTemp(Jim_Interp *interp, const char *contents)
{
    char name[MAX_PATH];
    HANDLE handle;

    if (!GetTempPath(MAX_PATH, name) || !GetTempFileName(name, "JIM", 0, name)) {
        return JIM_BAD_FD;
    }

    handle = CreateFile(name, GENERIC_READ | GENERIC_WRITE, 0, JimStdSecAttrs(),
            CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
            NULL);

    if (handle == INVALID_HANDLE_VALUE) {
        goto error;
    }

    if (contents != NULL) {
        /* Use fdopen() to get automatic text-mode translation */
        FILE *fh = JimFdOpenForWrite(JimDupFd(handle));
        if (fh == NULL) {
            goto error;
        }

        if (fwrite(contents, strlen(contents), 1, fh) != 1) {
            fclose(fh);
            goto error;
        }
        fseek(fh, 0, SEEK_SET);
        fclose(fh);
    }
    return handle;

  error:
    Jim_SetResultErrno(interp, "failed to create temp file");
    CloseHandle(handle);
    DeleteFile(name);
    return JIM_BAD_FD;
}

static int
JimWinFindExecutable(const char *originalName, char fullPath[MAX_PATH])
{
    int i;
    static char extensions[][5] = {".exe", "", ".bat"};

    for (i = 0; i < (int) (sizeof(extensions) / sizeof(extensions[0])); i++) {
        lstrcpyn(fullPath, originalName, MAX_PATH - 5);
        lstrcat(fullPath, extensions[i]);

        if (SearchPath(NULL, fullPath, NULL, MAX_PATH, fullPath, NULL) == 0) {
            continue;
        }
        if (GetFileAttributes(fullPath) & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        return 0;
    }

    return -1;
}

static char **JimSaveEnv(char **env)
{
    return env;
}

static void JimRestoreEnv(char **env)
{
    JimFreeEnv(env, NULL);
}

static Jim_Obj *
JimWinBuildCommandLine(Jim_Interp *interp, char **argv)
{
    char *start, *special;
    int quote, i;

    Jim_Obj *strObj = Jim_NewStringObj(interp, "", 0);

    for (i = 0; argv[i]; i++) {
        if (i > 0) {
            Jim_AppendString(interp, strObj, " ", 1);
        }

        if (argv[i][0] == '\0') {
            quote = 1;
        }
        else {
            quote = 0;
            for (start = argv[i]; *start != '\0'; start++) {
                if (isspace(UCHAR(*start))) {
                    quote = 1;
                    break;
                }
            }
        }
        if (quote) {
            Jim_AppendString(interp, strObj, "\"" , 1);
        }

        start = argv[i];
        for (special = argv[i]; ; ) {
            if ((*special == '\\') && (special[1] == '\\' ||
                    special[1] == '"' || (quote && special[1] == '\0'))) {
                Jim_AppendString(interp, strObj, start, special - start);
                start = special;
                while (1) {
                    special++;
                    if (*special == '"' || (quote && *special == '\0')) {
                        /*
                         * N backslashes followed a quote -> insert
                         * N * 2 + 1 backslashes then a quote.
                         */

                        Jim_AppendString(interp, strObj, start, special - start);
                        break;
                    }
                    if (*special != '\\') {
                        break;
                    }
                }
                Jim_AppendString(interp, strObj, start, special - start);
                start = special;
            }
            if (*special == '"') {
        if (special == start) {
            Jim_AppendString(interp, strObj, "\"", 1);
        }
        else {
            Jim_AppendString(interp, strObj, start, special - start);
        }
                Jim_AppendString(interp, strObj, "\\\"", 2);
                start = special + 1;
            }
            if (*special == '\0') {
                break;
            }
            special++;
        }
        Jim_AppendString(interp, strObj, start, special - start);
        if (quote) {
            Jim_AppendString(interp, strObj, "\"", 1);
        }
    }
    return strObj;
}

static pidtype
JimStartWinProcess(Jim_Interp *interp, char **argv, char *env, fdtype inputId, fdtype outputId, fdtype errorId)
{
    STARTUPINFO startInfo;
    PROCESS_INFORMATION procInfo;
    HANDLE hProcess, h;
    char execPath[MAX_PATH];
    char *originalName;
    pidtype pid = JIM_BAD_PID;
    Jim_Obj *cmdLineObj;

    if (JimWinFindExecutable(argv[0], execPath) < 0) {
        return JIM_BAD_PID;
    }
    originalName = argv[0];
    argv[0] = execPath;

    hProcess = GetCurrentProcess();
    cmdLineObj = JimWinBuildCommandLine(interp, argv);

    /*
     * STARTF_USESTDHANDLES must be used to pass handles to child process.
     * Using SetStdHandle() and/or dup2() only works when a console mode
     * parent process is spawning an attached console mode child process.
     */

    ZeroMemory(&startInfo, sizeof(startInfo));
    startInfo.cb = sizeof(startInfo);
    startInfo.dwFlags   = STARTF_USESTDHANDLES;
    startInfo.hStdInput	= INVALID_HANDLE_VALUE;
    startInfo.hStdOutput= INVALID_HANDLE_VALUE;
    startInfo.hStdError = INVALID_HANDLE_VALUE;

    /*
     * Duplicate all the handles which will be passed off as stdin, stdout
     * and stderr of the child process. The duplicate handles are set to
     * be inheritable, so the child process can use them.
     */
    if (inputId == JIM_BAD_FD) {
        if (CreatePipe(&startInfo.hStdInput, &h, JimStdSecAttrs(), 0) != FALSE) {
            CloseHandle(h);
        }
    } else {
        DuplicateHandle(hProcess, inputId, hProcess, &startInfo.hStdInput,
                0, TRUE, DUPLICATE_SAME_ACCESS);
    }
    if (startInfo.hStdInput == JIM_BAD_FD) {
        goto end;
    }

    if (outputId == JIM_BAD_FD) {
        startInfo.hStdOutput = CreateFile("NUL:", GENERIC_WRITE, 0,
                JimStdSecAttrs(), OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    } else {
        DuplicateHandle(hProcess, outputId, hProcess, &startInfo.hStdOutput,
                0, TRUE, DUPLICATE_SAME_ACCESS);
    }
    if (startInfo.hStdOutput == JIM_BAD_FD) {
        goto end;
    }

    if (errorId == JIM_BAD_FD) {
        /*
         * If handle was not set, errors should be sent to an infinitely
         * deep sink.
         */

        startInfo.hStdError = CreateFile("NUL:", GENERIC_WRITE, 0,
                JimStdSecAttrs(), OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    } else {
        DuplicateHandle(hProcess, errorId, hProcess, &startInfo.hStdError,
                0, TRUE, DUPLICATE_SAME_ACCESS);
    }
    if (startInfo.hStdError == JIM_BAD_FD) {
        goto end;
    }

    if (!CreateProcess(NULL, (char *)Jim_String(cmdLineObj), NULL, NULL, TRUE,
            0, env, NULL, &startInfo, &procInfo)) {
        goto end;
    }

    /*
     * "When an application spawns a process repeatedly, a new thread
     * instance will be created for each process but the previous
     * instances may not be cleaned up.  This results in a significant
     * virtual memory loss each time the process is spawned.  If there
     * is a WaitForInputIdle() call between CreateProcess() and
     * CloseHandle(), the problem does not occur." PSS ID Number: Q124121
     */

    WaitForInputIdle(procInfo.hProcess, 5000);
    CloseHandle(procInfo.hThread);

    pid = procInfo.hProcess;

    end:
    Jim_FreeNewObj(interp, cmdLineObj);
    if (startInfo.hStdInput != JIM_BAD_FD) {
        CloseHandle(startInfo.hStdInput);
    }
    if (startInfo.hStdOutput != JIM_BAD_FD) {
        CloseHandle(startInfo.hStdOutput);
    }
    if (startInfo.hStdError != JIM_BAD_FD) {
        CloseHandle(startInfo.hStdError);
    }
    return pid;
}
#else
/* Unix-specific implementation */
static int JimOpenForWrite(const char *filename, int append)
{
    return open(filename, O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0666);
}

static int JimRewindFd(int fd)
{
    return lseek(fd, 0L, SEEK_SET);
}

static int JimCreateTemp(Jim_Interp *interp, const char *contents)
{
    char inName[] = "/tmp/tcl.tmp.XXXXXX";

    int fd = mkstemp(inName);
    if (fd == JIM_BAD_FD) {
        Jim_SetResultErrno(interp, "couldn't create temp file");
        return -1;
    }
    unlink(inName);
    if (contents) {
        int length = strlen(contents);
        if (write(fd, contents, length) != length) {
            Jim_SetResultErrno(interp, "couldn't write temp file");
            close(fd);
            return -1;
        }
        lseek(fd, 0L, SEEK_SET);
    }
    return fd;
}

static char **JimSaveEnv(char **env)
{
    char **saveenv = Jim_GetEnviron();
    Jim_SetEnviron(env);
    return saveenv;
}

static void JimRestoreEnv(char **env)
{
    JimFreeEnv(Jim_GetEnviron(), env);
    Jim_SetEnviron(env);
}
#endif
#endif
