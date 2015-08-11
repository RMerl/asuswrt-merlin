/*
 * $Id: w32-service.c 1429 2006-11-14 01:40:22Z rpedde $
 *
 * simple service management functions
 *
 * Copyright (C) 2005 Ron Pedde (ron.pedde@firstmarkcu.org)
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

#include "daapd.h"
#include "err.h"

/* Forwards */
BOOL service_update_status (DWORD,DWORD,DWORD,DWORD,DWORD);
void service_mainfunc(DWORD, LPTSTR *);
void service_init(void);
void service_handler(DWORD);

/* Globals */
SERVICE_STATUS_HANDLE service_status_handle = NULL;
HANDLE service_kill_event = NULL;
DWORD service_current_status;

/**
 * handle the stop, start, pause requests, etc.
 *
 * @param code action the SCM wants us to take
 */
void service_handler(DWORD code) {
    switch(code) {
        case SERVICE_CONTROL_INTERROGATE:
            break;

        case SERVICE_CONTROL_SHUTDOWN:
            // fallthrough

        case SERVICE_CONTROL_STOP:
            service_update_status(SERVICE_STOP_PENDING, NO_ERROR, 0, 1, 5000);
            SetEvent(service_kill_event);
            return;

        default:
          break;
   }
   service_update_status(service_current_status, NO_ERROR, 0, 0, 0);
}

/**
 * exit the service as gracefully as possible, returning the error
 * level specified.  I think there are some state machine problems
 * here, but probably nothing fatal -- an unnecessary call to
 * xfer_shutdown, maybe.  That's about it.
 *
 * @param errorlevel errorlevel to return
 */
void service_shutdown(int errorlevel) {
    DPRINTF(E_INF,L_MISC,"Service about to terminate with error %d\n",errorlevel);

    if(service_current_status != SERVICE_STOPPED) {
        service_update_status(SERVICE_STOP_PENDING, NO_ERROR, 0, 1, 5000);

        config.stop = 1;
        service_update_status(SERVICE_STOPPED,errorlevel,0,0,3000);
    }

    if(service_kill_event) {
        SetEvent(service_kill_event);
        CloseHandle(service_kill_event);
    }

//    exit(errorlevel);
    /* we'll let the main process exit (or hang!) */
    Sleep(5000);
    exit(1);
}

/**
 * The main for the service -- starts up the service and
 * makes it run.  This sits in a tight loop until the service
 * is shutdown.  If this function exits, it's because the
 * service is stopping.
 *
 * @param argc argc as passed from SCM
 * @param argv argv as passed from SCM
 */
void service_mainfunc(DWORD argc, LPTSTR *argv) {
    BOOL success;

    service_current_status = SERVICE_STOPPED;
    service_status_handle = RegisterServiceCtrlHandler(PACKAGE,
                           (LPHANDLER_FUNCTION) service_handler);

    if(!service_status_handle) {
        service_shutdown(GetLastError());
        return;
    }

    // Next Notify the Service Control Manager of progress
    success = service_update_status(SERVICE_START_PENDING, NO_ERROR, 0, 1, 5000);
    if (!success) {
        service_shutdown(GetLastError());
        return;
    }

    service_kill_event = CreateEvent (0, TRUE, FALSE, 0);
    if(!service_kill_event) {
        service_shutdown(GetLastError());
        return;
    }

    // FIXME
    // startup errors generate an ERR_FATAL, which exit()s in the
    // logging code -- fine for console apps, not so good here.
    // Generates ugly error messages from the service tool.
    // (terminated unexpectedly).
//    xfer_startup();

    // The service is now running.  Notify the SCM of this fact.
    service_current_status = SERVICE_RUNNING;
    success = service_update_status(SERVICE_RUNNING, NO_ERROR, 0, 0, 0);
    if (!success) {
        service_shutdown(GetLastError());
        return;
    }

    // Now just wait for our killed service signal, and then exit, which
    // terminates the service!
    WaitForSingleObject (service_kill_event, INFINITE);
    service_shutdown(0);
}


/**
 * update the scm service status, so it can make pretty
 * gui stupidness.  light wrapper around SetServiceStatus
 *
 * If the visual studio editor wasn't so horribly useless
 * and munge up the indents, I'd split these parameters
 * into several lines, but Microsoft apparently can't build
 * an editor that is as good as that made by volunteers.
 *
 * Those who do not understand Unix are condemned to reinvent
 * it, poorly.  -- Henry Spencer
 *
 * @param dwCurrentState new service state
 * @param dwWin32ExitCode exit code if state is SERVICE_STOPPED
 * @param dwServiceSpecificExitCode specified service exit code
 * @param dwCheckPoint incremented value for long startups
 * @param dwWaitHint how to before giving up on the service
 *
 * @returns result of SetServiceStatus - true on success
 */
BOOL service_update_status (DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwServiceSpecificExitCode, DWORD dwCheckPoint, DWORD dwWaitHint) {
    BOOL success;
    SERVICE_STATUS serviceStatus;

    service_current_status = dwCurrentState;

    serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    serviceStatus.dwCurrentState = dwCurrentState;
    if (dwCurrentState == SERVICE_START_PENDING) {
        serviceStatus.dwControlsAccepted = 0;
    } else {
        serviceStatus.dwControlsAccepted =
            SERVICE_ACCEPT_STOP |
            SERVICE_ACCEPT_SHUTDOWN;
    }

    if (dwServiceSpecificExitCode == 0) {
        serviceStatus.dwWin32ExitCode = dwWin32ExitCode;
    } else {
        serviceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
    }

    serviceStatus.dwServiceSpecificExitCode =   dwServiceSpecificExitCode;
    serviceStatus.dwCheckPoint = dwCheckPoint;
    serviceStatus.dwWaitHint = dwWaitHint;

    success = SetServiceStatus (service_status_handle, &serviceStatus);
    if (!success) {
        SetEvent(service_kill_event);
    }
    return success;
}

/**
 * kick off the application when in service mdoe - suitable for threadprocing
 */
void *service_startup(void *arg) {
    SERVICE_TABLE_ENTRY serviceTable[] = {
        { PACKAGE, (LPSERVICE_MAIN_FUNCTION) service_mainfunc },
        { NULL, NULL }
    };

    BOOL success;

    // Register the service with the Service Control Manager
    // If it were to fail, what would we do, anyway?
    success = StartServiceCtrlDispatcher(serviceTable);
    return NULL;
}

/**
 * install the service
 */
void service_register(void) {
    SC_HANDLE scm;
    SC_HANDLE svc;
    char path[PATH_MAX];

    GetModuleFileName(NULL, path, PATH_MAX );

    if(!(scm = OpenSCManager(0,0,SC_MANAGER_CREATE_SERVICE))) {
        DPRINTF(E_FATAL,L_MISC,"Cannot open service control manager\n");
    }

    svc = CreateService(scm,PACKAGE,SERVICENAME,SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,SERVICE_AUTO_START,SERVICE_ERROR_NORMAL,
        path,0,0,0,0,0);
    if(!svc) {
        DPRINTF(E_FATAL,L_MISC,"Cannot create service: %d\n",GetLastError());
    }

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);

    DPRINTF(E_LOG,L_MISC,"Registered service successfully\n");
}


/**
 * uninstall the service
 */
void service_unregister(void) {
    SC_HANDLE scm;
    SC_HANDLE svc;
    BOOL res;
    SERVICE_STATUS status;

    if(!(scm = OpenSCManager(0,0,SC_MANAGER_CREATE_SERVICE))) {
        DPRINTF(E_FATAL,L_MISC,"Cannot open service control manager: %d\n",GetLastError());
    }

    svc = OpenService(scm,PACKAGE,SERVICE_ALL_ACCESS | DELETE);
    if(!svc) {
        DPRINTF(E_FATAL,L_MISC,"Cannot open service: %d (is it installed??)\n",GetLastError());
    }

    res=QueryServiceStatus(svc,&status);
    if(!res) {
        DPRINTF(E_FATAL,L_MISC,"Cannot query service status: %d\n",GetLastError());
    }

    if(status.dwCurrentState != SERVICE_STOPPED) {
        DPRINTF(E_LOG,L_MISC,"Stopping service...\n");
        ControlService(svc,SERVICE_CONTROL_STOP,&status);
        // we should probably poll for service stoppage...
        Sleep(2000);
    }

    DPRINTF(E_LOG,L_MISC,"Deleting service...\n");
    res = DeleteService(svc);

    if(res) {
        DPRINTF(E_LOG,L_MISC,"Deleted successfully\n");
    } else {
        DPRINTF(E_FATAL,L_MISC,"Error deleting service: %d\n",GetLastError());
    }

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
}

