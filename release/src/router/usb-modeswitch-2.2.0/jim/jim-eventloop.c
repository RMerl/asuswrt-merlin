
/* Jim - A small embeddable Tcl interpreter
 *
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 * Copyright 2005 Clemens Hintze <c.hintze@gmx.net>
 * Copyright 2005 patthoyts - Pat Thoyts <patthoyts@users.sf.net>
 * Copyright 2008 oharboe - Øyvind Harboe - oyvind.harboe@zylin.com
 * Copyright 2008 Andrew Lunn <andrew@lunn.ch>
 * Copyright 2008 Duane Ellis <openocd@duaneellis.com>
 * Copyright 2008 Uwe Klein <uklein@klein-messgeraete.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE JIM TCL PROJECT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * JIM TCL PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the Jim Tcl Project.
 **/

#include "jim.h"
#include "jimautoconf.h"
#include "jim-eventloop.h"

/* POSIX includes */
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#if defined(__MINGW32__)
#include <windows.h>
#include <winsock.h>
#define msleep Sleep
#ifndef HAVE_USLEEP
#define usleep(US) msleep((US) / 1000)
#endif
#else
#include <sys/select.h>

#ifndef HAVE_USLEEP
/* XXX: Implement this in terms of select() or nanosleep() */
#define usleep(US)
#endif
#define msleep(MS) sleep((MS) / 1000); usleep(((MS) % 1000) * 1000);
#endif

/* --- */

/* File event structure */
typedef struct Jim_FileEvent
{
    FILE *handle;
    int mask;                   /* one of JIM_EVENT_(READABLE|WRITABLE|EXCEPTION) */
    Jim_FileProc *fileProc;
    Jim_EventFinalizerProc *finalizerProc;
    void *clientData;
    struct Jim_FileEvent *next;
} Jim_FileEvent;

/* Time event structure */
typedef struct Jim_TimeEvent
{
    jim_wide id;                /* time event identifier. */
    int mode;                   /* restart, repetitive .. UK */
    long initialms;             /* initial relativ timer value UK */
    long when_sec;              /* seconds */
    long when_ms;               /* milliseconds */
    Jim_TimeProc *timeProc;
    Jim_EventFinalizerProc *finalizerProc;
    void *clientData;
    struct Jim_TimeEvent *next;
} Jim_TimeEvent;

/* Per-interp stucture containing the state of the event loop */
typedef struct Jim_EventLoop
{
    jim_wide timeEventNextId;
    Jim_FileEvent *fileEventHead;
    Jim_TimeEvent *timeEventHead;
    int suppress_bgerror; /* bgerror returned break, so don't call it again */
} Jim_EventLoop;

static void JimAfterTimeHandler(Jim_Interp *interp, void *clientData);
static void JimAfterTimeEventFinalizer(Jim_Interp *interp, void *clientData);

int Jim_EvalObjBackground(Jim_Interp *interp, Jim_Obj *scriptObjPtr)
{
    Jim_EventLoop *eventLoop = Jim_GetAssocData(interp, "eventloop");
    Jim_CallFrame *savedFramePtr;
    int retval;

    savedFramePtr = interp->framePtr;
    interp->framePtr = interp->topFramePtr;
    retval = Jim_EvalObj(interp, scriptObjPtr);
    interp->framePtr = savedFramePtr;
    /* Try to report the error (if any) via the bgerror proc */
    if (retval != JIM_OK && !eventLoop->suppress_bgerror) {
        Jim_Obj *objv[2];
        int rc = JIM_ERR;

        objv[0] = Jim_NewStringObj(interp, "bgerror", -1);
        objv[1] = Jim_GetResult(interp);
        Jim_IncrRefCount(objv[0]);
        Jim_IncrRefCount(objv[1]);
        if (Jim_GetCommand(interp, objv[0], JIM_NONE) == NULL || (rc = Jim_EvalObjVector(interp, 2, objv)) != JIM_OK) {
            if (rc == JIM_BREAK) {
                /* No more bgerror calls */
                eventLoop->suppress_bgerror++;
            }
            else {
                /* Report the error to stderr. */
                Jim_MakeErrorMessage(interp);
                fprintf(stderr, "%s\n", Jim_String(Jim_GetResult(interp)));
                /* And reset the result */
                Jim_SetResultString(interp, "", -1);
            }
        }
        Jim_DecrRefCount(interp, objv[0]);
        Jim_DecrRefCount(interp, objv[1]);
    }
    return retval;
}


void Jim_CreateFileHandler(Jim_Interp *interp, FILE * handle, int mask,
    Jim_FileProc * proc, void *clientData, Jim_EventFinalizerProc * finalizerProc)
{
    Jim_FileEvent *fe;
    Jim_EventLoop *eventLoop = Jim_GetAssocData(interp, "eventloop");

    fe = Jim_Alloc(sizeof(*fe));
    fe->handle = handle;
    fe->mask = mask;
    fe->fileProc = proc;
    fe->finalizerProc = finalizerProc;
    fe->clientData = clientData;
    fe->next = eventLoop->fileEventHead;
    eventLoop->fileEventHead = fe;
}

void Jim_DeleteFileHandler(Jim_Interp *interp, FILE * handle)
{
    Jim_FileEvent *fe, *prev = NULL;
    Jim_EventLoop *eventLoop = Jim_GetAssocData(interp, "eventloop");

    fe = eventLoop->fileEventHead;
    while (fe) {
        if (fe->handle == handle) {
            if (prev == NULL)
                eventLoop->fileEventHead = fe->next;
            else
                prev->next = fe->next;
            if (fe->finalizerProc)
                fe->finalizerProc(interp, fe->clientData);
            Jim_Free(fe);
            return;
        }
        prev = fe;
        fe = fe->next;
    }
}

static void JimGetTime(long *seconds, long *milliseconds)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    *seconds = tv.tv_sec;
    *milliseconds = tv.tv_usec / 1000;
}

jim_wide Jim_CreateTimeHandler(Jim_Interp *interp, jim_wide milliseconds,
    Jim_TimeProc * proc, void *clientData, Jim_EventFinalizerProc * finalizerProc)
{
    Jim_EventLoop *eventLoop = Jim_GetAssocData(interp, "eventloop");
    jim_wide id = eventLoop->timeEventNextId++;
    Jim_TimeEvent *te, *e, *prev;
    long cur_sec, cur_ms;

    JimGetTime(&cur_sec, &cur_ms);

    te = Jim_Alloc(sizeof(*te));
    te->id = id;
    te->mode = 0;
    te->initialms = milliseconds;
    te->when_sec = cur_sec + milliseconds / 1000;
    te->when_ms = cur_ms + milliseconds % 1000;
    if (te->when_ms >= 1000) {
        te->when_sec++;
        te->when_ms -= 1000;
    }
    te->timeProc = proc;
    te->finalizerProc = finalizerProc;
    te->clientData = clientData;

    /* Add to the appropriate place in the list */
    if (eventLoop->timeEventHead) {
        prev = NULL;
        for (e = eventLoop->timeEventHead; e; e = e->next) {
            if (te->when_sec < e->when_sec || (te->when_sec == e->when_sec && te->when_ms < e->when_ms)) {
                break;
            }
            prev = e;
        }
        if (prev) {
            te->next = prev->next;
            prev->next = te;
            return id;
        }
    }

    te->next = eventLoop->timeEventHead;
    eventLoop->timeEventHead = te;

    return id;
}

static jim_wide JimParseAfterId(Jim_Obj *idObj)
{
    int len;
    const char *tok = Jim_GetString(idObj, &len);
    jim_wide id;

    if (strncmp(tok, "after#", 6) == 0 && Jim_StringToWide(tok + 6, &id, 10) == JIM_OK) {
        /* Got an event by id */
        return id;
    }
    return -1;
}

static jim_wide JimFindAfterByScript(Jim_EventLoop *eventLoop, Jim_Obj *scriptObj)
{
    Jim_TimeEvent *te;

    for (te = eventLoop->timeEventHead; te; te = te->next) {
        /* Is this an 'after' event? */
        if (te->timeProc == JimAfterTimeHandler) {
            if (Jim_StringEqObj(scriptObj, te->clientData)) {
                return te->id;
            }
        }
    }
    return -1;                  /* NO event with the specified ID found */
}

static Jim_TimeEvent *JimFindTimeHandlerById(Jim_EventLoop *eventLoop, jim_wide id)
{
    Jim_TimeEvent *te;

    for (te = eventLoop->timeEventHead; te; te = te->next) {
        if (te->id == id) {
            return te;
        }
    }
    return NULL;
}

static Jim_TimeEvent *Jim_RemoveTimeHandler(Jim_EventLoop *eventLoop, jim_wide id)
{
    Jim_TimeEvent *te, *prev = NULL;

    for (te = eventLoop->timeEventHead; te; te = te->next) {
        if (te->id == id) {
            if (prev == NULL)
                eventLoop->timeEventHead = te->next;
            else
                prev->next = te->next;
            return te;
        }
        prev = te;
    }
    return NULL;
}

static void Jim_FreeTimeHandler(Jim_Interp *interp, Jim_TimeEvent *te)
{
    if (te->finalizerProc)
        te->finalizerProc(interp, te->clientData);
    Jim_Free(te);
}

jim_wide Jim_DeleteTimeHandler(Jim_Interp *interp, jim_wide id)
{
    Jim_TimeEvent *te;
    Jim_EventLoop *eventLoop = Jim_GetAssocData(interp, "eventloop");

    if (id >= eventLoop->timeEventNextId) {
        return -2;              /* wrong event ID */
    }

    te = Jim_RemoveTimeHandler(eventLoop, id);
    if (te) {
        jim_wide remain;
        long cur_sec, cur_ms;

        JimGetTime(&cur_sec, &cur_ms);

        remain = (te->when_sec - cur_sec) * 1000;
        remain += (te->when_ms - cur_ms);
        remain = (remain < 0) ? 0 : remain;

        Jim_FreeTimeHandler(interp, te);
        return remain;
    }
    return -1;                  /* NO event with the specified ID found */
}

/* --- POSIX version of Jim_ProcessEvents, for now the only available --- */

/* Process every pending time event, then every pending file event
 * (that may be registered by time event callbacks just processed).
 * Without special flags the function sleeps until some file event
 * fires, or when the next time event occurrs (if any).
 *
 * If flags is 0, the function does nothing and returns.
 * if flags has JIM_ALL_EVENTS set, all the kind of events are processed.
 * if flags has JIM_FILE_EVENTS set, file events are processed.
 * if flags has JIM_TIME_EVENTS set, time events are processed.
 * if flags has JIM_DONT_WAIT set the function returns ASAP until all
 * the events that's possible to process without to wait are processed.
 *
 * The function returns the number of events processed or -1 if
 * there are no matching handlers, or -2 on error.
 */
int Jim_ProcessEvents(Jim_Interp *interp, int flags)
{
    jim_wide sleep_ms = -1;
    int processed = 0;
    Jim_EventLoop *eventLoop = Jim_GetAssocData(interp, "eventloop");
    Jim_FileEvent *fe = eventLoop->fileEventHead;
    Jim_TimeEvent *te;
    jim_wide maxId;

    if ((flags & JIM_FILE_EVENTS) == 0 || fe == NULL) {
        /* No file events */
        if ((flags & JIM_TIME_EVENTS) == 0 || eventLoop->timeEventHead == NULL) {
            /* No time events */
            return -1;
        }
    }

    /* Note that we want call select() even if there are no
     * file events to process as long as we want to process time
     * events, in order to sleep until the next time event is ready
     * to fire. */

    if (flags & JIM_DONT_WAIT) {
        /* Wait no time */
        sleep_ms = 0;
    }
    else if (flags & JIM_TIME_EVENTS) {
        /* The nearest timer is always at the head of the list */
        if (eventLoop->timeEventHead) {
            Jim_TimeEvent *shortest = eventLoop->timeEventHead;
            long now_sec, now_ms;

            /* Calculate the time missing for the nearest
             * timer to fire. */
            JimGetTime(&now_sec, &now_ms);
            sleep_ms = 1000 * (shortest->when_sec - now_sec) + (shortest->when_ms - now_ms);
            if (sleep_ms < 0) {
                sleep_ms = 1;
            }
        }
        else {
            /* Wait forever */
            sleep_ms = -1;
        }
    }

#ifdef HAVE_SELECT
    if (flags & JIM_FILE_EVENTS) {
        int retval;
        struct timeval tv, *tvp = NULL;
        fd_set rfds, wfds, efds;
        int maxfd = -1;

        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_ZERO(&efds);

        /* Check file events */
        while (fe != NULL) {
            int fd = fileno(fe->handle);

            if (fe->mask & JIM_EVENT_READABLE)
                FD_SET(fd, &rfds);
            if (fe->mask & JIM_EVENT_WRITABLE)
                FD_SET(fd, &wfds);
            if (fe->mask & JIM_EVENT_EXCEPTION)
                FD_SET(fd, &efds);
            if (maxfd < fd)
                maxfd = fd;
            fe = fe->next;
        }

        if (sleep_ms >= 0) {
            tvp = &tv;
            tvp->tv_sec = sleep_ms / 1000;
            tvp->tv_usec = 1000 * (sleep_ms % 1000);
        }

        retval = select(maxfd + 1, &rfds, &wfds, &efds, tvp);

        if (retval < 0) {
            if (errno == EINVAL) {
                /* This can happen on mingw32 if a non-socket filehandle is passed */
                Jim_SetResultString(interp, "non-waitable filehandle", -1);
                return -2;
            }
            /* XXX: What about EINTR? */
        }
        else if (retval > 0) {
            fe = eventLoop->fileEventHead;
            while (fe != NULL) {
                int fd = fileno(fe->handle);
                int mask = 0;

                if ((fe->mask & JIM_EVENT_READABLE) && FD_ISSET(fd, &rfds))
                    mask |= JIM_EVENT_READABLE;
                if (fe->mask & JIM_EVENT_WRITABLE && FD_ISSET(fd, &wfds))
                    mask |= JIM_EVENT_WRITABLE;
                if (fe->mask & JIM_EVENT_EXCEPTION && FD_ISSET(fd, &efds))
                    mask |= JIM_EVENT_EXCEPTION;

                if (mask) {
                    if (fe->fileProc(interp, fe->clientData, mask) != JIM_OK) {
                        /* Remove the element on handler error */
                        Jim_DeleteFileHandler(interp, fe->handle);
                    }
                    processed++;
                    /* After an event is processed our file event list
                     * may no longer be the same, so what we do
                     * is to clear the bit for this file descriptor and
                     * restart again from the head. */
                    fe = eventLoop->fileEventHead;
                    FD_CLR(fd, &rfds);
                    FD_CLR(fd, &wfds);
                    FD_CLR(fd, &efds);
                }
                else {
                    fe = fe->next;
                }
            }
        }
    }
#else
    if (sleep_ms > 0) {
        msleep(sleep_ms);
    }
#endif

    /* Check time events */
    te = eventLoop->timeEventHead;
    maxId = eventLoop->timeEventNextId - 1;
    while (te) {
        long now_sec, now_ms;
        jim_wide id;

        if (te->id > maxId) {
            te = te->next;
            continue;
        }
        JimGetTime(&now_sec, &now_ms);
        if (now_sec > te->when_sec || (now_sec == te->when_sec && now_ms >= te->when_ms)) {
            id = te->id;
            /* Remove from the list before executing */
            Jim_RemoveTimeHandler(eventLoop, id);
            te->timeProc(interp, te->clientData);
            /* After an event is processed our time event list may
             * no longer be the same, so we restart from head.
             * Still we make sure to don't process events registered
             * by event handlers itself in order to don't loop forever
             * even in case an [after 0] that continuously register
             * itself. To do so we saved the max ID we want to handle. */
            Jim_FreeTimeHandler(interp, te);

            te = eventLoop->timeEventHead;
            processed++;
        }
        else {
            te = te->next;
        }
    }

    return processed;
}

/* ---------------------------------------------------------------------- */

static void JimELAssocDataDeleProc(Jim_Interp *interp, void *data)
{
    void *next;
    Jim_FileEvent *fe;
    Jim_TimeEvent *te;
    Jim_EventLoop *eventLoop = data;

    fe = eventLoop->fileEventHead;
    while (fe) {
        next = fe->next;
        if (fe->finalizerProc)
            fe->finalizerProc(interp, fe->clientData);
        Jim_Free(fe);
        fe = next;
    }

    te = eventLoop->timeEventHead;
    while (te) {
        next = te->next;
        if (te->finalizerProc)
            te->finalizerProc(interp, te->clientData);
        Jim_Free(te);
        te = next;
    }
    Jim_Free(data);
}

static int JimELVwaitCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_EventLoop *eventLoop = Jim_CmdPrivData(interp);
    Jim_Obj *oldValue;
    int rc;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "name");
        return JIM_ERR;
    }

    oldValue = Jim_GetGlobalVariable(interp, argv[1], JIM_NONE);
    if (oldValue) {
        Jim_IncrRefCount(oldValue);
    }
    else {
        /* If a result was left, it is an error */
        int len;
        Jim_GetString(interp->result, &len);
        if (len) {
            return JIM_ERR;
        }
    }

    eventLoop->suppress_bgerror = 0;

    while ((rc = Jim_ProcessEvents(interp, JIM_ALL_EVENTS)) >= 0) {
        Jim_Obj *currValue;
        currValue = Jim_GetGlobalVariable(interp, argv[1], JIM_NONE);
        /* Stop the loop if the vwait-ed variable changed value,
         * or if was unset and now is set (or the contrary). */
        if ((oldValue && !currValue) ||
            (!oldValue && currValue) ||
            (oldValue && currValue && !Jim_StringEqObj(oldValue, currValue)))
            break;
    }
    if (oldValue)
        Jim_DecrRefCount(interp, oldValue);


    if (rc == -2) {
        return JIM_ERR;
    }

    Jim_SetEmptyResult(interp);
    return JIM_OK;
}

static int JimELUpdateCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_EventLoop *eventLoop = Jim_CmdPrivData(interp);
    static const char * const options[] = {
        "idletasks", NULL
    };
    enum { UPDATE_IDLE, UPDATE_NONE };
    int option = UPDATE_NONE;
    int flags = JIM_TIME_EVENTS;

    if (argc == 1) {
        flags = JIM_ALL_EVENTS;
    }
    else if (argc > 2 || Jim_GetEnum(interp, argv[1], options, &option, NULL, JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
        Jim_WrongNumArgs(interp, 1, argv, "?idletasks?");
        return JIM_ERR;
    }

    eventLoop->suppress_bgerror = 0;

    while (Jim_ProcessEvents(interp, flags | JIM_DONT_WAIT) > 0) {
    }

    return JIM_OK;
}

static void JimAfterTimeHandler(Jim_Interp *interp, void *clientData)
{
    Jim_Obj *objPtr = clientData;

    Jim_EvalObjBackground(interp, objPtr);
}

static void JimAfterTimeEventFinalizer(Jim_Interp *interp, void *clientData)
{
    Jim_Obj *objPtr = clientData;

    Jim_DecrRefCount(interp, objPtr);
}

static int JimELAfterCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_EventLoop *eventLoop = Jim_CmdPrivData(interp);
    jim_wide ms = 0, id;
    Jim_Obj *objPtr, *idObjPtr;
    static const char * const options[] = {
        "cancel", "info", "idle", NULL
    };
    enum
    { AFTER_CANCEL, AFTER_INFO, AFTER_IDLE, AFTER_RESTART, AFTER_EXPIRE, AFTER_CREATE };
    int option = AFTER_CREATE;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "option ?arg ...?");
        return JIM_ERR;
    }
    if (Jim_GetWide(interp, argv[1], &ms) != JIM_OK) {
        if (Jim_GetEnum(interp, argv[1], options, &option, "argument", JIM_ERRMSG) != JIM_OK) {
            return JIM_ERR;
        }
        Jim_SetEmptyResult(interp);
    }
    else if (argc == 2) {
        /* Simply a sleep */
        msleep(ms);
        return JIM_OK;
    }

    switch (option) {
        case AFTER_IDLE:
            if (argc < 3) {
                Jim_WrongNumArgs(interp, 2, argv, "script ?script ...?");
                return JIM_ERR;
            }
            /* fall through */
        case AFTER_CREATE: {
            Jim_Obj *scriptObj = Jim_ConcatObj(interp, argc - 2, argv + 2);
            Jim_IncrRefCount(scriptObj);
            id = Jim_CreateTimeHandler(interp, ms, JimAfterTimeHandler, scriptObj,
                JimAfterTimeEventFinalizer);
            objPtr = Jim_NewStringObj(interp, NULL, 0);
            Jim_AppendString(interp, objPtr, "after#", -1);
            idObjPtr = Jim_NewIntObj(interp, id);
            Jim_IncrRefCount(idObjPtr);
            Jim_AppendObj(interp, objPtr, idObjPtr);
            Jim_DecrRefCount(interp, idObjPtr);
            Jim_SetResult(interp, objPtr);
            return JIM_OK;
        }
        case AFTER_CANCEL:
            if (argc < 3) {
                Jim_WrongNumArgs(interp, 2, argv, "id|command");
                return JIM_ERR;
            }
            else {
                jim_wide remain = 0;

                id = JimParseAfterId(argv[2]);
                if (id < 0) {
                    /* Not an event id, so search by script */
                    Jim_Obj *scriptObj = Jim_ConcatObj(interp, argc - 2, argv + 2);
                    id = JimFindAfterByScript(eventLoop, scriptObj);
                    Jim_FreeNewObj(interp, scriptObj);
                    if (id < 0) {
                        /* Not found */
                        break;
                    }
                }
                remain = Jim_DeleteTimeHandler(interp, id);
                if (remain >= 0) {
                    Jim_SetResultInt(interp, remain);
                }
            }
            break;

        case AFTER_INFO:
            if (argc == 2) {
                Jim_TimeEvent *te = eventLoop->timeEventHead;
                Jim_Obj *listObj = Jim_NewListObj(interp, NULL, 0);
                char buf[30];
                const char *fmt = "after#%" JIM_WIDE_MODIFIER;

                while (te) {
                    snprintf(buf, sizeof(buf), fmt, te->id);
                    Jim_ListAppendElement(interp, listObj, Jim_NewStringObj(interp, buf, -1));
                    te = te->next;
                }
                Jim_SetResult(interp, listObj);
            }
            else if (argc == 3) {
                id = JimParseAfterId(argv[2]);
                if (id >= 0) {
                    Jim_TimeEvent *e = JimFindTimeHandlerById(eventLoop, id);
                    if (e && e->timeProc == JimAfterTimeHandler) {
                        Jim_Obj *listObj = Jim_NewListObj(interp, NULL, 0);
                        Jim_ListAppendElement(interp, listObj, e->clientData);
                        Jim_ListAppendElement(interp, listObj, Jim_NewStringObj(interp, e->initialms ? "timer" : "idle", -1));
                        Jim_SetResult(interp, listObj);
                        return JIM_OK;
                    }
                }
                Jim_SetResultFormatted(interp, "event \"%#s\" doesn't exist", argv[2]);
                return JIM_ERR;
            }
            else {
                Jim_WrongNumArgs(interp, 2, argv, "?id?");
                return JIM_ERR;
            }
            break;
    }
    return JIM_OK;
}

int Jim_eventloopInit(Jim_Interp *interp)
{
    Jim_EventLoop *eventLoop;

    if (Jim_PackageProvide(interp, "eventloop", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    eventLoop = Jim_Alloc(sizeof(*eventLoop));
    eventLoop->fileEventHead = NULL;
    eventLoop->timeEventHead = NULL;
    eventLoop->timeEventNextId = 1;
    eventLoop->suppress_bgerror = 0;
    Jim_SetAssocData(interp, "eventloop", JimELAssocDataDeleProc, eventLoop);

    Jim_CreateCommand(interp, "vwait", JimELVwaitCommand, eventLoop, NULL);
    Jim_CreateCommand(interp, "update", JimELUpdateCommand, eventLoop, NULL);
    Jim_CreateCommand(interp, "after", JimELAfterCommand, eventLoop, NULL);

    return JIM_OK;
}
