/*
 * Device config
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: dev_config.c 383924 2013-02-08 04:14:39Z $
 */

#include <string.h>
#include <stdlib.h>

#include <wpsheaders.h>
#include <wpscommon.h>
#include <wpserror.h>
#include <info.h>
#include <portability.h>
#include <tutrace.h>
#include <wps_enrapi.h>
#include <reg_protomsg.h>

/* Functions for devinfo */
DevInfo *
devinfo_new()
{
	DevInfo *dev_info;

	dev_info = (DevInfo *)alloc_init(sizeof(DevInfo));
	if (!dev_info) {
		TUTRACE((TUTRACE_INFO, "Could not create deviceInfo\n"));
		return NULL;
	}

	/* Initialize other member variables */
	dev_info->assocState = WPS_ASSOC_NOT_ASSOCIATED;
	dev_info->configError = 0; /* No error */
	dev_info->devPwdId = WPS_DEVICEPWDID_DEFAULT;

	return dev_info;
}

void
devinfo_delete(DevInfo *dev_info)
{
	if (dev_info) {
		if (dev_info->DHSecret)
			DH_free(dev_info->DHSecret);
		if (dev_info->mp_tlvEsM7Ap)
			reg_msg_es_del(dev_info->mp_tlvEsM7Ap, 0);
		if (dev_info->mp_tlvEsM7Sta)
			reg_msg_es_del(dev_info->mp_tlvEsM7Sta, 0);
		if (dev_info->mp_tlvEsM8Ap)
			reg_msg_es_del(dev_info->mp_tlvEsM8Ap, 0);
		if (dev_info->mp_tlvEsM8Sta)
			reg_msg_es_del(dev_info->mp_tlvEsM8Sta, 0);

		free(dev_info);
	}
	return;
}

uint16
devinfo_getKeyMgmtType(DevInfo *dev_info)
{
	char *pmgmt = dev_info->keyMgmt;

	if (!strcmp(pmgmt, "WPA-PSK"))
		return WPS_WL_AKM_PSK;
	else if (!strcmp(pmgmt, "WPA2-PSK"))
		return WPS_WL_AKM_PSK2;
	else if (!strcmp(pmgmt, "WPA-PSK WPA2-PSK"))
		return WPS_WL_AKM_BOTH;

	return WPS_WL_AKM_NONE;
}
