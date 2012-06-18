/*
 * WPS environment variables
 *
 * Copyright (C) 2011, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * $Id: wps_ui.h 276844 2011-08-11 14:04:11Z $
 */

#ifndef __WPS_UI_H__
#define __WPS_UI_H__

/*
 * WPS module
 */
#define WPS_EAP_ADDR			"127.0.0.1"
#define WPS_UPNPDEV_ADDR		"127.0.0.1"
#define WPS_UI_ADDR				"127.0.0.1"

#define WPS_UPNPDEV_PORT		40000
#define WPS_UI_PORT			40500

/* WPS_UI definitions */
#define WPS_UI_CMD_NONE			0
#define WPS_UI_CMD_START		1
#define WPS_UI_CMD_STOP			2

#define WPS_UI_METHOD_NONE		0
#define WPS_UI_METHOD_PIN		1
#define WPS_UI_METHOD_PBC		2

#define WPS_UI_ACT_NONE			0
#define WPS_UI_ACT_ENROLL		1
#define WPS_UI_ACT_CONFIGAP		2
#define WPS_UI_ACT_ADDENROLLEE		3
#define WPS_UI_ACT_STA_CONFIGAP		4
#define WPS_UI_ACT_STA_GETAPCONFIG	5

#define WPS_UI_PBC_NONE			0
#define WPS_UI_PBC_HW			1
#define WPS_UI_PBC_SW			2

int wps_ui_is_pending();
void wps_ui_clear_pending();
int wps_ui_pending_expire();
int wps_ui_init();
void wps_ui_deinit();
char *wps_ui_get_env(char *name);
void wps_ui_set_env(char *name, char *value);
void wps_ui_reset_env();
int wps_ui_process_msg(char *buf, int buflen);

#ifdef WPS_ADDCLIENT_WWTP
int wps_ui_is_SET_cmd(char *buf, int buflen);
void wps_ui_close_addclient_window();
#endif

#endif	/* __WPS_UI_H__ */
