
/* Jim - POSIX extension
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * A copy of the license is also included in the source distribution
 * of Jim, as a TXT file name called LICENSE.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include "jim.h"
#include "jimautoconf.h"

#ifdef HAVE_SYS_SYSINFO_H
#include <sys/sysinfo.h>
#endif

static void Jim_PosixSetError(Jim_Interp *interp)
{
    Jim_SetResultString(interp, strerror(errno), -1);
}

#if defined(HAVE_FORK)
static int Jim_PosixForkCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    pid_t pid;

    JIM_NOTUSED(argv);

    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }
    if ((pid = fork()) == -1) {
        Jim_PosixSetError(interp);
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, (jim_wide) pid);
    return JIM_OK;
}
#endif

/*
 * os.wait ?-nohang? pid
 *
 * An interface to waitpid(2)
 *
 * Returns a 3 element list.
 *
 * If -nohang is specified, and the process is still alive, returns
 *
 *   {0 none 0}
 *
 * If the process does not exist or has already been waited for, returns:
 *
 *   {-1 error <error-description>}
 *
 * If the process exited normally, returns:
 *
 *   {<pid> exit <exit-status>}
 *
 * If the process terminated on a signal, returns:
 *
 *   {<pid> signal <signal-number>}
 *
 * Otherwise (core dump, stopped, continued, ...), returns:
 *
 *   {<pid> other 0}
 */
static int Jim_PosixWaitCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int nohang = 0;
    long pid;
    int status;
    Jim_Obj *listObj;
    const char *type;
    int value;

    if (argc > 1 && Jim_CompareStringImmediate(interp, argv[1], "-nohang")) {
        nohang = 1;
    }
    if (argc != nohang + 2) {
        Jim_WrongNumArgs(interp, 1, argv, "?-nohang? pid");
        return JIM_ERR;
    }
    if (Jim_GetLong(interp, argv[nohang + 1], &pid) != JIM_OK) {
        return JIM_ERR;
    }

    pid = waitpid(pid, &status, nohang ? WNOHANG : 0);
    listObj = Jim_NewListObj(interp, NULL, 0);
    Jim_ListAppendElement(interp, listObj, Jim_NewIntObj(interp, pid));
    if (pid < 0) {
        type = "error";
        value = errno;
    }
    else if (pid == 0) {
        type = "none";
        value = 0;
    }
    else if (WIFEXITED(status)) {
        type = "exit";
        value = WEXITSTATUS(status);
    }
    else if (WIFSIGNALED(status)) {
        type = "signal";
        value = WTERMSIG(status);
    }
    else {
        type = "other";
        value = 0;
    }

    Jim_ListAppendElement(interp, listObj, Jim_NewStringObj(interp, type, -1));
    if (pid < 0) {
        Jim_ListAppendElement(interp, listObj, Jim_NewStringObj(interp, strerror(value), -1));
    }
    else {
        Jim_ListAppendElement(interp, listObj, Jim_NewIntObj(interp, value));
    }
    Jim_SetResult(interp, listObj);
    return JIM_OK;
}

static int Jim_PosixGetidsCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objv[8];

    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }
    objv[0] = Jim_NewStringObj(interp, "uid", -1);
    objv[1] = Jim_NewIntObj(interp, getuid());
    objv[2] = Jim_NewStringObj(interp, "euid", -1);
    objv[3] = Jim_NewIntObj(interp, geteuid());
    objv[4] = Jim_NewStringObj(interp, "gid", -1);
    objv[5] = Jim_NewIntObj(interp, getgid());
    objv[6] = Jim_NewStringObj(interp, "egid", -1);
    objv[7] = Jim_NewIntObj(interp, getegid());
    Jim_SetResult(interp, Jim_NewListObj(interp, objv, 8));
    return JIM_OK;
}

#define JIM_HOST_NAME_MAX 1024
static int Jim_PosixGethostnameCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    char *buf;
    int rc = JIM_OK;

    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }
    buf = Jim_Alloc(JIM_HOST_NAME_MAX);
    if (gethostname(buf, JIM_HOST_NAME_MAX) == -1) {
        Jim_PosixSetError(interp);
        rc = JIM_ERR;
    }
    else {
        Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, buf, -1));
    }
    return rc;
}

static int Jim_PosixUptimeCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
#ifdef HAVE_STRUCT_SYSINFO_UPTIME
    struct sysinfo info;

    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }

    if (sysinfo(&info) == -1) {
        Jim_PosixSetError(interp);
        return JIM_ERR;
    }

    Jim_SetResultInt(interp, info.uptime);
#else
    Jim_SetResultInt(interp, (long)time(NULL));
#endif
    return JIM_OK;
}

static int Jim_PosixPidCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }

    Jim_SetResultInt(interp, getpid());
    return JIM_OK;
}

int Jim_posixInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "posix", "1.0", JIM_ERRMSG))
        return JIM_ERR;

#ifdef HAVE_FORK
    Jim_CreateCommand(interp, "os.fork", Jim_PosixForkCommand, NULL, NULL);
#endif
    Jim_CreateCommand(interp, "os.wait", Jim_PosixWaitCommand, NULL, NULL);
    Jim_CreateCommand(interp, "os.getids", Jim_PosixGetidsCommand, NULL, NULL);
    Jim_CreateCommand(interp, "os.gethostname", Jim_PosixGethostnameCommand, NULL, NULL);
    Jim_CreateCommand(interp, "os.uptime", Jim_PosixUptimeCommand, NULL, NULL);
    Jim_CreateCommand(interp, "pid", Jim_PosixPidCommand, NULL, NULL);
    return JIM_OK;
}
