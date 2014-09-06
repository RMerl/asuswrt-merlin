/*
 *  System MIB group implementation - system.c
 *
 */
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <sys/types.h>

#if HAVE_UTSNAME_H
#include <utsname.h>
#else
#if HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/sysORTable.h>

#include "util_funcs.h"
#include "system_mib.h"
#include "updates.h"

netsnmp_feature_require(watcher_read_only_int_scalar)

        /*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/

#define SYS_STRING_LEN	256
static char     version_descr[SYS_STRING_LEN] = NETSNMP_VERS_DESC;
static char     sysContact[SYS_STRING_LEN] = NETSNMP_SYS_CONTACT;
static char     sysName[SYS_STRING_LEN] = NETSNMP_SYS_NAME;
static char     sysLocation[SYS_STRING_LEN] = NETSNMP_SYS_LOC;
static oid      sysObjectID[MAX_OID_LEN];
static size_t sysObjectIDByteLength;

extern oid      version_sysoid[];
extern int      version_sysoid_len;

static int      sysServices = 72;
static int      sysServicesConfiged = 0;

extern oid      version_id[];
extern int      version_id_len;

static int      sysContactSet = 0, sysLocationSet = 0, sysNameSet = 0;

#if (defined (WIN32) && defined (HAVE_WIN32_PLATFORM_SDK)) || defined (mingw32)
static void     windowsOSVersionString(char [], size_t);
#endif

        /*********************
	 *
	 *  snmpd.conf config parsing
	 *
	 *********************/

static void
system_parse_config_string2(const char *token, char *cptr,
                            char* value, size_t size)
{
    if (strlen(cptr) < size) {
        strcpy(value, cptr);
    } else {
        netsnmp_config_error("%s token too long (must be < %lu):\n\t%s",
                             token, (unsigned long)size, cptr);
    }
}

static void
system_parse_config_string(const char *token, char *cptr,
                           const char *name, char* value, size_t size,
                           int* guard)
{
    if (*token == 'p') {
        if (*guard < 0) {
            /*
             * This is bogus (and shouldn't happen anyway) -- the value is
             * already configured read-only.
             */
            snmp_log(LOG_WARNING,
                     "ignoring attempted override of read-only %s.0\n", name);
            return;
        } else {
            *guard = 1;
        }
    } else {
        if (*guard > 0) {
            /*
             * This is bogus (and shouldn't happen anyway) -- we already read a
             * persistent value which we should ignore in favour of this one.
             */
            snmp_log(LOG_WARNING,
                     "ignoring attempted override of read-only %s.0\n", name);
            /*
             * Fall through and copy in this value.
             */
        }
        *guard = -1;
    }

    system_parse_config_string2(token, cptr, value, size);
}

static void
system_parse_config_sysdescr(const char *token, char *cptr)
{
    system_parse_config_string2(token, cptr, version_descr,
                                sizeof(version_descr));
}

static void
system_parse_config_sysloc(const char *token, char *cptr)
{
    system_parse_config_string(token, cptr, "sysLocation", sysLocation,
                               sizeof(sysLocation), &sysLocationSet);
}

static void
system_parse_config_syscon(const char *token, char *cptr)
{
    system_parse_config_string(token, cptr, "sysContact", sysContact,
                               sizeof(sysContact), &sysContactSet);
}

static void
system_parse_config_sysname(const char *token, char *cptr)
{
    system_parse_config_string(token, cptr, "sysName", sysName,
                               sizeof(sysName), &sysNameSet);
}

static void
system_parse_config_sysServices(const char *token, char *cptr)
{
    sysServices = atoi(cptr);
    sysServicesConfiged = 1;
}

static void
system_parse_config_sysObjectID(const char *token, char *cptr)
{
    size_t sysObjectIDLength = MAX_OID_LEN;
    if (!read_objid(cptr, sysObjectID, &sysObjectIDLength)) {
	netsnmp_config_error("sysobjectid token not a parsable OID:\n\t%s",
			     cptr);
        sysObjectIDByteLength = version_sysoid_len  * sizeof(oid);
        memcpy(sysObjectID, version_sysoid, sysObjectIDByteLength);
    } else

		sysObjectIDByteLength = sysObjectIDLength * sizeof(oid);
}


        /*********************
	 *
	 *  Initialisation & common implementation functions
	 *
	 *********************/

oid             system_module_oid[] = { SNMP_OID_SNMPMODULES, 1 };
int             system_module_oid_len = OID_LENGTH(system_module_oid);
int             system_module_count = 0;

static int
system_store(int a, int b, void *c, void *d)
{
    char            line[SNMP_MAXBUF_SMALL];

    if (sysLocationSet > 0) {
        snprintf(line, SNMP_MAXBUF_SMALL, "psyslocation %s", sysLocation);
        snmpd_store_config(line);
    }
    if (sysContactSet > 0) {
        snprintf(line, SNMP_MAXBUF_SMALL, "psyscontact %s", sysContact);
        snmpd_store_config(line);
    }
    if (sysNameSet > 0) {
        snprintf(line, SNMP_MAXBUF_SMALL, "psysname %s", sysName);
        snmpd_store_config(line);
    }

    return 0;
}

static int
handle_sysServices(netsnmp_mib_handler *handler,
                   netsnmp_handler_registration *reginfo,
                   netsnmp_agent_request_info *reqinfo,
                   netsnmp_request_info *requests)
{
#if NETSNMP_NO_DUMMY_VALUES
    if (reqinfo->mode == MODE_GET && !sysServicesConfiged)
        netsnmp_request_set_error(requests, SNMP_NOSUCHINSTANCE);
#endif
    return SNMP_ERR_NOERROR;
}

static int
handle_sysUpTime(netsnmp_mib_handler *handler,
                   netsnmp_handler_registration *reginfo,
                   netsnmp_agent_request_info *reqinfo,
                   netsnmp_request_info *requests)
{
    snmp_set_var_typed_integer(requests->requestvb, ASN_TIMETICKS,
                               netsnmp_get_agent_uptime());
    return SNMP_ERR_NOERROR;
}

void
init_system_mib(void)
{

#ifdef HAVE_UNAME
    struct utsname  utsName;

    uname(&utsName);
    snprintf(version_descr, sizeof(version_descr),
            "%s %s %s %s %s", utsName.sysname,
            utsName.nodename, utsName.release, utsName.version,
            utsName.machine);
    version_descr[ sizeof(version_descr)-1 ] = 0;
#else
#if HAVE_EXECV
    struct extensible extmp;

    /*
     * set default values of system stuff 
     */
    sprintf(extmp.command, "%s -a", UNAMEPROG);
    /*
     * setup defaults 
     */
    extmp.type = EXECPROC;
    extmp.next = NULL;
    exec_command(&extmp);
    strlcpy(version_descr, extmp.output, sizeof(version_descr));
    if (strlen(version_descr) >= 1)
        version_descr[strlen(version_descr) - 1] = 0; /* chomp new line */
#else
#if (defined (WIN32) && defined (HAVE_WIN32_PLATFORM_SDK)) || defined (mingw32)
    windowsOSVersionString(version_descr, sizeof(version_descr));
#else
    strcpy(version_descr, "unknown");
#endif
#endif
#endif

#ifdef HAVE_GETHOSTNAME
    gethostname(sysName, sizeof(sysName));
#else
#ifdef HAVE_UNAME
    strlcpy(sysName, utsName.nodename, sizeof(sysName));
#else
#if defined (HAVE_EXECV) && !defined (mingw32)
    sprintf(extmp.command, "%s -n", UNAMEPROG);
    /*
     * setup defaults 
     */
    extmp.type = EXECPROC;
    extmp.next = NULL;
    exec_command(&extmp);
    strlcpy(sysName, extmp.output, sizeof(sysName));
    if (strlen(sysName) >= 1)
        sysName[strlen(sysName) - 1] = 0; /* chomp new line */
#else
    strcpy(sysName, "unknown");
#endif                          /* HAVE_EXECV */
#endif                          /* HAVE_UNAME */
#endif                          /* HAVE_GETHOSTNAME */

#if (defined (WIN32) && defined (HAVE_WIN32_PLATFORM_SDK)) || defined (mingw32)
    {
      HKEY hKey;
      /* Default sysContact is the registered windows user */
      if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                       "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0,
                       KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
          char registeredOwner[256] = "";
          DWORD registeredOwnerSz = 256;
          if (RegQueryValueEx(hKey, "RegisteredOwner", NULL, NULL,
                              (LPBYTE)registeredOwner,
                              &registeredOwnerSz) == ERROR_SUCCESS) {
              strcpy(sysContact, registeredOwner);
          }
          RegCloseKey(hKey);
      }
    }
#endif

    /* default sysObjectID */
    memcpy(sysObjectID, version_sysoid, version_sysoid_len * sizeof(oid));
    sysObjectIDByteLength = version_sysoid_len * sizeof(oid);

    {
        const oid sysDescr_oid[] = { 1, 3, 6, 1, 2, 1, 1, 1 };
        static netsnmp_watcher_info sysDescr_winfo;
        netsnmp_register_watched_scalar(
            netsnmp_create_handler_registration(
                "mibII/sysDescr", NULL, sysDescr_oid, OID_LENGTH(sysDescr_oid),
                HANDLER_CAN_RONLY),
            netsnmp_init_watcher_info(&sysDescr_winfo, version_descr, 0,
				      ASN_OCTET_STR, WATCHER_SIZE_STRLEN));
    }
    {
        const oid sysObjectID_oid[] = { 1, 3, 6, 1, 2, 1, 1, 2 };
        static netsnmp_watcher_info sysObjectID_winfo;
        netsnmp_register_watched_scalar(
            netsnmp_create_handler_registration(
                "mibII/sysObjectID", NULL,
                sysObjectID_oid, OID_LENGTH(sysObjectID_oid),
                HANDLER_CAN_RONLY),
            netsnmp_init_watcher_info6(
		&sysObjectID_winfo, sysObjectID, 0, ASN_OBJECT_ID,
                WATCHER_MAX_SIZE | WATCHER_SIZE_IS_PTR,
                MAX_OID_LEN, &sysObjectIDByteLength));
    }
    {
        const oid sysUpTime_oid[] = { 1, 3, 6, 1, 2, 1, 1, 3 };
        netsnmp_register_scalar(
            netsnmp_create_handler_registration(
                "mibII/sysUpTime", handle_sysUpTime,
                sysUpTime_oid, OID_LENGTH(sysUpTime_oid),
                HANDLER_CAN_RONLY));
    }
    {
        const oid sysContact_oid[] = { 1, 3, 6, 1, 2, 1, 1, 4 };
        static netsnmp_watcher_info sysContact_winfo;
#ifndef NETSNMP_NO_WRITE_SUPPORT
        netsnmp_register_watched_scalar(
            netsnmp_create_update_handler_registration(
                "mibII/sysContact", sysContact_oid, OID_LENGTH(sysContact_oid), 
                HANDLER_CAN_RWRITE, &sysContactSet),
            netsnmp_init_watcher_info(
                &sysContact_winfo, sysContact, SYS_STRING_LEN - 1,
                ASN_OCTET_STR, WATCHER_MAX_SIZE | WATCHER_SIZE_STRLEN));
#else  /* !NETSNMP_NO_WRITE_SUPPORT */
        netsnmp_register_watched_scalar(
            netsnmp_create_update_handler_registration(
                "mibII/sysContact", sysContact_oid, OID_LENGTH(sysContact_oid),
                HANDLER_CAN_RONLY, &sysContactSet),
            netsnmp_init_watcher_info(
                &sysContact_winfo, sysContact, SYS_STRING_LEN - 1,
                ASN_OCTET_STR, WATCHER_MAX_SIZE | WATCHER_SIZE_STRLEN));
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    }
    {
        const oid sysName_oid[] = { 1, 3, 6, 1, 2, 1, 1, 5 };
        static netsnmp_watcher_info sysName_winfo;
#ifndef NETSNMP_NO_WRITE_SUPPORT
        netsnmp_register_watched_scalar(
            netsnmp_create_update_handler_registration(
                "mibII/sysName", sysName_oid, OID_LENGTH(sysName_oid),
                HANDLER_CAN_RWRITE, &sysNameSet),
            netsnmp_init_watcher_info(
                &sysName_winfo, sysName, SYS_STRING_LEN - 1, ASN_OCTET_STR,
                WATCHER_MAX_SIZE | WATCHER_SIZE_STRLEN));
#else  /* !NETSNMP_NO_WRITE_SUPPORT */
        netsnmp_register_watched_scalar(
            netsnmp_create_update_handler_registration(
                "mibII/sysName", sysName_oid, OID_LENGTH(sysName_oid),
                HANDLER_CAN_RONLY, &sysNameSet),
            netsnmp_init_watcher_info(
                &sysName_winfo, sysName, SYS_STRING_LEN - 1, ASN_OCTET_STR,
                WATCHER_MAX_SIZE | WATCHER_SIZE_STRLEN));
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    }
    {
        const oid sysLocation_oid[] = { 1, 3, 6, 1, 2, 1, 1, 6 };
        static netsnmp_watcher_info sysLocation_winfo;
#ifndef NETSNMP_NO_WRITE_SUPPORT
        netsnmp_register_watched_scalar(
            netsnmp_create_update_handler_registration(
                "mibII/sysLocation", sysLocation_oid,
                OID_LENGTH(sysLocation_oid),
                HANDLER_CAN_RWRITE, &sysLocationSet),
            netsnmp_init_watcher_info(
		&sysLocation_winfo, sysLocation, SYS_STRING_LEN - 1,
		ASN_OCTET_STR, WATCHER_MAX_SIZE | WATCHER_SIZE_STRLEN));
#else  /* !NETSNMP_NO_WRITE_SUPPORT */
        netsnmp_register_watched_scalar(
            netsnmp_create_update_handler_registration(
                "mibII/sysLocation", sysLocation_oid,
                OID_LENGTH(sysLocation_oid),
                HANDLER_CAN_RONLY, &sysLocationSet),
            netsnmp_init_watcher_info(
		&sysLocation_winfo, sysLocation, SYS_STRING_LEN - 1,
		ASN_OCTET_STR, WATCHER_MAX_SIZE | WATCHER_SIZE_STRLEN));
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    }
    {
        const oid sysServices_oid[] = { 1, 3, 6, 1, 2, 1, 1, 7 };
        netsnmp_register_read_only_int_scalar(
            "mibII/sysServices", sysServices_oid, OID_LENGTH(sysServices_oid),
            &sysServices, handle_sysServices);
    }
    if (++system_module_count == 3)
        REGISTER_SYSOR_ENTRY(system_module_oid,
                             "The MIB module for SNMPv2 entities");

    sysContactSet = sysLocationSet = sysNameSet = 0;

    /*
     * register our config handlers 
     */
    snmpd_register_config_handler("sysdescr",
                                  system_parse_config_sysdescr, NULL,
                                  "description");
    snmpd_register_config_handler("syslocation",
                                  system_parse_config_sysloc, NULL,
                                  "location");
    snmpd_register_config_handler("syscontact", system_parse_config_syscon,
                                  NULL, "contact-name");
    snmpd_register_config_handler("sysname", system_parse_config_sysname,
                                  NULL, "node-name");
    snmpd_register_config_handler("psyslocation",
                                  system_parse_config_sysloc, NULL, NULL);
    snmpd_register_config_handler("psyscontact",
                                  system_parse_config_syscon, NULL, NULL);
    snmpd_register_config_handler("psysname", system_parse_config_sysname,
                                  NULL, NULL);
    snmpd_register_config_handler("sysservices",
                                  system_parse_config_sysServices, NULL,
                                  "NUMBER");
    snmpd_register_config_handler("sysobjectid",
                                  system_parse_config_sysObjectID, NULL,
                                  "OID");
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                           system_store, NULL);
}

        /*********************
	 *
	 *  Internal implementation functions - None
	 *
	 *********************/

#if (defined (WIN32) && defined (HAVE_WIN32_PLATFORM_SDK)) || defined (mingw32)
static void
windowsOSVersionString(char stringbuf[], size_t stringbuflen)
{
    /* copy OS version to string buffer in 'uname -a' format */
    OSVERSIONINFOEX osVersionInfo;
    BOOL gotOsVersionInfoEx;
    char windowsVersion[256] = "";
    char hostname[256] = "";
    char identifier[256] = "";
    DWORD identifierSz = 256;
    HKEY hKey;

    ZeroMemory(&osVersionInfo, sizeof(OSVERSIONINFOEX));
    osVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    gotOsVersionInfoEx = GetVersionEx((OSVERSIONINFO *)&osVersionInfo);
    if (gotOsVersionInfoEx == FALSE) {
       GetVersionEx((OSVERSIONINFO *)&osVersionInfo);
    }

    switch (osVersionInfo.dwPlatformId) {
       case VER_PLATFORM_WIN32_NT:
          if ((osVersionInfo.dwMajorVersion == 5) && (osVersionInfo.dwMinorVersion == 2)) {
             strcat(windowsVersion, "Server 2003");
          } else if ((osVersionInfo.dwMajorVersion == 5) && (osVersionInfo.dwMinorVersion == 1)) {
             strcat(windowsVersion, "XP");
          } else if ((osVersionInfo.dwMajorVersion == 5) && (osVersionInfo.dwMinorVersion == 0)) {
             strcat(windowsVersion, "2000");
          } else if (osVersionInfo.dwMajorVersion <= 4) {
             strcat(windowsVersion, "NT");
          }
          if (gotOsVersionInfoEx == TRUE) {
             if (osVersionInfo.wProductType == VER_NT_WORKSTATION) {
                if (osVersionInfo.dwMajorVersion == 4) {
                   strcat(windowsVersion, " Workstation 4.0");
                } else if (osVersionInfo.wSuiteMask & VER_SUITE_PERSONAL) {
                   strcat(windowsVersion, " Home Edition");
                } else {
                   strcat(windowsVersion, " Professional");
                }
             } else if (osVersionInfo.wProductType == VER_NT_SERVER) {
                if ((osVersionInfo.dwMajorVersion == 5) && (osVersionInfo.dwMinorVersion == 2)) {
                   if (osVersionInfo.wSuiteMask & VER_SUITE_DATACENTER) {
                      strcat(windowsVersion, " Datacenter Edition");
                   } else if (osVersionInfo.wSuiteMask & VER_SUITE_ENTERPRISE) {
                      strcat(windowsVersion, " Enterprise Edition");
                   } else if (osVersionInfo.wSuiteMask == VER_SUITE_BLADE) {
                      strcat(windowsVersion, " Web Edition");
                   } else {
                      strcat(windowsVersion, " Standard Edition");
                   }
                } else if ((osVersionInfo.dwMajorVersion == 5) && (osVersionInfo.dwMinorVersion == 0)) {
                   if (osVersionInfo.wSuiteMask & VER_SUITE_DATACENTER) {
                      strcat(windowsVersion, " Datacenter Server");
                   } else if (osVersionInfo.wSuiteMask & VER_SUITE_ENTERPRISE) {
                      strcat(windowsVersion, " Advanced Server");
                   } else {
                      strcat(windowsVersion, " Server");
                   }
                } else {
                   if (osVersionInfo.wSuiteMask & VER_SUITE_ENTERPRISE) {
                      strcat(windowsVersion, " Server 4.0, Enterprise Edition");
                   } else {
                      strcat(windowsVersion, " Server 4.0");
                   }
                }
             }
          } else {
             char productType[80];
             DWORD productTypeSz = 80;

             if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\ProductOptions", 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
                if (RegQueryValueEx(hKey, "ProductType", NULL, NULL, (LPBYTE) productType, &productTypeSz) == ERROR_SUCCESS) {
                   char versionStr[10];
                   if (strcmpi("WINNT", productType) == 0) {
                      strcat(windowsVersion, " Workstation");
                   } else if (strcmpi("LANMANNT", productType) == 0) {
                      strcat(windowsVersion, " Server");
                   } else if (strcmpi("SERVERNT", productType) == 0) {
                      strcat(windowsVersion, " Advanced Server");
                   }
                   sprintf(versionStr, " %d.%d", (int)osVersionInfo.dwMajorVersion, (int)osVersionInfo.dwMinorVersion);
                   strcat(windowsVersion, versionStr);
                }
                RegCloseKey(hKey);
             }
          }
          break;
       case VER_PLATFORM_WIN32_WINDOWS:
          if ((osVersionInfo.dwMajorVersion == 4) && (osVersionInfo.dwMinorVersion == 90)) {
             strcat(windowsVersion, "ME");
          } else if ((osVersionInfo.dwMajorVersion == 4) && (osVersionInfo.dwMinorVersion == 10)) {
             strcat(windowsVersion, "98");
             if (osVersionInfo.szCSDVersion[1] == 'A') {
                strcat(windowsVersion, " SE");
             }
          } else if ((osVersionInfo.dwMajorVersion == 4) && (osVersionInfo.dwMinorVersion == 0)) {
             strcat(windowsVersion, "95");
             if ((osVersionInfo.szCSDVersion[1] == 'C') || (osVersionInfo.szCSDVersion[1] == 'B')) {
                strcat(windowsVersion, " OSR2");
             }
          }
          break;
    }

    gethostname(hostname, sizeof(hostname));

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
       RegQueryValueEx(hKey, "Identifier", NULL, NULL, (LPBYTE)&identifier, &identifierSz);
       RegCloseKey(hKey);
    }

    /* Output is made to look like results from uname -a */
    snprintf(stringbuf, stringbuflen,
            "Windows %s %d.%d.%d %s %s %s", hostname,
             (int)osVersionInfo.dwMajorVersion, (int)osVersionInfo.dwMinorVersion,
             (int)osVersionInfo.dwBuildNumber, osVersionInfo.szCSDVersion,
             windowsVersion, identifier);
}
#endif /* WIN32 and HAVE_WIN32_PLATFORM_SDK or mingw32 */

