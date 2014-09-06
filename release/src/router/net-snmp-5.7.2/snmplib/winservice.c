/*
 * Windows Service related function definitions
 * By Raju Krishnappa(raju_krishnappa@yahoo.com)
 *
 */

#ifdef WIN32

#include <net-snmp/net-snmp-config.h>

#include <windows.h>
#include <tchar.h>

#include <stdio.h>		/* sprintf */
#include <process.h>		/* beginthreadex  */

#include <net-snmp/library/winservice.h>

#ifdef mingw32 /* MinGW doesn't fully support exception handling. */

#define TRY if(1)
#define LEAVE goto labelFIN
#define FINALLY do { \
labelFIN: \
	; \
} while(0); if(1)

#else

#define TRY __try
#define LEAVE __leave
#define FINALLY __finally

#endif /* mingw32 */


#define CountOf(arr) ( sizeof(arr) / sizeof(arr[0]) )


#if defined(WIN32) && defined(HAVE_WIN32_PLATFORM_SDK) && !defined(mingw32)
#pragma comment(lib, "iphlpapi.lib")
#ifdef USING_WINEXTDLL_MODULE
#pragma comment(lib, "snmpapi.lib")
#pragma comment(lib, "mgmtapi.lib")
#endif
#endif

 
    /*
     * External global variables used here
     */

    /*
     * Application Name 
     * This should be declared by the application, which wants to register as
     * windows service
     */
extern LPTSTR app_name_long;

    /*
     * Declare global variable
     */

    /*
     * Flag to indicate whether process is running as Service 
     */
BOOL g_fRunningAsService = FALSE;

    /*
     * Variable to maintain Current Service status 
     */
static SERVICE_STATUS ServiceStatus;

    /*
     * Service Handle 
     */
static SERVICE_STATUS_HANDLE hServiceStatus = 0L;

    /*
     * Service Table Entry 
     */
SERVICE_TABLE_ENTRY ServiceTableEntry[] = {
  {NULL, ServiceMain},		/* Service Main function */
  {NULL, NULL}
};

    /*
     * Handle to Thread, to implement Pause, Resume and Stop functions
     */
static HANDLE hServiceThread = NULL;	/* Thread Handle */

    /*
     * Holds calling partys Function Entry point, that should start
     * when entering service mode
     */
static INT (*ServiceEntryPoint) (INT Argc, LPTSTR Argv[]) = 0L;

    /*
     * To hold Stop Function address, to be called when STOP request
     * received from the SCM
     */
static VOID (*StopFunction) (VOID) = 0L;


    /*
     * To update windows service status to SCM 
     */
static BOOL UpdateServiceStatus (DWORD dwStatus, DWORD dwErrorCode,
				 DWORD dwWaitHint);

    /*
     * To Report current service status to SCM 
     */
static BOOL ReportCurrentServiceStatus (VOID);

VOID
ProcessError (WORD eventLogType, LPCTSTR pszMessage, int useGetLastError, int quiet);

    /*
     * To register as Windows Service with SCM(Service Control Manager)
     * Input - Service Name, Service Display Name,Service Description and
     * Service startup arguments
     */
int
RegisterService (LPCTSTR lpszServiceName, LPCTSTR lpszServiceDisplayName,
		 LPCTSTR lpszServiceDescription,
		 InputParams * StartUpArg, int quiet) /* Startup argument to the service */
{
  TCHAR szServicePath[MAX_PATH];	/* To hold module File name */
  TCHAR MsgErrorString[MAX_STR_SIZE];	/* Message or Error string */
  TCHAR szServiceCommand[MAX_PATH + 9];	/* Command to execute */
  SC_HANDLE hSCManager = NULL;
  SC_HANDLE hService = NULL;
  TCHAR szRegAppLogKey[] =
    _T("SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\");
  TCHAR szRegKey[512];
  HKEY hKey = NULL;		/* Key to registry entry */
  HKEY hParamKey = NULL;	/* To store startup parameters */
  DWORD dwData;			/* Type of logging supported */
  DWORD i, j;			/* Loop variables */
  int exitStatus = 0;
  GetModuleFileName (NULL, szServicePath, MAX_PATH);
  TRY
  {

    /*
     * Open Service Control Manager handle 
     */
    hSCManager = OpenSCManager (NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (hSCManager == NULL)
      {
        ProcessError (EVENTLOG_ERROR_TYPE, _T ("Can't open SCM (Service Control Manager)"), 1, quiet);
        exitStatus = SERVICE_ERROR_SCM_OPEN;
	LEAVE;
      }

    /*
     * Generate the command to be executed by the SCM 
     */
    _sntprintf (szServiceCommand, CountOf(szServiceCommand), _T("%s %s"), szServicePath, _T ("-service"));

    /*
     * Create the desired service 
     */
    hService = CreateService (hSCManager, lpszServiceName, lpszServiceDisplayName,
			SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
			SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, szServiceCommand,
			      NULL,	/* load-order group */
			      NULL,	/* group member tag */
			      NULL,	/* dependencies */
			      NULL,	/* account */
			      NULL);	/* password */
    if (hService == NULL)
      {
	_sntprintf (MsgErrorString, CountOf(MsgErrorString), _T("%s %s"),
		   _T ("Can't create service"), lpszServiceDisplayName);
        ProcessError (EVENTLOG_ERROR_TYPE, MsgErrorString, 1, quiet);

        exitStatus = SERVICE_ERROR_CREATE_SERVICE;
	LEAVE;
      }

    /*
     * Create registry entries for the event log 
     */
    /*
     * Create registry Application event log key 
     */
    _tcscpy (szRegKey, szRegAppLogKey);
    _tcscat (szRegKey, lpszServiceName);

    /*
     * Create registry key 
     */
    if (RegCreateKey (HKEY_LOCAL_MACHINE, szRegKey, &hKey) != ERROR_SUCCESS)
      {
	_sntprintf (MsgErrorString, CountOf(MsgErrorString), _T("%s %s"),
		   _T ("is unable to create registry entries"), lpszServiceDisplayName);
        ProcessError (EVENTLOG_ERROR_TYPE, MsgErrorString, 1, quiet);
        exitStatus = SERVICE_ERROR_CREATE_REGISTRY_ENTRIES;
	LEAVE;
      }

    /*
     * Add Event ID message file name to the 'EventMessageFile' subkey 
     */
    RegSetValueEx (hKey, _T("EventMessageFile"), 0, REG_EXPAND_SZ,
		   (CONST BYTE *) szServicePath,
		   _tcslen (szServicePath) + sizeof (TCHAR));

    /*
     * Set the supported types flags. 
     */
    dwData = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
    RegSetValueEx (hKey, _T("TypesSupported"), 0, REG_DWORD,
		   (CONST BYTE *) & dwData, sizeof (DWORD));

    /*
     * Close Registry key 
     */
    RegCloseKey (hKey);

    /*
     * Set Service Description String  and save startup parameters if present
     */
    if (lpszServiceDescription != NULL || StartUpArg->Argc > 2)
      {
	/*
	 * Create Registry Key path 
	 */
	_tcscpy (szRegKey, _T ("SYSTEM\\CurrentControlSet\\Services\\"));
	_tcscat (szRegKey, app_name_long);
	hKey = NULL;

	/*
	 * Open Registry key using Create and Set access. 
	 */
	if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, szRegKey, 0, KEY_WRITE,
			  &hKey) != ERROR_SUCCESS)
	  {
	    _sntprintf (MsgErrorString, CountOf(MsgErrorString), _T("%s %s"),
		       _T ("is unable to create registry entries"),
		       lpszServiceDisplayName);
            ProcessError (EVENTLOG_ERROR_TYPE, MsgErrorString, 1, quiet);
            exitStatus = SERVICE_ERROR_CREATE_REGISTRY_ENTRIES;
	    LEAVE;
	  }

	/*
	 * Create description subkey and the set value 
	 */
	if (lpszServiceDescription != NULL)
	  {
	    if (RegSetValueEx (hKey, _T("Description"), 0, REG_SZ,
			       (CONST BYTE *) lpszServiceDescription,
			       _tcslen (lpszServiceDescription) +
			       sizeof (TCHAR)) != ERROR_SUCCESS)
	      {
		_sntprintf (MsgErrorString, CountOf(MsgErrorString), _T("%s %s"),
			   _T ("is unable to create registry entries"),
			   lpszServiceDisplayName);
                ProcessError (EVENTLOG_ERROR_TYPE, MsgErrorString, 1, quiet);
                exitStatus = SERVICE_ERROR_CREATE_REGISTRY_ENTRIES;
		LEAVE;
	      };
	  }

	/*
	 * Save startup arguments if they are present 
	 */
	if (StartUpArg->Argc > 2)
	  {
	    /*
	     * Create Subkey parameters 
	     */
	    if (RegCreateKeyEx
		(hKey, _T("Parameters"), 0, NULL,
		 REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
		 &hParamKey, NULL) != ERROR_SUCCESS)
	      {
		_sntprintf (MsgErrorString, CountOf(MsgErrorString), _T("%s %s"),
			   _T ("is unable to create registry entries"),
			   lpszServiceDisplayName);
                ProcessError (EVENTLOG_ERROR_TYPE, MsgErrorString, 1, quiet);
                exitStatus = SERVICE_ERROR_CREATE_REGISTRY_ENTRIES;
                LEAVE;
	      }

	    /*
	     * Save parameters 
	     */

	    /*
	     * Loop through arguments 
	     */
            if (quiet) /* Make sure we don't store -quiet arg */
              i = 3;
            else
              i = 2;

	    for (j = 1; i < StartUpArg->Argc; i++, j++)
	      {
		_sntprintf (szRegKey, CountOf(szRegKey), _T("%s%d"), _T ("Param"), j);

		/*
		 * Create registry key 
		 */
		if (RegSetValueEx
		    (hParamKey, szRegKey, 0, REG_SZ,
		     (CONST BYTE *) StartUpArg->Argv[i],
		     _tcslen (StartUpArg->Argv[i]) +
		     sizeof (TCHAR)) != ERROR_SUCCESS)
		  {
		    _sntprintf (MsgErrorString, CountOf(MsgErrorString), _T("%s %s"),
			       _T ("is unable to create registry entries"),
			       lpszServiceDisplayName);
                    ProcessError (EVENTLOG_ERROR_TYPE, MsgErrorString, 1, quiet);
                    exitStatus = SERVICE_ERROR_CREATE_REGISTRY_ENTRIES;
		    LEAVE;
		  };
	      }
	  }

	/*
	 * Everything is set, delete hKey 
	 */
	RegCloseKey (hParamKey);
	RegCloseKey (hKey);
      }

    /*
     * Ready to log messages 
     */

    /*
     * Successfully registered as service 
     */
    _sntprintf (MsgErrorString, CountOf(MsgErrorString), _T("%s %s"), lpszServiceName,
	       _T ("successfully registered as a service"));

    /*
     * Log message to eventlog 
     */
    ProcessError (EVENTLOG_INFORMATION_TYPE, MsgErrorString, 0, quiet);
  }

  FINALLY
  {
    if (hSCManager)
      CloseServiceHandle (hSCManager);
    if (hService)
      CloseServiceHandle (hService);
    if (hKey)
      RegCloseKey (hKey);
    if (hParamKey)
      RegCloseKey (hParamKey);
  }
  return (exitStatus);
}

    /*
     * Unregister the service with the  Windows SCM 
     * Input - ServiceName
     */
int
UnregisterService (LPCTSTR lpszServiceName, int quiet)
{
  TCHAR MsgErrorString[MAX_STR_SIZE];	/* Message or Error string */
  SC_HANDLE hSCManager = NULL;	/* SCM handle */
  SC_HANDLE hService = NULL;	/* Service Handle */
  SERVICE_STATUS sStatus;
  TCHAR szRegAppLogKey[] =
    _T("SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\");
  TCHAR szRegKey[512];
  int exitStatus = 0;
/*  HKEY hKey = NULL;		?* Key to registry entry */
  TRY
  {
    /*
     * Open Service Control Manager 
     */
    hSCManager = OpenSCManager (NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (hSCManager == NULL)
      {
        ProcessError (EVENTLOG_ERROR_TYPE, _T ("Can't open SCM (Service Control Manager)"), 1, quiet);
        exitStatus = SERVICE_ERROR_SCM_OPEN;       
	LEAVE;
      }

    /*
     * Open registered service 
     */
    hService = OpenService (hSCManager, lpszServiceName, SERVICE_ALL_ACCESS);
    if (hService == NULL)
      {
	_sntprintf (MsgErrorString, CountOf(MsgErrorString), _T("%s %s"), _T ("Can't open service"),
		   lpszServiceName);
        ProcessError (EVENTLOG_ERROR_TYPE, MsgErrorString, 1, quiet);
        exitStatus = SERVICE_ERROR_OPEN_SERVICE;       
	LEAVE;
      }

    /*
     * Query service status 
     * If running stop before deleting 
     */
    if (QueryServiceStatus (hService, &sStatus))
      {
	if (sStatus.dwCurrentState == SERVICE_RUNNING
	    || sStatus.dwCurrentState == SERVICE_PAUSED)
	  {
	    ControlService (hService, SERVICE_CONTROL_STOP, &sStatus);
	  }
      };

    /*
     * Delete the service  
     */
    if (DeleteService (hService) == FALSE)
      {
	_sntprintf (MsgErrorString, CountOf(MsgErrorString), _T("%s %s"), _T ("Can't delete service"),
		   lpszServiceName);

	/*
	 * Log message to eventlog 
	 */
        ProcessError (EVENTLOG_ERROR_TYPE, MsgErrorString, 0, quiet);
	LEAVE;
      }

    /*
     * Log "Service deleted successfully " message to eventlog
     */
    _sntprintf (MsgErrorString, CountOf(MsgErrorString), _T("%s %s"), lpszServiceName, _T ("service deleted"));
    ProcessError (EVENTLOG_INFORMATION_TYPE, MsgErrorString, 0, quiet);

    /*
     * Delete registry entries for EventLog 
     */
    _tcscpy (szRegKey, szRegAppLogKey);
    _tcscat (szRegKey, lpszServiceName);
    RegDeleteKey (HKEY_LOCAL_MACHINE, szRegKey);
  }

  /*
   * Delete the handles 
   */
  FINALLY
  {
    if (hService)
      CloseServiceHandle (hService);
    if (hSCManager)
      CloseServiceHandle (hSCManager);
  }
  return (exitStatus);
}

    /*
     * Write a message to the Windows event log.
     */
VOID
WriteToEventLog (WORD wType, LPCTSTR pszFormat, ...)
{
  TCHAR szMessage[512];
  LPCTSTR LogStr[1];
  va_list ArgList;
  HANDLE hEventSource = NULL;

  va_start (ArgList, pszFormat);
  _vsntprintf (szMessage, CountOf(szMessage), pszFormat, ArgList);
  va_end (ArgList);
  LogStr[0] = szMessage;
  hEventSource = RegisterEventSource (NULL, app_name_long);
  if (hEventSource == NULL)
    return;
  ReportEvent (hEventSource, wType, 0,
	       DISPLAY_MSG,
	       NULL, 1, 0, LogStr, NULL);
  DeregisterEventSource (hEventSource);
}

    /*
     * Pre-process the second command-line argument from the user. 
     *     Service related options are:
     *     -register       - registers the service
     *     -unregister     - unregisters the service
     *     -service        - run as service
     *     other command-line arguments are ignored here.
     *
     * Return: Type indicating the option specified
     */
INT
ParseCmdLineForServiceOption (int argc, TCHAR * argv[], int *quiet)
{
  int nReturn = RUN_AS_CONSOLE;	/* default is to run as a console application */

  if (argc >= 2)
    {

      /*
       * second argument present 
       */
      if (lstrcmpi (_T ("-register"), argv[1]) == 0)
	{
	  nReturn = REGISTER_SERVICE;
	}

      else if (lstrcmpi (_T ("-unregister"), argv[1]) == 0)
	{
	  nReturn = UN_REGISTER_SERVICE;
	}

      else if (lstrcmpi (_T ("-service"), argv[1]) == 0)
	{
	  nReturn = RUN_AS_SERVICE;
	}
    }

  if (argc >= 3)
  {
    /*
     * third argument present 
     */
    if (lstrcmpi (_T ("-quiet"), argv[2]) == 0)
    {
      *quiet = 1;	
    }
  }
  
  return nReturn;
}

    /*
     * Write error message to event log, console or pop-up window.
     *
     * If useGetLastError is 1, the last error returned from GetLastError()
     * is appended to pszMessage, separated by a ": ".
     *
     * eventLogType:                 MessageBox equivalent:
     * 
     * EVENTLOG_INFORMATION_TYPE     MB_ICONASTERISK
     * EVENTLOG_WARNING_TYPE         MB_ICONEXCLAMATION
     * EVENTLOG_ERROR_TYPE           MB_ICONSTOP
     * 
     */
VOID
ProcessError (WORD eventLogType, LPCTSTR pszMessage, int useGetLastError, int quiet)
{
  HANDLE hEventSource = NULL;
  TCHAR pszMessageFull[MAX_STR_SIZE]; /* Combined pszMessage and GetLastError */

  /*
   * If useGetLastError enabled, generate text from GetLastError() and append to
   * pszMessageFull
   */
  if (useGetLastError) {
  LPTSTR pErrorMsgTemp = NULL;
  FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER |
		 FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError (),
		 MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) & pErrorMsgTemp, 0, NULL);

    _sntprintf (pszMessageFull, CountOf(pszMessageFull), _T("%s: %s"), pszMessage, pErrorMsgTemp);
    if (pErrorMsgTemp) {
      LocalFree (pErrorMsgTemp);
      pErrorMsgTemp = NULL;
    }
  }
  else {
    _sntprintf (pszMessageFull, CountOf(pszMessageFull), _T("%s"), pszMessage);
  }
  
  hEventSource = RegisterEventSource (NULL, app_name_long);
  if (hEventSource != NULL) {
    LPCTSTR LogStr[1];
    LogStr[0] = pszMessageFull;
    
    if (ReportEvent (hEventSource, 
          eventLogType, 
          0,
          DISPLAY_MSG,	/* just output the text to the event log */
          NULL, 
          1, 
          0, 
          LogStr, 
          NULL)) {
    }
    else {
      LPTSTR pErrorMsgTemp = NULL;
      FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER |
          FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError (),
          MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
          (LPTSTR) & pErrorMsgTemp, 0, NULL);
      _ftprintf(stderr,_T("Could NOT lot to Event Log.  Error returned from ReportEvent(): %s\n"),pErrorMsgTemp);
      if (pErrorMsgTemp) {
        LocalFree (pErrorMsgTemp);
        pErrorMsgTemp = NULL;
      }
    }
    DeregisterEventSource (hEventSource);
    }

      if (quiet) {
    _ftprintf(stderr,_T("%s\n"),pszMessageFull);
      }
      else {
    switch (eventLogType) {
      case EVENTLOG_INFORMATION_TYPE:
        MessageBox (NULL, pszMessageFull, app_name_long, MB_ICONASTERISK);
        break;
      case EVENTLOG_WARNING_TYPE:
        MessageBox (NULL, pszMessageFull, app_name_long, MB_ICONEXCLAMATION);
        break;
      case EVENTLOG_ERROR_TYPE:
        MessageBox (NULL, pszMessageFull, app_name_long, MB_ICONSTOP);
        break;
      default:
        MessageBox (NULL, pszMessageFull, app_name_long, EVENTLOG_WARNING_TYPE);
        break;
      }
    }
}

    /*
     * Update current service status.
     * Sends the current service status to the SCM. Also updates
     * the global service status structure.
     */
static BOOL
UpdateServiceStatus (DWORD dwStatus, DWORD dwErrorCode, DWORD dwWaitHint)
{
  DWORD static dwCheckpoint = 1;
  DWORD dwControls = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE;
  if (g_fRunningAsService == FALSE)
    return FALSE;
  ZeroMemory (&ServiceStatus, sizeof (ServiceStatus));
  ServiceStatus.dwServiceType = SERVICE_WIN32;
  ServiceStatus.dwCurrentState = dwStatus;
  ServiceStatus.dwWaitHint = dwWaitHint;
  if (dwErrorCode)
    {
      ServiceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
      ServiceStatus.dwServiceSpecificExitCode = dwErrorCode;
    }

  /*
   * special cases that depend on the new state 
   */
  switch (dwStatus)
    {
    case SERVICE_START_PENDING:
      dwControls = 0;
      break;
    case SERVICE_RUNNING:
    case SERVICE_STOPPED:
      dwCheckpoint = 0;
      break;
    }
  ServiceStatus.dwCheckPoint = dwCheckpoint++;
  ServiceStatus.dwControlsAccepted = dwControls;
  return ReportCurrentServiceStatus ();
}

    /*
     * Reports current service status to SCM
     */
static BOOL
ReportCurrentServiceStatus ()
{
  return SetServiceStatus (hServiceStatus, &ServiceStatus);
}

    /*
     * ServiceMain function.
     */
VOID WINAPI
ServiceMain (DWORD argc, LPTSTR argv[])
{
  SECURITY_ATTRIBUTES SecurityAttributes;
  unsigned threadId;

  /*
   * Input arguments
   */
  DWORD ArgCount = 0;
  LPTSTR *ArgArray = NULL;
  TCHAR szRegKey[512];
  HKEY hParamKey = NULL;
  DWORD TotalParams = 0;
  DWORD i;
  InputParams ThreadInputParams;

  /*
   * Build the Input parameters to pass to worker thread 
   */

  /*
   * SCM sends Service Name as first arg, increment to point
   * arguments user specified while starting control agent
   */

  /*
   * Read registry parameter 
   */
  ArgCount = 1;

  /*
   * Create registry key path 
   */
  _sntprintf (szRegKey, CountOf(szRegKey), _T("%s%s\\%s"),
	     _T ("SYSTEM\\CurrentControlSet\\Services\\"), app_name_long,
	     _T("Parameters"));
  if (RegOpenKeyEx
      (HKEY_LOCAL_MACHINE, szRegKey, 0, KEY_ALL_ACCESS, &hParamKey) == ERROR_SUCCESS)
    {

      /*
       * Read startup configuration information 
       */
      /*
       * Find number of subkeys inside parameters 
       */
      if (RegQueryInfoKey (hParamKey, NULL, NULL, 0,
	   NULL, NULL, NULL, &TotalParams,
	   NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
	{
	  if (TotalParams != 0)
	    {
	      ArgCount += TotalParams;

	      /*
	       * Allocate memory to hold strings 
	       */
	      ArgArray = calloc(ArgCount, sizeof(ArgArray[0]));
              if (ArgArray == 0)
                {
                  WriteToEventLog (EVENTLOG_ERROR_TYPE,
		       _T ("Resource failure"));
                  return;
                }

	      /*
	       * Copy first argument 
	       */
	      ArgArray[0] = _tcsdup (argv[0]);
	      for (i = 1; i <= TotalParams; i++)
		{
                  DWORD dwErrorcode;
                  DWORD nSize;
                  DWORD nRegkeyType;
                  TCHAR *szValue;

		  /*
		   * Create Subkey value name 
		   */
		  _sntprintf (szRegKey, CountOf(szRegKey), _T("%s%d"), _T("Param"), i);

		  /*
		   * Query subkey.
		   */
		  nSize = 0;
		  dwErrorcode = RegQueryValueEx(hParamKey, szRegKey, NULL,
                                                &nRegkeyType, NULL, &nSize);
                  if (dwErrorcode == ERROR_SUCCESS) {
                    if (nRegkeyType == REG_SZ || nRegkeyType == REG_EXPAND_SZ) {
                      szValue = malloc(nSize + sizeof(szValue[0]));
                      if (szValue) {
		        dwErrorcode = RegQueryValueEx(hParamKey, szRegKey, NULL,
                                                      &nRegkeyType, (LPBYTE)szValue, &nSize);
                        if (dwErrorcode == ERROR_SUCCESS) {
                          szValue[nSize] = 0;
                          ArgArray[i] = szValue;
                        } else {
                          free(szValue);
                          WriteToEventLog(EVENTLOG_ERROR_TYPE, _T("Querying registry key %s failed: error code %ld"), szRegKey, dwErrorcode);
                        }
                      } else
                        WriteToEventLog(EVENTLOG_ERROR_TYPE, _T("Querying registry key %s failed: out of memory"), szRegKey);
                    } else
                      WriteToEventLog(EVENTLOG_ERROR_TYPE, _T("Type %ld of registry key %s is incorrect"), nRegkeyType, szRegKey);
                  } else
                    WriteToEventLog(EVENTLOG_ERROR_TYPE, _T("Querying registry key %s failed: error code %ld"), szRegKey, dwErrorcode);

                  if (!ArgArray[i]) {
                    TotalParams = ArgCount = i;
                    break;
                  }
		}
	    }
	}
      RegCloseKey (hParamKey);
    }
  if (ArgCount == 1)
    {

      /*
       * No startup args are given 
       */
      ThreadInputParams.Argc = argc;
      ThreadInputParams.Argv = argv;
    }

  else
    {
      ThreadInputParams.Argc = ArgCount;
      ThreadInputParams.Argv = ArgArray;
    }

  /*
   * Register Service Control Handler 
   */
  hServiceStatus = RegisterServiceCtrlHandler (app_name_long, ControlHandler);
  if (hServiceStatus == 0)
    {
      WriteToEventLog (EVENTLOG_ERROR_TYPE,
		       _T ("RegisterServiceCtrlHandler failed"));
      return;
    }

  /*
   * Update the service status to START_PENDING.
   */
  UpdateServiceStatus (SERVICE_START_PENDING, NO_ERROR, SCM_WAIT_INTERVAL);

  /*
   * Start the worker thread, which does the majority of the work .
   */
  TRY
  {
    if (SetSimpleSecurityAttributes (&SecurityAttributes) == FALSE)
      {
	WriteToEventLog (EVENTLOG_ERROR_TYPE,
			 _T ("Couldn't init security attributes"));
	LEAVE;
      }
    hServiceThread =
      (void *) _beginthreadex (&SecurityAttributes, 0,
			       ThreadFunction,
			       (void *) &ThreadInputParams, 0, &threadId);
    if (hServiceThread == NULL)
      {
	WriteToEventLog (EVENTLOG_ERROR_TYPE, _T ("Couldn't start worker thread"));
	LEAVE;
      }

    /*
     * Set service status to SERVICE_RUNNING.
     */
    UpdateServiceStatus (SERVICE_RUNNING, NO_ERROR, SCM_WAIT_INTERVAL);

    /*
     * Wait until the worker thread finishes.
     */
    WaitForSingleObject (hServiceThread, INFINITE);
  }
  FINALLY
  {
    /*
     * Release resources 
     */
    UpdateServiceStatus (SERVICE_STOPPED, NO_ERROR, SCM_WAIT_INTERVAL);
    if (hServiceThread)
      CloseHandle (hServiceThread);
    FreeSecurityAttributes (&SecurityAttributes);

    /*
     * Free allocated argument list 
     */
    if (ArgCount > 1 && ArgArray != NULL)
      {
	/*
	 * Free all strings 
	 */
	for (i = 0; i < ArgCount; i++)
	  {
	    free (ArgArray[i]);
	  }
	free (ArgArray);
      }
  }
}

    /*
     * Function to start as Windows service
     * The calling party should specify their entry point as input parameter
     * Returns TRUE if the Service is started successfully
     */
BOOL
RunAsService (INT (*ServiceFunction) (INT, LPTSTR *))
{

  /*
   * Set the ServiceEntryPoint 
   */
  ServiceEntryPoint = ServiceFunction;

  /*
   * By default, mark as Running as a service 
   */
  g_fRunningAsService = TRUE;

  /*
   * Initialize ServiceTableEntry table 
   */
  ServiceTableEntry[0].lpServiceName = app_name_long;	/* Application Name */

  /*
   * Call SCM via StartServiceCtrlDispatcher to run as Service 
   * * If the function returns TRUE we are running as Service, 
   */
  if (StartServiceCtrlDispatcher (ServiceTableEntry) == FALSE)
    {
      g_fRunningAsService = FALSE;

      /*
       * Some other error has occurred. 
       */
      WriteToEventLog (EVENTLOG_ERROR_TYPE,
		       _T ("Couldn't start service - %s"), app_name_long);
    }
  return g_fRunningAsService;
}

    /*
     * Service control handler function
     * Responds to SCM commands/requests
     * This service handles 4 commands
     * - interrogate, pause, continue and stop.
     */
VOID WINAPI
ControlHandler (DWORD dwControl)
{
  switch (dwControl)
    {
    case SERVICE_CONTROL_INTERROGATE:
      ProcessServiceInterrogate ();
      break;

    case SERVICE_CONTROL_PAUSE:
      ProcessServicePause ();
      break;

    case SERVICE_CONTROL_CONTINUE:
      ProcessServiceContinue ();
      break;

    case SERVICE_CONTROL_STOP:
      ProcessServiceStop ();
      break;
    }
}

    /*
     * To stop the service.
     * If a stop function was registered, invoke it,
     * otherwise terminate the worker thread.
     * After stopping, Service status is set to STOP in 
     * main loop
     */
VOID
ProcessServiceStop (VOID)
{
  UpdateServiceStatus (SERVICE_STOP_PENDING, NO_ERROR, SCM_WAIT_INTERVAL);

  if (StopFunction != NULL)
    {
      (*StopFunction) ();
    }

  else
    {
      TerminateThread (hServiceThread, 0);
    }
}

    /*
     * Returns the current state of the service to the SCM.
     */
VOID
ProcessServiceInterrogate (VOID)
{
  ReportCurrentServiceStatus ();
}

    /*
     * To Create a security descriptor with a NULL ACL, which
     * allows unlimited access. Returns a SECURITY_ATTRIBUTES
     * structure that contains the security descriptor.
     * The structure contains a dynamically allocated security
     * descriptor that must be freed either manually, or by
     * calling FreeSecurityAttributes 
     */
BOOL
SetSimpleSecurityAttributes (SECURITY_ATTRIBUTES * pSecurityAttr)
{
  BOOL fReturn = FALSE;
  SECURITY_DESCRIPTOR *pSecurityDesc = NULL;

  /*
   * If an invalid address is passed as a parameter, return
   * FALSE right away. 
   */
  if (!pSecurityAttr)
    return FALSE;
  pSecurityDesc =
    (SECURITY_DESCRIPTOR *) LocalAlloc (LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
  if (!pSecurityDesc)
    return FALSE;
  fReturn =
    InitializeSecurityDescriptor (pSecurityDesc, SECURITY_DESCRIPTOR_REVISION);
  if (fReturn != FALSE)
    {
      fReturn = SetSecurityDescriptorDacl (pSecurityDesc, TRUE, NULL, FALSE);
    }
  if (fReturn != FALSE)
    {
      pSecurityAttr->nLength = sizeof (SECURITY_ATTRIBUTES);
      pSecurityAttr->lpSecurityDescriptor = pSecurityDesc;
      pSecurityAttr->bInheritHandle = TRUE;
    }

  else
    {
      /*
       * Couldn't initialize or set security descriptor. 
       */
      LocalFree (pSecurityDesc);
    }
  return fReturn;
}

    /*
     * This function Frees the security descriptor, if any was created.
     */
VOID
FreeSecurityAttributes (SECURITY_ATTRIBUTES * pSecurityAttr)
{
  if (pSecurityAttr && pSecurityAttr->lpSecurityDescriptor)
    LocalFree (pSecurityAttr->lpSecurityDescriptor);
}

    /*
     * This function runs in the worker thread
     * until an exit is forced, or until the SCM issues the STOP command.
     * Invokes registered service function
     * Returns when called registered function returns
     *
     * Input:
     *   lpParam contains argc and argv, pass to service main function 
     */
unsigned WINAPI
ThreadFunction (LPVOID lpParam)
{
  InputParams * pInputArg = (InputParams *) lpParam;
  return (*ServiceEntryPoint) (pInputArg->Argc, pInputArg->Argv);
}

    /*
     * This function is called to register an application-specific function
     *   which is invoked when the SCM stops the worker thread.
     */
VOID
RegisterStopFunction (VOID (*StopFunc) (VOID))
{
  StopFunction = StopFunc;
}

    /*
     * SCM pause command invokes this function
     * If the service is not running, this function does nothing.
     * Otherwise, suspend the worker thread and update the status.
     */
VOID
ProcessServicePause (VOID)
{
  if (ServiceStatus.dwCurrentState == SERVICE_RUNNING)
    {
      UpdateServiceStatus (SERVICE_PAUSE_PENDING, NO_ERROR, SCM_WAIT_INTERVAL);

      if (SuspendThread (hServiceThread) != -1)
	{
	  UpdateServiceStatus (SERVICE_PAUSED, NO_ERROR, SCM_WAIT_INTERVAL);
	}
    }
}

    /*
     * SCM resume command invokes this function
     * If the service is not paused, this function does nothing.
     * Otherwise, resume the worker thread and update the status.
     */
VOID
ProcessServiceContinue (VOID)
{
  if (ServiceStatus.dwCurrentState == SERVICE_PAUSED)
    {
      UpdateServiceStatus (SERVICE_CONTINUE_PENDING, NO_ERROR, SCM_WAIT_INTERVAL);

      if (ResumeThread (hServiceThread) != -1)
	{
	  UpdateServiceStatus (SERVICE_RUNNING, NO_ERROR, SCM_WAIT_INTERVAL);
	}
    }
}

#endif /* WIN32 */


