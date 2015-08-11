/*
 * $Id: w32-eventlog.c 1484 2007-01-17 01:06:16Z rpedde $
 *
 * eventlog messages utility functions
 *
 * Copyright (C) 2005-2006 Ron Pedde (ron@pedde.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <windows.h>
#include <stdio.h>

#include "daapd.h"
#include "messages.h"

static HANDLE elog_handle = NULL;

/**
 * register eventlog functions
 *
 * @returns TRUE if successful, FALSE otherwise
 */
int elog_register(void) {
    HKEY reg_key = NULL;
    DWORD err = 0;
    char path[PATH_MAX];
    DWORD event_types;

    wsprintf(path,"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\%s", PACKAGE);
    if((err=RegCreateKey(HKEY_LOCAL_MACHINE, path, &reg_key)) != ERROR_SUCCESS)
        return FALSE;

    GetModuleFileName(NULL, path, PATH_MAX);

    err=RegSetValueEx(reg_key, "EventMessageFile",0,REG_EXPAND_SZ,path,(DWORD)strlen(path) + 1);
    if(err != ERROR_SUCCESS) {
        RegCloseKey(reg_key);
        return FALSE;
    }

    event_types = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
    err=RegSetValueEx(reg_key,"TypesSupported", 0, REG_DWORD, (BYTE*)&event_types, sizeof event_types );
    if(err != ERROR_SUCCESS) {
        RegCloseKey(reg_key);
        return FALSE;
    }

    RegCloseKey(reg_key);

    return TRUE;
}

/**
 * FIXME: ENOTIMPL
 */
int elog_unregister(void) {
    return TRUE;
}

/**
 * initialize the eventlog
 *
 * @returns TRUE if successful, FALSE otherwise
 */
int elog_init(void) {
    elog_handle=RegisterEventSource(NULL, PACKAGE);
    if(elog_handle == INVALID_HANDLE_VALUE)
        return FALSE;

    return TRUE;
}


/**
 * uninitialize the eventlog
 *
 * @returns TRUE
 */
int elog_deinit(void) {
    DeregisterEventSource(elog_handle);
    return TRUE;
}

/**
 * log a message to the event viewer
 *
 * @param level event level to log: 0=fatal, 3=warn, > info
 * @returns TRUE on success, FALSE otherwise
 */

int elog_message(int level, char *msg) {
    WORD message_level = EVENTLOG_INFORMATION_TYPE;
    int ret;

    if(level == 0)
        message_level = EVENTLOG_ERROR_TYPE;

    ret = ReportEvent(
        elog_handle,
        message_level,
        0,
        EVENT_MSG,
        NULL,
        1,
        0,
        &msg,
        NULL);

    /* ReportEvent returns non-zero on success */
    return ret ? TRUE : FALSE;
}
