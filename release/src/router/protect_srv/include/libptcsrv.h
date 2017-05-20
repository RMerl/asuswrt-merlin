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

#ifndef __libptcsrv_h__
#define __libptcsrv_h__

#include <protect_srv.h>

extern void SEND_PTCSRV_EVENT(int s_type, int status, const char *addr, const char *msg);

#ifndef RTCONFIG_NOTIFICATION_CENTER
extern void Debug2Console(const char * format, ...);
extern int isFileExist(char *fname);
extern int GetDebugValue(char *path);
#endif

#endif
