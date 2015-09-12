/*
 * WPS ENROLL header file
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_enr.h 278550 2011-08-19 01:53:37Z $
 */

#ifndef __WPS_ENR_H__
#define __WPS_ENR_H__

#define WPS_ENR_EAP_DATA_MAX_LENGTH			2048
#define WPS_ENR_EAP_READ_DATA_TIMEOUT		1

/* OS dependent APIs */
void wpsenr_osl_proc_states(int state);
int wpsenr_osl_set_wsec(int ess_id, void *credential, int mode);
int wpsenr_osl_clear_wsec(void);
int wpsenr_osl_restore_wsec(void);

#endif /* __WPS_ENR_H__ */
