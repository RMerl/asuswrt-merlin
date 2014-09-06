/*
 * swrun_cygwin.c:
 *     hrSWRunTable data access:
 *     Cygwin interface 
 */
#include <net-snmp/net-snmp-config.h>

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <windows.h>
#include <sys/cygwin.h>
#include <tlhelp32.h>
#include <psapi.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/library/container.h>
#include <net-snmp/library/snmp_debug.h>
#include <net-snmp/data_access/swrun.h>
#include <net-snmp/data_access/swrun.h>

/*
 * a lot of this is "stolen" from cygwin ps.cc
 */

typedef         BOOL(WINAPI * ENUMPROCESSMODULES) (HANDLE hProcess,
                                                   HMODULE * lphModule,
                                                   DWORD cb,
                                                   LPDWORD lpcbNeeded);

typedef         DWORD(WINAPI * GETMODULEFILENAME) (HANDLE hProcess,
                                                   HMODULE hModule,
                                                   LPTSTR lpstrFIleName,
                                                   DWORD nSize);

typedef         DWORD(WINAPI * GETPROCESSMEMORYINFO) (HANDLE hProcess,
                                                      PPROCESS_MEMORY_COUNTERS
                                                      pmc, DWORD nSize);

typedef         HANDLE(WINAPI * CREATESNAPSHOT) (DWORD dwFlags,
                                                 DWORD th32ProcessID);

typedef         BOOL(WINAPI * PROCESSWALK) (HANDLE hSnapshot,
                                            LPPROCESSENTRY32 lppe);

ENUMPROCESSMODULES myEnumProcessModules;
GETMODULEFILENAME myGetModuleFileNameEx;
CREATESNAPSHOT  myCreateToolhelp32Snapshot;
PROCESSWALK     myProcess32First;
PROCESSWALK     myProcess32Next;
GETPROCESSMEMORYINFO myGetProcessMemoryInfo = NULL;
cygwin_getinfo_types query = CW_GETPINFO;

static BOOL WINAPI
dummyprocessmodules(HANDLE hProcess,
                    HMODULE * lphModule, DWORD cb, LPDWORD lpcbNeeded)
{
    lphModule[0] = (HMODULE) * lpcbNeeded;
    *lpcbNeeded = 1;
    return 1;
}

static DWORD WINAPI
GetModuleFileNameEx95(HANDLE hProcess,
                      HMODULE hModule, LPTSTR lpstrFileName, DWORD n)
{
    HANDLE          h;
    DWORD           pid = (DWORD) hModule;
    PROCESSENTRY32  proc;

    h = myCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (!h)
        return 0;
    proc.dwSize = sizeof(proc);
    if (myProcess32First(h, &proc))
        do
            if (proc.th32ProcessID == pid) {
                CloseHandle(h);
                strcpy(lpstrFileName, proc.szExeFile);
                return 1;
            }
        while (myProcess32Next(h, &proc));
    CloseHandle(h);
    return 0;
}

#define FACTOR (0x19db1ded53ea710LL)
#define NSPERSEC 10000000LL
#define NSPERMSEC 10000LL

static time_t   __stdcall
to_time_t(PFILETIME ptr)
{
    long            rem;
    long long       x =
        ((long long) ptr->dwHighDateTime << 32) +
        ((unsigned) ptr->dwLowDateTime);
    x -= FACTOR;
    rem = x % NSPERSEC;
    rem += NSPERSEC / 2;
    x /= NSPERSEC;
    x += rem / NSPERSEC;
    return x;
}

static long
to_msec(PFILETIME ptr)
{
    long long       x =
        ((long long) ptr->dwHighDateTime << 32) +
        (unsigned) ptr->dwLowDateTime;
    x /= NSPERMSEC;
    return x;
}

OSVERSIONINFO   ver;
HMODULE         h;

/* ---------------------------------------------------------------------
 */
void
netsnmp_arch_swrun_init(void)
{
    memset(&ver, 0, sizeof ver);
    ver.dwOSVersionInfoSize = sizeof ver;
    GetVersionEx(&ver);

    if (ver.dwPlatformId == VER_PLATFORM_WIN32_NT) {
        h = LoadLibrary("psapi.dll");
        if (h) {
            myEnumProcessModules   = (ENUMPROCESSMODULES)
                       GetProcAddress(h, "EnumProcessModules");
            myGetModuleFileNameEx  = (GETMODULEFILENAME)
                       GetProcAddress(h, "GetModuleFileNameExA");
            myGetProcessMemoryInfo = (GETPROCESSMEMORYINFO)
                       GetProcAddress(h, "GetProcessMemoryInfo");
            if (myEnumProcessModules && myGetModuleFileNameEx)
                query = CW_GETPINFO_FULL;
            else
                snmp_log(LOG_ERR, "hr_swrun failed NT init\n");
        } else
            snmp_log(LOG_ERR, "hr_swrun failed to load psapi.dll\n");
    } else {
        h = GetModuleHandle("KERNEL32.DLL");
        myCreateToolhelp32Snapshot = (CREATESNAPSHOT)
                       GetProcAddress(h, "CreateToolhelp32Snapshot");
        myProcess32First = (PROCESSWALK) GetProcAddress(h, "Process32First");
        myProcess32Next  = (PROCESSWALK) GetProcAddress(h, "Process32Next");
        myEnumProcessModules  = dummyprocessmodules;
        myGetModuleFileNameEx = GetModuleFileNameEx95;
        if (myCreateToolhelp32Snapshot && myProcess32First
            && myProcess32Next)
#if 0
            /*
             * This doesn't work after all on Win98 SE 
             */
            query = CW_GETPINFO_FULL;
#else
            query = CW_GETPINFO;
#endif
        else
            snmp_log(LOG_ERR, "hr_swrun failed non-NT init\n");
    }
    return;
}

/* ---------------------------------------------------------------------
 */
int
netsnmp_arch_swrun_container_load( netsnmp_container *container, u_int flags)
{
    struct external_pinfo  *curproc;
    pid_t                   curpid = 0;
    DWORD                   n;
    HANDLE                  h;
    FILETIME                ct, et, kt, ut;
    PROCESS_MEMORY_COUNTERS pmc;
    char                   *cp1, *cp2;
    netsnmp_swrun_entry    *entry;

    cygwin_internal(CW_LOCK_PINFO, 1000);
    while (curproc = (struct external_pinfo *)
               cygwin_internal(query, curpid | CW_NEXTPID)) {

        curpid = curproc->pid;
        if (( PID_EXITED & curproc.process_state ) ||
               ( ~0xffff & curproc.exitcode ))
            continue;
        entry = netsnmp_swrun_entry_create(curpid);
        if (NULL == entry)
            continue;   /* error already logged by function */
        rc = CONTAINER_INSERT(container, entry);


        n = lowproc.dwProcessId & 0xffff;
        h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, n);

        if (curproc.ppid) {
            entry->hrSWRunPath_len = snprintf(entry->hrSWRunPath,
                                       sizeof(entry->hrSWRunPath)-1, "%s",
                    cygwin_conv_to_posix_path(curproc.progname));
        } else if (query == CW_GETPINFO_FULL) {

            if (h) {
                HMODULE         hm[1000];
                if (!myEnumProcessModules(h, hm, sizeof hm, &n))
                    n = 0;
                if (n && myGetModuleFileNameEx(h, hm[0], string,
                                                  sizeof string)) {
                   entry->hrSWRunPath_len = snprintf(entry->hrSWRunPath,
                                              sizeof(entry->hrSWRunPath)-1,
                                                    "%s", string );
                }
            }
        }
        /*
         * Set hrSWRunName to be the last component of hrSWRunPath,
         *    but without any file extension
         */
        if ( entry->hrSWRunPath_len ) {
            cp1 = strrchr( entry->hrSWRunPath, '.' );
            if ( cp1 )
                *cp1 = '\0';    /* Mask the file extension */

            cp2  = strrchr( entry->hrSWRunPath, '/' );
            if (cp2) 
                cp2++;           /* Find the final component ... */
            else
                cp2 = entry->hrSWRunPath;          /* ... if any */
            entry->hrSWRunName_len = snprintf(entry->hrSWRunName,
                                       sizeof(entry->hrSWRunName)-1, "%s", cp2);

            if ( cp1 )
                *cp1 = '.';     /* Restore the file extension */
        }

        /*
         * XXX - No information regarding process arguments
         * XXX - No information regarding system processes vs applications
         */

        if (PID_STOPPED & curproc.process_state ) {
            entry->hrSWRunStatus = HRSWRUNSTATUS_NOTRUNNABLE;
        }
      /*  else if (PID_ZOMBIE & curproc.process_state )  */
          else if (   ~0xffff & curproc.exitcode )
        {
            entry->hrSWRunStatus = HRSWRUNSTATUS_INVALID;
        } else {
        /*  entry->hrSWRunStatus = HRSWRUNSTATUS_RUNNABLE;  ?? */
            entry->hrSWRunStatus = HRSWRUNSTATUS_RUNNING;
        }

        if (h) {
            if (GetProcessTimes(h, &ct, &et, &kt, &ut))
                entry->hrSWRunPerfCPU = (to_msec(&kt) + to_msec(&ut)) / 10;
            if (myGetProcessMemoryInfo
                && myGetProcessMemoryInfo(h, &pmc, sizeof pmc))
                entry->hrSWRunPerfMem = pmc.WorkingSetSize / 1024;

            CloseHandle(h);
        }
    }
    cygwin_internal(CW_UNLOCK_PINFO);

    DEBUGMSGTL(("swrun:load:arch"," loaded %d entries\n",
                CONTAINER_SIZE(container)));

    return 0;
}
