
/* Syslog interface for tcl
 * Copyright Victor Wagner <vitus@ice.ru> at
 * http://www.ice.ru/~vitus/works/tcl.html#syslog
 *
 * Slightly modified by Steve Bennett <steveb@snapgear.com>
 * Ported to Jim by Steve Bennett <steveb@workware.net.au>
 */
#include <syslog.h>
#include <string.h>

#include "jim.h"
#include "jimautoconf.h"

typedef struct
{
    int logOpened;
    int facility;
    int options;
    char ident[32];
} SyslogInfo;

#ifndef LOG_AUTHPRIV
# define LOG_AUTHPRIV LOG_AUTH
#endif

static const char * const facilities[] = {
    [LOG_AUTHPRIV] = "authpriv",
    [LOG_CRON] = "cron",
    [LOG_DAEMON] = "daemon",
    [LOG_KERN] = "kernel",
    [LOG_LPR] = "lpr",
    [LOG_MAIL] = "mail",
    [LOG_NEWS] = "news",
    [LOG_SYSLOG] = "syslog",
    [LOG_USER] = "user",
    [LOG_UUCP] = "uucp",
    [LOG_LOCAL0] = "local0",
    [LOG_LOCAL1] = "local1",
    [LOG_LOCAL2] = "local2",
    [LOG_LOCAL3] = "local3",
    [LOG_LOCAL4] = "local4",
    [LOG_LOCAL5] = "local5",
    [LOG_LOCAL6] = "local6",
    [LOG_LOCAL7] = "local7",
};

static const char * const priorities[] = {
    [LOG_EMERG] = "emerg",
    [LOG_ALERT] = "alert",
    [LOG_CRIT] = "crit",
    [LOG_ERR] = "error",
    [LOG_WARNING] = "warning",
    [LOG_NOTICE] = "notice",
    [LOG_INFO] = "info",
    [LOG_DEBUG] = "debug",
};

/**
 * Deletes the syslog command.
 */
static void Jim_SyslogCmdDelete(Jim_Interp *interp, void *privData)
{
    SyslogInfo *info = (SyslogInfo *) privData;

    if (info->logOpened) {
        closelog();
    }
    Jim_Free(info);
}

/* Syslog_Log -
 * implements syslog tcl command. General format: syslog ?options? level text
 * options -facility -ident -options
 *
 * syslog ?-facility cron|daemon|...? ?-ident string? ?-options int? ?debug|info|...? text
 */
int Jim_SyslogCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int priority = LOG_INFO;
    int i = 1;
    SyslogInfo *info = Jim_CmdPrivData(interp);

    if (argc <= 1) {
      wrongargs:
        Jim_WrongNumArgs(interp, 1, argv,
            "?-facility cron|daemon|...? ?-ident string? ?-options int? ?debug|info|...? message");
        return JIM_ERR;
    }
    while (i < argc - 1) {
        if (Jim_CompareStringImmediate(interp, argv[i], "-facility")) {
            int entry =
                Jim_FindByName(Jim_String(argv[i + 1]), facilities,
                sizeof(facilities) / sizeof(*facilities));
            if (entry < 0) {
                Jim_SetResultString(interp, "Unknown facility", -1);
                return JIM_ERR;
            }
            if (info->facility != entry) {
                info->facility = entry;
                if (info->logOpened) {
                    closelog();
                    info->logOpened = 0;
                }
            }
        }
        else if (Jim_CompareStringImmediate(interp, argv[i], "-options")) {
            long tmp;

            if (Jim_GetLong(interp, argv[i + 1], &tmp) == JIM_ERR) {
                return JIM_ERR;
            }
            info->options = tmp;
            if (info->logOpened) {
                closelog();
                info->logOpened = 0;
            }
        }
        else if (Jim_CompareStringImmediate(interp, argv[i], "-ident")) {
            strncpy(info->ident, Jim_String(argv[i + 1]), sizeof(info->ident));
            info->ident[sizeof(info->ident) - 1] = 0;
            if (info->logOpened) {
                closelog();
                info->logOpened = 0;
            }
        }
        else {
            break;
        }
        i += 2;
    }

    /* There should be either 0, 1 or 2 args left */
    if (i == argc) {
        /* No args, but they have set some options, so OK */
        return JIM_OK;
    }

    if (i < argc - 1) {
        priority =
            Jim_FindByName(Jim_String(argv[i]), priorities,
            sizeof(priorities) / sizeof(*priorities));
        if (priority < 0) {
            Jim_SetResultString(interp, "Unknown priority", -1);
            return JIM_ERR;
        }
        i++;
    }

    if (i != argc - 1) {
        goto wrongargs;
    }
    if (!info->logOpened) {
        if (!info->ident[0]) {
            Jim_Obj *argv0 = Jim_GetGlobalVariableStr(interp, "argv0", JIM_NONE);

            if (argv0) {
                strncpy(info->ident, Jim_String(argv0), sizeof(info->ident));
            }
            else {
                strcpy(info->ident, "Tcl script");
            }
            info->ident[sizeof(info->ident) - 1] = 0;
        }
        openlog(info->ident, info->options, info->facility);
        info->logOpened = 1;
    }
    syslog(priority, "%s", Jim_String(argv[i]));

    return JIM_OK;
}

int Jim_syslogInit(Jim_Interp *interp)
{
    SyslogInfo *info;

    if (Jim_PackageProvide(interp, "syslog", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    info = Jim_Alloc(sizeof(*info));

    info->logOpened = 0;
    info->options = 0;
    info->facility = LOG_USER;
    info->ident[0] = 0;

    Jim_CreateCommand(interp, "syslog", Jim_SyslogCmd, info, Jim_SyslogCmdDelete);

    return JIM_OK;
}
