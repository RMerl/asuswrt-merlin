/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*  Copyright 2001, ASUSTeK Inc.
    All Rights Reserved.
    
    This is UNPUBLISHED PROPRIETARY SOURCE CODE of ASUSTeK Inc.;
    the contents of this file may not be disclosed to third parties, copied or
    duplicated in any form, in whole or in part, without the prior written
    permission of ASUSTeK Inc..
*/
/*
 * NVRAM variable manipulation
 *
 * Copyright (C) 2001 ASUSTeK Inc.
 *
 * $Id: common.h,v 1.1 2007/06/08 10:20:42 arthur Exp $
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

// 2008.10 magic {
enum{
	RESTART_FTPSAMBA = 0x1,
	RESTART_DDNS = 0x2,
	RESTART_HTTPD = 0x4,
	RESTART_DNS = 0x8,
	RESTART_DHCPD = 0x10,
	RESTART_UPNP = 0x20,
	RESTART_QOS = 0x40,
	RESTART_SYSLOG = 0x80,
	RESTART_NETWORKING = 0x100,
	RESTART_FIREWALL = 0x200,
	RESTART_NTPC = 0x400,
	RESTART_NAS= 0x800,
	RESTART_NASDOCK= 0x1000,
	RESTART_REBOOT= 0x2000,
	RESTART_TIME= 0x4000,
        RESTART_RSTATS = 0x40000, 	//Viz add 2010.08
#ifdef WSC
	RESTART_WPS = 0x8000,
#endif
	RESTART_APCLI = 0x10000,
	RESTART_DMS = 0x20000
};

#define RESTART_ALL (RESTART_NETWORKING|RESTART_FTPSAMBA)

#define RESTART_NONE 0

//Each service restart needed time
#define ITVL_RESTART_FTPSAMBA 3
#define ITVL_RESTART_DDNS 1
#define ITVL_RESTART_HTTPD 3
#define ITVL_RESTART_DNS 1
#define ITVL_RESTART_DHCPD 1
#define ITVL_RESTART_UPNP 1
#define ITVL_RESTART_DMS 3
#define ITVL_RESTART_QOS 3
#define ITVL_RESTART_SYSLOG 1
//#define ITVL_RESTART_NETWORKING 13	// just restart rc
#define ITVL_RESTART_NETWORKING 60	// reboot
#define ITVL_RESTART_FIREWALL 1
#define ITVL_RESTART_NTPC 1 
#define ITVL_RESTART_NAS 3
#define ITVL_RESTART_NASDOCK 1
#define ITVL_RESTART_REBOOT 60
#define ITVL_RESTART_TIME 3
#define ITVL_RESTART_RSTATS 3		//Viz add 2010.08
#ifdef WSC
#define ITVL_RESTART_WPS 6
#endif
#define ITVL_RESTART_APCLI 1
// 2008.10 magic }


struct variable {
	char *name;
	char *longname;
	int (*validate)(char *value, struct variable *v);
	char **argv;	
	int nullok;
	int event;
};

struct action
{
      char *name;
      char **in_name;
      char **in_var;
      char **out_name;
      char **out_var; 
      char **group_tag;     
      int (*callback)(void);
};

struct svcLink
{
      char *serviceId;
      char *serviceType;
      struct variable *variables;
      struct action *actions;
};

#define ARGV(args...) ((char *[]) { args, NULL })


#ifdef __cplusplus
extern "C" {
#endif

/* API export for UPnP function */
void InitVariables(void);
int LookupServiceId(char *serviceId);
char *GetServiceId(int sid);
struct action *GetActions(int sid);
struct variable *GetVariables(int sid);
char *GetServiceType(int sid);
struct action *CheckActions(int sid, char *name);
int CheckVariables(int sid, char *name, char *var);
char *GetVariable(int sid, char *name);
void SetVariable(int sid, char *name, char *value);

struct variable *LookupGroupVariables(int sid, char *groupName);
int CheckGroupVariables(int sid, struct variable *gvs, char *name, char *var);
void SetGroupVariable(int sid, struct variable *gvs, char *name, char *value, char *action);
char *GetGroupVariable(int sid, struct variable *gvs, char *name);


#ifdef __cplusplus
}
#endif

#endif /* _COMMON_H_ */
