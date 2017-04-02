 /*
 * Copyright 2017, ASUSTeK Inc.
 * All Rights Reserved.
 *
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */

#ifndef __protect_srv_h__
#define __protect_srv_h__

/* SOCKET SERVER DEFINE SETTING 
---------------------------------*/
#define PROTECT_SRV_SOCKET_PATH               "/etc/ProtectSrv_socket"
#define PROTECT_SRV_MAX_SOCKET_CLIENT         5
#define PROTECT_SRV_PID_PATH                  "/tmp/ProtectSrv.pid"
#define PROTECT_SRV_LOG_FILE                  "/tmp/ProtectSrv.log"
#define PROTECT_SRV_DEBUG                     "/tmp/PTCSRV_DEBUG"

/*  IPTABLES SETTING 
---------------------------------*/
#define PROTECT_SRV_RULE_CHAIN                "SECURITY_PROTECT"
#define PROTECT_SRV_RULE_FILE                 "/tmp/ipt_protectSrv_rule"

typedef enum {
	PROTECTION_SERVICE_NONE=0,
	PROTECTION_SERVICE_WEB,
	PROTECTION_SERVICE_SSH,
	PROTECTION_SERVICE_TELNET,
	PROTECTION_SERVICE_SMB
} PTCSRV_TYPE_T;


/* NOTIFY PROTECTION STATE REPORT STRUCTURE
---------------------------------*/
typedef struct __ptcsrv_state_report__t_
{
	int        s_type;             /* Service type */
	char       addr[64];           /* Record fail address */
	char       msg[256];           /* Info */

       /* Control by Protection Srv */
	int        frequency;          /* Login retry fail time */
	time_t     tstamp;             /* Login fail timestamp */

} PTCSRV_STATE_REPORT_T;

typedef struct __drop_report__t_
{
	char       addr[64];
	time_t     tstamp;

} DROP_REPORT_T;

#endif
