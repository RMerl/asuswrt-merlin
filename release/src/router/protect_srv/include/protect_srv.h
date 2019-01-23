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
#define PROTECT_SRV_MAX_SOCKET_CLIENT         5
#define PROTECT_SRV_SOCKET_PATH               "/var/run/protect_srv_socket"
#define PROTECT_SRV_PID_PATH                  "/var/run/protect_srv.pid"
#define PROTECT_SRV_LOG_FILE                  "/tmp/protect_srv.log"
#define PROTECT_SRV_DEBUG                     "/tmp/PTCSRV_DEBUG"

/*  IPTABLES SETTING 
---------------------------------*/
#define PROTECT_SRV_RULE_CHAIN                "PTCSRV"
#define PROTECT_SRV_RULE_FILE                 "/tmp/ipt_protectSrv_rule"

typedef enum {
	PROTECTION_SERVICE_SSH=0,
	PROTECTION_SERVICE_TELNET
} PTCSRV_TYPE_T;

typedef enum {
	UNSTATUS=-1,
	RPT_FAIL=0,
	RPT_SUCCESS
} RPT_STATE_T;


/* NOTIFY PROTECTION STATE REPORT STRUCTURE
---------------------------------*/
typedef struct __ptcsrv_state_report__t_
{
	int        s_type;             /* Service type */
	int        status;             /* Login succeeded/failed */
	char       addr[64];           /* Record address */
	char       msg[256];           /* Info */

} PTCSRV_STATE_REPORT_T;

#endif
