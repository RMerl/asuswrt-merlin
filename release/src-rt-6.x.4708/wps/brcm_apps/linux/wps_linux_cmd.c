/*
 * WPS main (platform dependent portion)
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_linux_cmd.c 383924 2013-02-08 04:14:39Z $
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>

#include <typedefs.h>
#include <security_ipc.h>
#include <wps_ui.h>

static int wps_hwaddr_check(char *value);

static int
set_wps_env(char *uibuf)
{
	int wps_fd = -1;
	struct sockaddr_in to;
	int sentBytes = 0;
	uint32 uilen = strlen(uibuf);

	if ((wps_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		goto exit;
	}

	/* send to WPS */
	to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	to.sin_family = AF_INET;
	to.sin_port = htons(WPS_UI_PORT);

	sentBytes = sendto(wps_fd, uibuf, uilen, 0, (struct sockaddr *) &to,
		sizeof(struct sockaddr_in));

	if (sentBytes != uilen) {
		goto exit;
	}

	/* Sleep 100 ms to make sure WPS have received socket */
	sleep(1);
	close(wps_fd);
	return 0;

exit:
	if (wps_fd > 0)
		close(wps_fd);

	/* Show error message ?  */
	return -1;
}


static void
usage(char *prog)
{
	fprintf(stderr,
		"usage:\n"
		"	%s stop\n"
		"	%s configap [ifname]\n"
		"	%s addenrollee [ifname] pbc\n"
		"	%s addenrollee [ifname] sta_pin=[pin] [mac]\n"
		"	%s enroll [ifname] [pbc|pin] [ssid] [bssid] [wsec]\n"
		"	%s enroll [ifname] pbc\n"
		"	%s enroll [ifname] scan\n",
		prog, prog, prog, prog, prog, prog, prog);
}

/*
 * Do wps commands
 */
static void
wps_stop()
{
	int uilen;
	char uibuf[256];

	uilen = 0;
	uibuf[0] = 0;

	uilen += sprintf(uibuf, "SET ");
	uilen += sprintf(uibuf + uilen, "wps_config_command=%d ", WPS_UI_CMD_STOP);
	uilen += sprintf(uibuf + uilen, "wps_action=%d ", WPS_UI_ACT_NONE);
	set_wps_env(uibuf);
	return;
}

static void
wps_configap(char *ifname)
{
	int uilen;
	char uibuf[256];

	uilen = 0;
	uibuf[0] = 0;

	uilen += sprintf(uibuf, "SET ");
	uilen += sprintf(uibuf + uilen, "wps_config_command=%d ", WPS_UI_CMD_START);
	uilen += sprintf(uibuf + uilen, "wps_action=%d ", WPS_UI_ACT_CONFIGAP);
	uilen += sprintf(uibuf + uilen, "wps_ifname=%s ", ifname);
	set_wps_env(uibuf);
	return;
}

static int
wps_pin_check(char *pin_string)
{
	unsigned long PIN = strtoul(pin_string, NULL, 10);
	unsigned long int accum = 0;
	unsigned int len = strlen(pin_string);

	if (len != 4 && len != 8)
		return 	-1;

	if (len == 8) {
		accum += 3 * ((PIN / 10000000) % 10);
		accum += 1 * ((PIN / 1000000) % 10);
		accum += 3 * ((PIN / 100000) % 10);
		accum += 1 * ((PIN / 10000) % 10);
		accum += 3 * ((PIN / 1000) % 10);
		accum += 1 * ((PIN / 100) % 10);
		accum += 3 * ((PIN / 10) % 10);
		accum += 1 * ((PIN / 1) % 10);

		if ((accum % 10) == 0)
			return 0;
	}
	else if (len == 4)
		return 0;

	return -1;
}

static int
wps_addenrollee(char *ifname, char *method, char *autho_sta_mac)
{
	int uilen;
	char uibuf[256];
	char pin[9] = {0};
	char *value;

	uilen = 0;
	uibuf[0] = 0;

	uilen += sprintf(uibuf, "SET ");
	uilen += sprintf(uibuf + uilen, "wps_config_command=%d ", WPS_UI_CMD_START);
	uilen += sprintf(uibuf + uilen, "wps_action=%d ", WPS_UI_ACT_ADDENROLLEE);
	uilen += sprintf(uibuf + uilen, "wps_ifname=%s ", ifname);

	if (!strcmp(method, "pbc")) {
		strcpy(pin, "00000000");
		uilen += sprintf(uibuf + uilen, "wps_method=%d ", WPS_UI_METHOD_PBC);
	}
	else if (memcmp(method, "sta_pin=", 8) == 0) {
		value = method+8;
		if (wps_pin_check(value) != 0)
			return -1;

		strncpy(pin, value, 8);
		uilen += sprintf(uibuf + uilen, "wps_method=%d ", WPS_UI_METHOD_PIN);

		/* WSC 2.0, authorized mac */
		if (autho_sta_mac) {
			if (wps_hwaddr_check(autho_sta_mac) != 0)
				return -1;
			uilen += sprintf(uibuf + uilen, "wps_autho_sta_mac=%s ", autho_sta_mac);
		}
	}
	else {
		return -1;
	}

	uilen += sprintf(uibuf + uilen, "wps_sta_pin=%s ", pin);
	uilen += sprintf(uibuf + uilen, "wps_pbc_method=%d ", WPS_UI_PBC_SW);
	set_wps_env(uibuf);
	return 0;
}

static int
wps_ether_atoe(const char *a, unsigned char *e)
{
#define ETHER_ADDR_LEN	6

	char *c = (char *) a;
	int i = 0;

	memset(e, 0, ETHER_ADDR_LEN);
	for (;;) {
		e[i++] = (unsigned char) strtoul(c, &c, 16);
		if (!*c++ || i == ETHER_ADDR_LEN)
			break;
	}
	return (i == ETHER_ADDR_LEN);
}

static int
wps_hwaddr_check(char *value)
{
	unsigned char hwaddr[6];

	/* Check for bad, multicast, broadcast, or null address */
	if (!wps_ether_atoe(value, hwaddr) ||
	    (hwaddr[0] & 1) ||
	    (hwaddr[0] & hwaddr[1] & hwaddr[2] & hwaddr[3] & hwaddr[4] & hwaddr[5]) == 0xff ||
	    (hwaddr[0] | hwaddr[1] | hwaddr[2] | hwaddr[3] | hwaddr[4] | hwaddr[5]) == 0x00) {
		return -1;
	}

	return 0;
}

static int
wps_enrollee(char *ifname, char *method, char *ssid, char *bssid, char *wsc)
{
	int uilen;
	char uibuf[256];
	int privacy;

	uilen = 0;
	uibuf[0] = 0;

	uilen += sprintf(uibuf, "SET ");
	uilen += sprintf(uibuf + uilen, "wps_config_command=%d ", WPS_UI_CMD_START);
	uilen += sprintf(uibuf + uilen, "wps_action=%d ", WPS_UI_ACT_ENROLL);
	uilen += sprintf(uibuf + uilen, "wps_ifname=%s ", ifname);

	/* Scan/Method */
	if (!strcmp(method, "scan")) {
		uilen += sprintf(uibuf + uilen, "wps_enr_scan=%d ", 1);
		set_wps_env(uibuf);
		return 0;
	}

	else if (!strcmp(method, "pin"))
		uilen += sprintf(uibuf + uilen, "wps_method=%d ", WPS_UI_METHOD_PIN);
	else if (!strcmp(method, "pbc"))
		uilen += sprintf(uibuf + uilen, "wps_method=%d ", WPS_UI_METHOD_PBC);
	else
		return -1;

	uilen += sprintf(uibuf + uilen, "wps_pbc_method=%d ",
		ssid ? WPS_UI_PBC_SW : WPS_UI_PBC_HW);

	if (ssid) {
		/* ssid */
		if (strlen(ssid) == 0 || strlen(ssid) > 32)
			return -1;

		if (wps_hwaddr_check(bssid) != 0)
			return -1;

		privacy = atoi(wsc);
		if (privacy != 0 && privacy != 1)
			return -1;

		uilen += sprintf(uibuf + uilen, "wps_enr_ssid=%s ", ssid);
		uilen += sprintf(uibuf + uilen, "wps_enr_bssid=%s ", bssid);
		uilen += sprintf(uibuf + uilen, "wps_enr_wsec=%d ", privacy);
	}
	set_wps_env(uibuf);
	return 0;
}


/*
 * Name        : main
 * Description : Main entry point for the WPS monitor
 * Arguments   : int argc, char *argv[] - command line parameters
 * Return type : int
 */
int
main(int argc, char *argv[])
{
	char *prog;
	char *command;
	char *ifname;
	char *method;
	char *ssid = 0;
	char *bssid = 0;
	char *wsec = 0;
	char *autho_sta_mac = 0; /* WSC 2.0 */

	/* Skip program name */
	--argc;
	prog = *argv;
	++argv;

	if (!*argv)
		goto out;

	/*
	  * Process wps command
	  */
	command = *argv;
	++argv;
	if (!command ||
		(strcmp(command, "stop") != 0 &&
		strcmp(command, "configap") != 0 &&
		strcmp(command, "addenrollee") != 0 &&
		strcmp(command, "enroll") != 0)) {
		/* Print usage and exit */
		goto out;
	}

	/* Do "wps stop" */
	if (!strcmp(command, "stop")) {
		wps_stop();
		return 0;
	}

	/*
	  * Process ifname
	  */
	ifname = *argv;
	++argv;
	if (!ifname)
		goto out;

	/* Do "wps configap ifname" */
	if (!strcmp(command, "configap")) {
		wps_configap(ifname);
		return 0;
	}

	/*
	  * Process method
	  */
	method = *argv;
	++argv;
	if (!method)
		goto out;

	/* Do "wps addenrollee [pbc|sta_pin=[pin]]" */
	if (!strcmp(command, "addenrollee")) {
		autho_sta_mac = *argv;
		++argv;
		if (wps_addenrollee(ifname, method, autho_sta_mac) != 0)
			goto out;

		return 0;
	}

	/*
	  * Process enroll mode
	  */
	if (strcmp(method, "scan") != 0) {
		ssid = *argv;
		++argv;
		if (!ssid) {
			if (strcmp(method, "pbc") == 0)
				goto done; /* Hardware push button */
			else
				goto out;
		}

		bssid = *argv;
		++argv;
		if (!bssid)
			goto out;

		wsec = *argv;
		++argv;
		if (!wsec)
			goto out;
	}

done:
	if (!strcmp(command, "enroll")) {
		if (wps_enrollee(ifname, method, ssid, bssid, wsec) != 0)
			goto out;

		return 0;
	}

out:
	usage(prog);
	return -1;
}
