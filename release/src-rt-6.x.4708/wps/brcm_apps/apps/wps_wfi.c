/*
 * WPS WiFi Invite (WFI)
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_wfi.c 383924 2013-02-08 04:14:39Z $
 */

#include <typedefs.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>

#include <bcmconfig.h>
#include <shutils.h>
#include <bcmutils.h>
#include <wlutils.h>
#include <bcmnvram.h>

#include <security_ipc.h>
#include <wps_wl.h>
#include <wps_ui.h>
#include <wps_ie.h>
#include <wps_wfi.h>
#include <wps.h>
#include <sminfo.h>

#if !defined(__CONFIG_WFI__)
#error __CONFIG_WFI__ is not set
#endif /* __CONFIG_WFI__ */

#if defined(BCMDBG)
#define WPS_WFI_DEBUG
#else
#undef WPS_WFI_DEBUG
#endif /* BCMDBG */

#define WPS_WFI_MAX_IF             2
#define WPS_WFI_MAX_BSS            (16 * WPS_WFI_MAX_IF)

/* NVRAM variables */
#define WPS_WFI_ENABLE_NVNAME      "wfi_enable" /* 0: disable, 1: enable */
#define WPS_WFI_STA_TO_NVNAME      "wfi_sta_to" /* STA timeout in sec for the list */
#define WPS_WFI_INVITE_TO_NVNAME   "wfi_invite_to" /* The Invitation Timeout in sec */
#define WPS_WFI_LIST_NVNAME        "wfi_list" /* nvram containing WFI STAs */
#define WPS_WFI_PINMODE_NVNAME     "wfi_pinmode" /* 0: auto-pin, 1: manual input */

/* WFI Command and Status interface */
#define WPS_WFI_CMD_NVNAME         "wfi_cmd" /* The WFI command interface */
#define WPS_WFI_CMD_LIST           "list" /* "list unit.subunit unit.subunit ..." */
#define WPS_WFI_CMD_INVITE         "invite" /* "invite unit.subunit MACFName" */
#define WPS_WFI_CMD_CANCEL         "cancel" /* Cancel the invitation */
#if defined(WPS_WFI_DEBUG)
#define WPS_WFI_CMD_DUMP           "dump" /* Dump BSS/STA info */
#define WPS_WFI_CMD_ASSOC          "assoc" /* "List assoc STAs. */
#define WPS_WFI_CMD_DBG            "dbg" /* Set debug message mask */
#endif /* WPS_WFI_DEBUG */

/* WFI Status interface */
#define WPS_WFI_STATUS_NVNAME      "wfi_status" /* The WFI status interface */
#define WPS_WFI_STATUS_DONE        "0" /* Done. No actions. */
#define WPS_WFI_STATUS_INVITE      "1" /* Inviting a STA */
#define WPS_WFI_STATUS_WPS         "2" /* STA has joined and WPS PIN is started */

/* WFI Error codes */
#define WPS_WFI_ERROR_NVNAME       "wfi_error" /* The WFI status interface */
#define WPS_WFI_ERROR_NONE         "0" /* No Error */
#define WPS_WFI_ERROR_TIMEOUT      "1" /* STA not responding and timeout */
#define WPS_WFI_ERROR_REJECT       "2" /* STA rejects the invite */
#define WPS_WFI_ERROR_CANCEL       "3" /* AP cancels the invitation */
#define WPS_WFI_ERROR_WPS_FAIL     "4" /* WPS failure */
#define WPS_WFI_ERROR_ADD_VNDR_EXT "-1" /* Internal Error */
#define WPS_WFI_ERROR_START_WPS    "-2" /* Internal Error */

/* WFI STA info format in NVRAM */
#define WPS_WFI_STANVINFO_DELIMETER	'\t' /* Delimeter between 2 STA nvram info */
#pragma pack(push, 1)
typedef struct wps_wfi_sta_nvinfo_s {
	union {
		struct { char acMAC[17], acFName[33], acNULL[1]; };
		char acValue[1]; /* MACfname format w/o space in between */
	};
}	wps_wfi_sta_nvinfo_t;
#pragma pack(pop)

typedef struct wps_wfi_bss_s {
	int siUnit, siSubunit;

	uint uiMode;
	#define WPS_WFI_BSS_MODE_NOWFI			0x01
	#define WPS_WFI_BSS_MODE_NOWPS			0x02
	#define WPS_WFI_BSS_MODE_NOAUTOPIN	0x04

	char acOSName[IFNAMSIZ + 1];
	char acNVPrefix[IFNAMSIZ];

	void *ptVEObj;
} wps_wfi_bss_t;

typedef struct wps_wfi_sta_s {
	struct wps_wfi_sta_s *ptNext;

	int siState;
	#define WPS_WFI_STA_STATE_INVITE    0x01 /* VE is set to invit this STA */
	#define WPS_WFI_STA_STATE_WPS_START 0x02 /* The WPS is started for this STA */
	#define WPS_WFI_STA_STATE_ASSOC     0x04 /* The STA has associated */

	struct {
		brcm_wfi_ie_t stWFIData;
		char acNameBuffer[32];  /* Actual buffer for friendly name. */
	};

	time_t stUpdateTime; /* Time of last ProbeReq received */
	wps_wfi_bss_t *apProbeReqFromBSS[WPS_WFI_MAX_BSS];

}	wps_wfi_sta_t;

typedef struct wps_wfi_s {
	int biEnable; /* bool, 0: WFI is disabled */

	/* STA maintenance */
	wps_wfi_sta_t *ptSTAList; /* Contains all WFI capable STAs from all interfaces */
	wps_wfi_sta_t *ptSTABeingInvited; /* The STA that a BSS is inviting */
	time_t stInviteTime; /* Time that the Invitation starts */
	time_t stLastCheckTime; /* Time the STA list is checked */
	int siListCheckInterval; /* In sec, how often do we check the STA list */
	int siSTAIdleTimeout; /* In sec, timeout if no ProbeReq received from a STA */
	int siInviteTimeout; /* In sec, timeout for an invitation */

	/* BSS maintenance */
	wps_wfi_bss_t *ptInvitingBSS; /* The BSS that is inviting a STA */
	wps_wfi_bss_t atBSSList[WPS_WFI_MAX_BSS]; /* Enabled BSSes of this router */

#if defined(WPS_WFI_DEBUG)
	int uiDebug; /* Message debug print level */
	#define WPS_WFI_DBG_LEVEL_INFO			0x01 /* Mask to enable INFO print */
	#define WPS_WFI_DBG_LEVEL_PRBREQ		0x10 /* Mask to enable ProbeReq print */
#endif /* WPS_WFI_DEBUG */

}	wps_wfi_t;
static wps_wfi_t stWFI = { 0 }, *ptWFI = &stWFI;

/* NVRAM space */
static char *
wps_wfi_get_nvram(char *pcName)
{	return nvram_get(pcName);
}

static void
wps_wfi_set_nvram(char *pcName, char *pcValue)
{	nvram_set(pcName, pcValue);
}

static int
wps_wfi_safe_get_nvram(char *outval, int outval_size, char *name)
{
	char *val;

	if (name == NULL || outval == NULL)
		return -1;

	val = nvram_safe_get(name);
	snprintf(outval, outval_size, "%s", val);
	return 0;
}

static int
wps_wfi_match_nvram(char *pcName, char *pcToMatch)
{
	const char *value = nvram_get(pcName);
	return (value && !strcmp(value, pcToMatch));
}

/* Configuration space is read-only */
static char *
wps_wfi_get_conf(char *pcName)
{	return wps_get_conf(pcName);
}

static int
wps_wfi_match_conf(char *pcName, char *pcToMatch)
{
	const char *value = wps_get_conf(pcName);
	return (value && !strcmp(value, pcToMatch));
}

/* Run-time environment space */

static int
wps_wfi_match_ui_var(char *pcName, char *pcToMatch)
{
	const char *value = wps_ui_get_env(pcName);
	return (value && !strcmp(value, pcToMatch));
}

#if defined(WPS_WFI_DEBUG)
#define WPS_WFI_DBG(MASK)					(ptWFI->uiDebug & MASK)
#define WPS_WFI_PRINT(fmt, arg...)	do {printf(fmt, ##arg);} while (0)
#define WPS_WFI_ERROR(fmt, arg...)	do {printf(fmt, ##arg);} while (0)
#define WPS_WFI_INFO(fmt, arg...)	do {\
	if (WPS_WFI_DBG(WPS_WFI_DBG_LEVEL_INFO)) printf(fmt, ##arg); } while (0)

static void
wps_wfi_print(char *ptFunction, /* Function name */
	const char *ptFormat, ...) /* Format and message to print */
{	time_t stNow;
	struct tm stTM;
	int siLen;
	va_list ptArgv;
	char acTime[32];
	char acMessage[256];

	time(&stNow);
	stTM = *(localtime(&stNow));
	strftime(acTime, sizeof(acTime), "%j:%H:%M:%S", &stTM);
	siLen = sprintf(acMessage, "\n%s@(%s): ", ptFunction? ptFunction: __FILE__, acTime);

	/* Append trace msg to prefix. */
	va_start(ptArgv, ptFormat);
	vsprintf(acMessage + siLen, ptFormat, ptArgv);
	va_end(ptArgv);

	WPS_WFI_PRINT(acMessage);
}

#define wps_wfi_print_info(arg...)	do { \
		if (WPS_WFI_DBG(WPS_WFI_DBG_LEVEL_INFO)) \
			wps_wfi_print(__FUNCTION__, ##arg); } while (0)
#define wps_wfi_print_err(arg...)	do { \
		wps_wfi_print(__FUNCTION__, ##arg); } while (0)

#else /* WPS_WFI_DEBUG */
#define WPS_WFI_DBG(MASK)					(0)
#define WPS_WFI_PRINT(fmt, arg...)	do { printf(fmt, ##arg); } while (0)
#define WPS_WFI_ERROR(fmt, arg...)	do { printf(fmt, ##arg); } while (0)
#define WPS_WFI_INFO(fmt, arg...)

#define wps_wfi_print(arg...)
#define wps_wfi_print_err(arg...)
#define wps_wfi_print_info(arg...)
#endif /* WPS_WFI_DEBUG */

/*
*  All private Functions, if returning
*   int type: < 0 error code, >= 0 when OK.
*   pointer type: NULL for error or not applicable. !NULL if done.
*/
static int
wps_wfi_build_vendor_ext(
	char *pcData, /* Output buffer of WFI vendor extension */
	int siMax, /* Max length of pcData buffer */
	wps_wfi_bss_t *ptBSS) /* The BSS info */
{
	int siLen = 0;
	wps_vndr_ext_t *ptVE;
	brcm_wfi_ie_t *ptWFIVE;
	wps_wfi_sta_t *ptSTA;

	if (siMax < WPS_WFI_VENDOR_EXT_LEN)
		return -1;

	ptVE = (wps_vndr_ext_t *)pcData;
	memcpy(ptVE->vndr_id, BRCM_VENDOR_ID, 3);
	siLen += sizeof(*ptVE) - 1; /* Skip vndr_data[1] */

	ptWFIVE = (brcm_wfi_ie_t *)&ptVE->vndr_data;
	memset((void *)ptWFIVE, 0, sizeof(brcm_wfi_ie_t));
	siLen += sizeof(*ptWFIVE) - 1; /* Skip fname[1] */

	ptWFIVE->type = WPS_WFI_TYPE;
	ptWFIVE->version = 1;
	if (ptBSS && (ptBSS->uiMode & WPS_WFI_BSS_MODE_NOWPS))
		ptWFIVE->cap |= WPS_WFI_HOTSPOT;

	/* Is there any STA being invited to this interface? */
	ptSTA = ptWFI->ptSTABeingInvited;
	if (ptSTA && (ptBSS == ptWFI->ptInvitingBSS)) {

		/* Update PIN */
		ptWFIVE->pin = ptSTA->stWFIData.pin; /* Should be AP's Nonce! */

		memcpy(&ptWFIVE->mac_addr, &ptSTA->stWFIData.mac_addr, ETHER_ADDR_LEN);
		/* Get the Friendly name length */
		ptWFIVE->fname_len = ptSTA->stWFIData.fname_len;
		memcpy(&ptWFIVE->fname, &ptSTA->stWFIData.fname, ptWFIVE->fname_len);
		siLen += ptWFIVE->fname_len;
	}

	return siLen;
}

static int
wps_wfi_set_vendor_ext(wps_wfi_bss_t *ptBSS, /* Target BSS */
	int biActivate) /* To in/exclude WFI extension to ProbeRsp */
{
	int siTotal;
	char acVEBuffer[128];

	if (ptBSS->ptVEObj == NULL)
		return -1;

	siTotal = wps_wfi_build_vendor_ext(acVEBuffer, sizeof(acVEBuffer), ptBSS);
	wps_vndr_ext_obj_copy(ptBSS->ptVEObj, acVEBuffer, siTotal);
	wps_vndr_ext_obj_set_mode(ptBSS->ptVEObj, WPS_IE_TYPE_SET_PROBE_RESPONSE_IE, biActivate);

	return 1;
}

static int
wps_wfi_add_prb_rsp_IE_open(wps_wfi_bss_t *ptBSS)
/*
*  This is used for hotspot only. We fake a WPS IE
*  in order to carry WFI Vendor Extension.
*/
{
	uint32 ret = 0;
	BufferObj *bufObj;

	WPS_WFI_INFO("\nwps_wfi_add_prb_rsp_IE_open: ");
	bufObj = buffobj_new();
	if (bufObj == NULL)
		return WPS_ERR_OUTOFMEMORY;

	/* Create a fake WPS IE to include WFI vendor extension */

	/* Version */
	{	uint8 version = 0x10;

		tlv_serialize(WPS_ID_VERSION, bufObj, &version, WPS_ID_VERSION_S);
	}

	/* Write Vendor Extension */
	{
		int siTotal;
		char acData[64];

		siTotal = wps_wfi_build_vendor_ext(acData, sizeof(acData), ptBSS);
		tlv_serialize(WPS_ID_VENDOR_EXT, bufObj, acData, siTotal);
	}

	/* Send a pointer to the serialized data to Transport */
	ret = wps_wl_set_wps_ie(ptBSS->acOSName,
	   bufObj->pBase, bufObj->m_dataLength,
	   WPS_IE_TYPE_SET_PROBE_RESPONSE_IE, OUITYPE_WPS);

	wps_wfi_print_info("set probrsp IE on %s failed\n", ptBSS->acOSName);

	/* bufObj will be destroyed when we exit this function */
	buffobj_del(bufObj);
	return ret;
}

/* Return number of WFI-enabled BSSes */
static int
wps_wfi_init_bsslist(void)
{
	char name[IFNAMSIZ], *next = NULL;
	char wl_name[IFNAMSIZ], os_name[IFNAMSIZ];
	int index, unit = -1, subunit = -1;
	int i = 0;
	wps_wfi_bss_t *ptBSS;
	char tmp[256], prefix[] = "wlXXXXXXXXXX_";
	char brname[256], ifnames[1024] = {0};

	/* find out all wl interface, if the wps is enabled	*/
	wps_wfi_safe_get_nvram(ifnames, sizeof(ifnames), "lan_ifnames");
	for (index = 1; index <= 255; index ++) {
		sprintf(brname, "lan%d_ifnames", index);
		wps_wfi_safe_get_nvram(tmp, sizeof(tmp), brname);
		add_to_list(tmp, ifnames, sizeof(ifnames));
	}
	wps_wfi_safe_get_nvram(tmp, sizeof(tmp), "unbridged_ifnames");
	add_to_list(tmp, ifnames, sizeof(ifnames));

	foreach(name, ifnames, next) {
		if (nvifname_to_osifname(name, os_name, sizeof(os_name)) < 0)
			continue;

		if (wl_probe(os_name) ||
			wl_ioctl(os_name, WLC_GET_INSTANCE, &unit, sizeof(unit)))
			continue;

		/* Convert eth name to wl name */
		if (osifname_to_nvifname(name, wl_name, sizeof(wl_name)) != 0)
			continue;

		/* The interface prefix */
		snprintf(prefix, sizeof(prefix), "%s_", wl_name);

		get_ifname_unit(wl_name, &unit, &subunit);
		if (unit < 0)
			unit = 0;
		if (subunit < 0)
			subunit = 0;

		if (!wps_wfi_match_nvram(strcat_r(prefix, WPS_WFI_ENABLE_NVNAME, tmp), "1"))
		{	WPS_WFI_PRINT("<%s=%s>", tmp, wps_wfi_get_conf(tmp));
			continue;
		}

		ptBSS = &ptWFI->atBSSList[i];

		if (wps_wfi_match_nvram(strcat_r(prefix, "wps_mode", tmp), "enabled"))
			ptBSS->uiMode &= ~WPS_WFI_BSS_MODE_NOWPS;
		else
			ptBSS->uiMode |= WPS_WFI_BSS_MODE_NOWPS;

		if (wps_wfi_match_nvram(strcat_r(prefix, WPS_WFI_PINMODE_NVNAME, tmp), "1"))
			ptBSS->uiMode |= WPS_WFI_BSS_MODE_NOAUTOPIN;

		strncpy(ptBSS->acNVPrefix, prefix, sizeof(ptBSS->acNVPrefix) - 1);
		strncpy(ptBSS->acOSName, os_name, sizeof(ptBSS->acOSName) - 1);
		ptBSS->siUnit = unit;
		ptBSS->siSubunit = subunit;

		if (subunit == 0)
			wps_wfi_set_nvram(strcat_r(prefix, WPS_WFI_LIST_NVNAME, tmp), "");

		ptBSS->ptVEObj = wps_vndr_ext_obj_alloc(WPS_WFI_VENDOR_EXT_LEN, ptBSS->acOSName);

		/* We will add WFI vendor extension only we are to invite a STA */

		i++;
	}
	return i;
}

static wps_wfi_bss_t *
wps_wfi_find_bss(wps_wfi_sta_t *ptSTA,
	const char *pcIFPrefix)
{
	int i;
	wps_wfi_bss_t *ptBSS;

	/* Check if the STA is observed from the interface pcIFPrefix */
	for (i = 0; i < WPS_WFI_MAX_BSS; i++) {
		if ((ptBSS = ptSTA->apProbeReqFromBSS[i]) == NULL)
			break;
		if (strcmp(ptBSS->acNVPrefix, pcIFPrefix) == 0)
			return ptBSS;
	}

	return NULL;
}

static wps_wfi_bss_t *
wps_wfi_find_mbss(wps_wfi_sta_t *ptSTA,
	const char *pcIFPrefix)
{
	int i, siUnit, siSubunit;
	wps_wfi_bss_t *ptBSS;
	char acName[16];

	if (pcIFPrefix == NULL)
		return NULL;

	if ((ptBSS = wps_wfi_find_bss(ptSTA, pcIFPrefix))) {
		/* Exact match of the interface prefix */
		return ptBSS;
	}

	/* Check the physical interface */
	wps_strncpy(acName, pcIFPrefix, sizeof(acName));
	get_ifname_unit(acName, &siUnit, &siSubunit);
	if (siUnit < 0)
		return NULL;

	sprintf(acName, "wl%d_", siUnit);
	ptBSS = wps_wfi_find_bss(ptSTA, acName);
	if (ptBSS == NULL) {
		/* This STA does not come from this physical interface */
		return NULL;
	}

	/* Look for Other BSS */
	for (i = 0; i < WPS_WFI_MAX_BSS; i++) {
		if ((ptBSS = &ptWFI->atBSSList[i]) == NULL)
			break;
		if (strcmp(ptBSS->acNVPrefix, pcIFPrefix) == 0)
			return ptBSS;
	}

	return NULL;
}

static wps_wfi_sta_t *
wps_wfi_find_sta(struct ether_addr *ptAddr,
	wps_wfi_sta_t **ppPrev)
{
	wps_wfi_sta_t *ptSTA, *ptPrev = NULL;

	if (ptAddr == NULL)
		return NULL;

	for (ptSTA = ptWFI->ptSTAList; ptSTA; ptSTA = ptSTA->ptNext) {
		if (bcmp(ptAddr, &ptSTA->stWFIData.mac_addr, ETHER_ADDR_LEN) == 0)
			break;
		ptPrev = ptSTA;
	}
	if (ptSTA == NULL)
		return NULL;

	if (ppPrev != NULL)
		*ppPrev = ptPrev;

	return ptSTA;
}

static int
wps_wfi_del_sta(struct ether_addr *ptAddr)
{
	wps_wfi_sta_t *ptSTA, *ptPrev;

	ptSTA = wps_wfi_find_sta(ptAddr, &ptPrev);
	if (ptSTA == NULL || ptSTA == ptWFI->ptSTABeingInvited)
		return 0;

	if (ptSTA->siState & (WPS_WFI_STA_STATE_INVITE | WPS_WFI_STA_STATE_WPS_START)) {
		WPS_WFI_ERROR("Cannot delete the invitee(0x%04x)!", ptSTA->siState);
		return -1;
	}

	if (WPS_WFI_DBG(WPS_WFI_DBG_LEVEL_INFO)) {
		char acMAC[18];

		ether_etoa(ptSTA->stWFIData.mac_addr, acMAC);
		wps_wfi_print_info("Del STA=%s!", acMAC);
	}

	if (ptPrev)
		ptPrev->ptNext = ptSTA->ptNext;
	else
		ptWFI->ptSTAList = ptSTA->ptNext;

	free(ptSTA);
	return 1;
}

static int
wps_wfi_add_sta(struct ether_addr *ptAddr,
	const brcm_wfi_ie_t *ptWFIVE,
	const char *ptIF)
{
	int i, j;
	wps_wfi_sta_t *ptSTA;

	ptSTA = wps_wfi_find_sta(ptAddr, NULL);
	if (ptSTA == NULL) {
		if (WPS_WFI_DBG(WPS_WFI_DBG_LEVEL_INFO)) {
			char acMAC[18];

			ether_etoa(ptAddr->octet, acMAC);
			wps_wfi_print_info("Add STA=%s!", acMAC);
		}
		ptSTA = (wps_wfi_sta_t *)malloc(sizeof(*ptSTA));
		if (!ptSTA)
			return 0;
		memset(ptSTA, 0, sizeof(*ptSTA));
		memcpy(ptSTA->stWFIData.mac_addr, ptAddr->octet, ETHER_ADDR_LEN);
		ptSTA->ptNext = ptWFI->ptSTAList;
		ptWFI->ptSTAList = ptSTA;
	}

	if (ptSTA->stWFIData.fname_len != ptWFIVE->fname_len ||
		bcmp(&ptWFIVE->fname, ptSTA->stWFIData.fname, ptSTA->stWFIData.fname_len)) {
		ptSTA->stWFIData.fname_len = ptWFIVE->fname_len;
		memcpy(&ptSTA->stWFIData.fname, ptWFIVE->fname, ptWFIVE->fname_len);
		ptSTA->stWFIData.fname[ptWFIVE->fname_len] = 0;
	}
	ptSTA->stWFIData.version = ptWFIVE->version;
	ptSTA->stWFIData.cap = ptWFIVE->cap;
	ptSTA->stWFIData.pin = ptWFIVE->pin;

	/* Update timestamp */
	time(&ptSTA->stUpdateTime);

	if (ptIF == NULL)
		return 0;

	/* Record the BSS this STA is observed */
	for (i = 0; i < WPS_WFI_MAX_BSS; i++) {
		if (ptSTA->apProbeReqFromBSS[i] == NULL)
			break;
		if (strcmp(ptSTA->apProbeReqFromBSS[i]->acOSName, ptIF) == 0)
			return 1;
	}
	if (i == WPS_WFI_MAX_BSS)
		return 0;

	for (j = 0; j < WPS_WFI_MAX_BSS; j++) {
		wps_wfi_bss_t *ptBSS;

		ptBSS = &ptWFI->atBSSList[j];
		if (ptBSS->acOSName[0] == 0x00)
			break;
		if (strcmp(ptBSS->acOSName, ptIF) == 0) {
			ptSTA->apProbeReqFromBSS[i] = ptBSS;
			return 1;
		}
	}

	return -2;
}

static int
wps_wfi_nvram_stalist(char **ppBuffer,
	int *piLen,
	wps_wfi_sta_t *ptSTA)
{
	int siLen;
	wps_wfi_sta_nvinfo_t stSTAinfo;

	if (*piLen < 17 || ppBuffer == NULL)
		return -1;

	siLen = sprintf(stSTAinfo.acValue, "%02x:%02x:%02x:%02x:%02x:%02x%s",
		ptSTA->stWFIData.mac_addr[0], ptSTA->stWFIData.mac_addr[1],
		ptSTA->stWFIData.mac_addr[2], ptSTA->stWFIData.mac_addr[3],
		ptSTA->stWFIData.mac_addr[4], ptSTA->stWFIData.mac_addr[5],
		ptSTA->stWFIData.fname);
	strncpy(*ppBuffer, stSTAinfo.acValue, siLen);
	*piLen = *piLen - siLen;
	*ppBuffer = *ppBuffer + siLen;

	return 1;
}

#if defined(BCMDBG)
static void
wps_wfi_use_fixpin(wps_wfi_sta_t *ptSTA)
{
	uint32 ulPin;
	char *pcPIN = wps_wfi_get_nvram("wfi_fix_pin");

	if (pcPIN == NULL)
		return;

	wps_wfi_set_nvram("wps_sta_pin", pcPIN);
	sscanf(pcPIN, "%x", &ulPin);
	ptSTA->stWFIData.pin = htonl(ulPin);
}
#endif /* BCMCBG */

static int
wps_wfi_cleanup_wps(void)
{
	ptWFI->ptSTABeingInvited->siState &= ~WPS_WFI_STA_STATE_WPS_START;
	return 1;
}

static char *
wps_wfi_get_wps_status(void)
{
	char *pcWPSStatus, *pcErrCode = WPS_WFI_ERROR_NONE;

	/* The status of last WPS session is set in nvram */
	pcWPSStatus = wps_wfi_get_nvram("wps_proc_status");
	switch (pcWPSStatus[0]) {
	default:
	case '3': /* WPS_MSG_ERR */
		pcErrCode = WPS_WFI_ERROR_WPS_FAIL;
		break;
	case '0': /* WPS Init */
	case '4': /* WPS_TIMEOUT */
		pcErrCode = WPS_WFI_ERROR_TIMEOUT;
		break;
	case '2': /* WPS_OK */
	case '7': /* WPS_MSGDONE */
		break;
	}
	return pcErrCode;
}

static int
wps_wfi_stop_wps(void)
{
	char acBuffer[256] = "SET ";
	int uiLen = 4;

	if (ptWFI->ptSTABeingInvited == NULL)
		return 0;

	uiLen += sprintf(acBuffer + uiLen, "wps_config_command=%d ", WPS_UI_CMD_STOP);
	uiLen += sprintf(acBuffer + uiLen, "wps_action=%d ", WPS_UI_ACT_NONE);
	wps_ui_process_msg(acBuffer, uiLen);

	wps_wfi_cleanup_wps();

	wps_wfi_print_info("Done");
	return 1;
}

static int
wps_wfi_start_wps(wps_wfi_sta_t *ptSTA,
	wps_wfi_bss_t *ptBSS)
{
	char acNVName[64];
	char acBuffer[256] = "SET ";
	int uiLen = 4;

	if ((ptBSS->uiMode & WPS_WFI_BSS_MODE_NOWPS))
		return 0;

	strcat_r(ptBSS->acNVPrefix, "wps_config_state", acNVName);
	if (wps_wfi_match_conf(acNVName, "0")) {
		WPS_WFI_ERROR("%s is %s...", acNVName, wps_get_conf(acNVName));
		return -1;
	}

	if (!wps_wfi_match_ui_var("wps_config_command", "0")) {
		WPS_WFI_ERROR("wps_config_command is %s...", wps_ui_get_env("wps_config_command"));
		return -2;
	}

	uiLen += sprintf(acBuffer + uiLen, "wps_config_command=%d ", WPS_UI_CMD_START);
	uiLen += sprintf(acBuffer + uiLen, "wps_action=%d ", WPS_UI_ACT_ADDENROLLEE);
	uiLen += sprintf(acBuffer + uiLen, "wps_method=%d ", WPS_UI_METHOD_PIN);

#if defined(BCMDBG)
	/* Use fixed pin from nvram if provided for testing only */
	wps_wfi_use_fixpin(ptSTA);
#endif /* BCMDBG */

	/* Auto pin from WFI Vendor Extension. For Manual-Pin, the UI shall set it.	*/
	if ((ptBSS->uiMode & WPS_WFI_BSS_MODE_NOAUTOPIN) == 0) {
		char acStaPin[16]; /* 8 digits */

		sprintf(acStaPin, "%08x", ntohl(ptSTA->stWFIData.pin));
		uiLen += sprintf(acBuffer + uiLen, "wps_sta_pin=%s ", acStaPin);
	}

	uiLen += sprintf(acBuffer + uiLen, "wps_ifname=%s ", ptBSS->acOSName);

	wps_wfi_set_nvram("wps_proc_status", "0");
	wps_ui_process_msg(acBuffer, uiLen);

	ptSTA->siState |= WPS_WFI_STA_STATE_WPS_START;

	wps_wfi_set_nvram(WPS_WFI_STATUS_NVNAME, WPS_WFI_STATUS_WPS);

	wps_wfi_print_info("Done");
	return 1;
}

static int
wps_wfi_stop_invite(char *pcErrorCode)
{
	wps_wfi_bss_t *ptBSS;
	wps_wfi_sta_t *ptSTA = ptWFI->ptSTABeingInvited;

	if (ptSTA == NULL)
		return 0;

	ptWFI->ptSTABeingInvited = NULL;
	ptBSS = ptWFI->ptInvitingBSS;
	ptWFI->ptInvitingBSS = NULL;
	ptSTA->siState &= ~WPS_WFI_STA_STATE_INVITE;

	if (ptBSS) {
		if (ptBSS->uiMode & WPS_WFI_BSS_MODE_NOWPS) {
			wps_wl_del_wps_ie(ptBSS->acOSName,
				WPS_IE_TYPE_SET_PROBE_RESPONSE_IE, OUITYPE_WPS);
		} else {
			wps_wfi_set_vendor_ext(ptBSS, FALSE);
		}
	}

	if (wps_wfi_match_nvram(WPS_WFI_ERROR_NVNAME, ""))
		wps_wfi_set_nvram(WPS_WFI_ERROR_NVNAME, pcErrorCode);
	wps_wfi_set_nvram(WPS_WFI_STATUS_NVNAME, WPS_WFI_STATUS_DONE);

	wps_wfi_print_info("error code=%s!", wps_wfi_get_nvram(WPS_WFI_ERROR_NVNAME));

	return 1;
}

static int
wps_wfi_start_invite(wps_wfi_sta_t *ptSTA,
	wps_wfi_bss_t *ptBSS)
/*
*  This function activates WFI Vendor Extension to ProbeReq template.
*/
{
	int siReturn;

	if (ptWFI->ptSTABeingInvited != NULL) {
		WPS_WFI_ERROR("Invitation in progress... Ignored!");
		return -2;
	}

	/* Add Probe WFI Vendor Extension to Probe Response */
	ptWFI->ptInvitingBSS = ptBSS;
	ptWFI->ptSTABeingInvited = ptSTA;
	if ((ptBSS->uiMode & WPS_WFI_BSS_MODE_NOWPS))
		siReturn = wps_wfi_add_prb_rsp_IE_open(ptBSS);
	else
		siReturn = wps_wfi_set_vendor_ext(ptBSS, TRUE);

	if (siReturn < 0) {
		wps_wfi_stop_invite(WPS_WFI_ERROR_ADD_VNDR_EXT);
		WPS_WFI_ERROR("Error in setting WFI Vendor Extention to Probe Response...");
		return -2;
	}

	time(&ptWFI->stInviteTime);
	ptSTA->siState |= WPS_WFI_STA_STATE_INVITE;

	wps_wfi_set_nvram(WPS_WFI_STATUS_NVNAME, WPS_WFI_STATUS_INVITE);
	wps_wfi_print_info("Done");
	return 1;
}

#if defined(WPS_WFI_DEBUG)
static void
wps_wfi_debug(char *pcParam)
{
	if (pcParam)
		ptWFI->uiDebug = atoi(pcParam);
	WPS_WFI_PRINT("\nwps_wfi_debug: %d", ptWFI->uiDebug);
}

static void
wps_wfi_dump_BSSes(void)
{
	int i;
	wps_wfi_bss_t *ptBSS;

	WPS_WFI_PRINT("\nBSS List: unit\tmode\tname\tprefix");
	for (i = 0; i < WPS_WFI_MAX_BSS; i++) {
		ptBSS = &ptWFI->atBSSList[i];
		if (ptBSS->acOSName[0] == 0)
			break;
		WPS_WFI_PRINT("\n\t%d.%d:\t%04x\t%s\t%s", ptBSS->siUnit, ptBSS->siSubunit,
			ptBSS->uiMode, ptBSS->acOSName, ptBSS->acNVPrefix);
	}
}

static void
wps_wfi_print_sta_info(wps_wfi_sta_t *ptSTA)
{
	int i;
	char acMACString[ETHER_ADDR_STR_LEN];
	wps_wfi_bss_t *ptBSS;

	ether_etoa((uchar *)&ptSTA->stWFIData.mac_addr, acMACString);
	WPS_WFI_PRINT("\n\t%s(%s): S=%04x T=%08x P=%08x L=%d from", acMACString,
		ptSTA->stWFIData.fname, ptSTA->siState, (int)ptSTA->stUpdateTime,
		ptSTA->stWFIData.pin, ptSTA->stWFIData.fname_len);
	for (i = 0; i < WPS_WFI_MAX_BSS; i ++) {
		ptBSS = ptSTA->apProbeReqFromBSS[i];
		if (ptBSS == NULL)
			break;
		WPS_WFI_PRINT(" [%s]", ptBSS->acNVPrefix);
	}
	if (ptSTA == ptWFI->ptSTABeingInvited)
		WPS_WFI_PRINT(" Being Invited...");
}

static void
wps_wfi_dump_STAs(wps_wfi_sta_t *ptSTA)
{
	if (ptSTA) {
		wps_wfi_print_sta_info(ptSTA);
		return;
	}

	WPS_WFI_INFO("\nSTA List:");
	for (ptSTA = ptWFI->ptSTAList; ptSTA; ptSTA = ptSTA->ptNext)
		wps_wfi_print_sta_info(ptSTA);
}

static void
wps_wfi_dump(void)
{
	wps_wfi_dump_BSSes();
	wps_wfi_dump_STAs(NULL);
}
#endif /* WPS_WFI_DEBUG */

static int
wps_wfi_query_stalist(char *pcIFPrefix, int biNonAssocOnly)
/*
*  Return < 0 if error. >=0 if OK.
*  pcPrefix: Input, NULL-terminated "wlX.Y_" string.
*  biNonAssocOnly: Input, 0 to return all, otherwise return
*     Non-associated STA only.
*/
{
	int i, siSize;
	wps_wfi_sta_t *ptSTA;
	char *pcData, tmp[64], acBuffer[1024];

	if (ptWFI->biEnable == 0)
		return 0;

	if (pcIFPrefix == NULL || *pcIFPrefix == 0x00)
		return -1;

	pcData = acBuffer;
	siSize = sizeof(acBuffer);
	for (ptSTA = ptWFI->ptSTAList; ptSTA; ptSTA = ptSTA->ptNext) {
		if (biNonAssocOnly && (ptSTA->siState & WPS_WFI_STA_STATE_ASSOC))
			continue;
		for (i = 0; i < WPS_WFI_MAX_BSS; i++) {
			if (ptSTA->apProbeReqFromBSS[i] == NULL)
				break;
			if (strcmp(ptSTA->apProbeReqFromBSS[i]->acNVPrefix, pcIFPrefix) == 0) {
				/* Write to nvram */
				if (ptSTA != ptWFI->ptSTAList) {
					*pcData++ = WPS_WFI_STANVINFO_DELIMETER;
					siSize--;
				}
				if (wps_wfi_nvram_stalist(&pcData, &siSize, ptSTA) < 0)
					break;
			}
		}
		if (siSize < 17)
			break;
	}
	*pcData = 0x00;

	wps_wfi_set_nvram(strcat_r(pcIFPrefix, WPS_WFI_LIST_NVNAME, tmp), acBuffer);

	return 1;
}

static void
wps_wfi_list(char *pcParam, int biNonAssoc)
/*
*  pcParam: Input, list of interface units in "x.y" format.
*  biNonAssocOnly: Input, 0 to return all, otherwise return
*     Non-associated STA only.
*/
{
	int i;
	wps_wfi_bss_t *ptBSS;
	char acBSSUnit[32], acPrefix[32], *next = NULL;
	char acUnitList[128] = {0};

	/* Check if there's specified interfaces */

	strncpy(acUnitList, pcParam, sizeof(acUnitList) -1);
	foreach(acBSSUnit, acUnitList, next) {
		sprintf(acPrefix, "wl%s_", acBSSUnit);
		for (i = 0; i < WPS_WFI_MAX_BSS; i++) {
			ptBSS = &ptWFI->atBSSList[i];
			if (ptBSS->acOSName[0] == 0x00)
				break;
			if (strcmp(ptBSS->acNVPrefix, acPrefix) == 0) {
				/* Write STA list from the BSS to nvram. */
				wps_wfi_query_stalist(acPrefix, biNonAssoc);
			}
		}
	}
}

static int
wps_wfi_invite(char *pcParam)
/*
*  pcParam: Input, BSS unit "x.y" followed by STA
*     info in 00:00:00:00:00:00FriendlyName
*/
{
	int i;
	wps_wfi_sta_nvinfo_t stNVInfo;
	wps_wfi_sta_t *ptSTA;
	wps_wfi_bss_t *ptBSS;
	uint8 *pcSTAToInvite;
	uint8 acMACAddr[6], acBSSUnit[16], acPrefix[IFNAMSIZ], acSTAInfo[64];

	/* invite a STA to a BSS */
	i = sscanf(pcParam, "%s %s", acBSSUnit, acSTAInfo);
	if (i < 2) {
		WPS_WFI_ERROR("Invalid invite format...");
		return -1;
	}

	memcpy(stNVInfo.acMAC, acSTAInfo, sizeof(stNVInfo.acMAC));
	ether_atoe(stNVInfo.acMAC, acMACAddr);
	ptSTA = wps_wfi_find_sta((struct ether_addr *)acMACAddr, NULL);
	if (ptSTA == NULL) {
		WPS_WFI_ERROR("Invalid MAC Address or STA not found!");
		return -2;
	}

	pcSTAToInvite = strstr(pcParam, acSTAInfo);
	wps_strncpy(stNVInfo.acFName, &pcSTAToInvite[sizeof(stNVInfo.acMAC)],
		sizeof(stNVInfo.acFName)); /* Friendly Name */
	if (bcmp(stNVInfo.acFName, ptSTA->stWFIData.fname, ptSTA->stWFIData.fname_len)) {
		WPS_WFI_ERROR("STA's friendly name mismatches...");
		return -3;
	}

	/* Search for the Target BSS */
	sprintf(acPrefix, "wl%s_", acBSSUnit);
	ptBSS = wps_wfi_find_mbss(ptSTA, acPrefix);
	if (ptBSS == NULL) {
		WPS_WFI_ERROR("BSS [%s] not found...", acBSSUnit);
		return -3;
	}

	if (wps_wfi_start_invite(ptSTA, ptBSS) < 0)
		return -4;

	wps_wfi_set_nvram(WPS_WFI_ERROR_NVNAME, "");

	/* For manual PIN, the WPS shall be started after users enters the PIN */
	if (ptBSS->uiMode & WPS_WFI_BSS_MODE_NOAUTOPIN) {
		wps_wfi_set_nvram("wps_sta_pin", "");
		return 1;
	}

	/* Start WPS PIN Method */
	if (wps_wfi_start_wps(ptSTA, ptBSS) < 0) {
		WPS_WFI_ERROR("Error in starting WPS...");
		wps_wfi_stop_invite(WPS_WFI_ERROR_START_WPS);
		return -5;
	}

	return 1;
}

static int
wps_wfi_process_command(void)
{
	int siLen;
	char *pcCommand;

	pcCommand = wps_wfi_get_nvram(WPS_WFI_CMD_NVNAME);
	if (pcCommand == NULL || *pcCommand == 0)
		return 0;

	WPS_WFI_INFO("\nwps_wfi_process_command: [%s]\n", pcCommand);
	if (strncmp(pcCommand, WPS_WFI_CMD_LIST, siLen = strlen(WPS_WFI_CMD_LIST)) == 0) {
		wps_wfi_list(pcCommand + siLen + 1, TRUE);
	}
	else if (strncmp(pcCommand, WPS_WFI_CMD_INVITE, siLen = strlen(WPS_WFI_CMD_INVITE)) == 0) {
		wps_wfi_invite(pcCommand + siLen + 1); /* Skip space before parameters */
	}
	else if (strcmp(pcCommand, WPS_WFI_CMD_CANCEL) == 0) {
		wps_wfi_stop_wps();
		wps_wfi_stop_invite(WPS_WFI_ERROR_CANCEL);
	}
#if defined(WPS_WFI_DEBUG)
	else if (strncmp(pcCommand, WPS_WFI_CMD_DBG, siLen = strlen(WPS_WFI_CMD_DBG)) == 0) {
		wps_wfi_debug(pcCommand + siLen);
	}
	else if (strncmp(pcCommand, WPS_WFI_CMD_ASSOC, siLen = strlen(WPS_WFI_CMD_ASSOC)) == 0) {
		wps_wfi_list(pcCommand + siLen + 1, FALSE);
	}
	else if (strcmp(pcCommand, WPS_WFI_CMD_DUMP) == 0) {
		wps_wfi_dump();
	}
#endif /* WPS_WFI_DEBUG */

	wps_wfi_set_nvram(WPS_WFI_CMD_NVNAME, "");
	return 1;
}

static void
wps_wfi_check_stalist(void)
{
	time_t stNow;
	wps_wfi_sta_t *ptSTA, *ptPrev;

	time(&stNow);
	if (stNow > ptWFI->stLastCheckTime + ptWFI->siListCheckInterval) {
		ptWFI->stLastCheckTime = stNow;
		for (ptSTA = ptWFI->ptSTAList, ptPrev = NULL; ptSTA; ) {
			if ((stNow > ptSTA->stUpdateTime + ptWFI->siSTAIdleTimeout) &&
				(ptSTA->siState & (WPS_WFI_STA_STATE_INVITE |
				WPS_WFI_STA_STATE_WPS_START)) == 0) {
				if (ptPrev)
					ptPrev->ptNext = ptSTA->ptNext;
				else
					ptWFI->ptSTAList = ptSTA->ptNext;

				if (WPS_WFI_DBG(WPS_WFI_DBG_LEVEL_INFO)) {
					char acMAC[18];

					ether_etoa(ptSTA->stWFIData.mac_addr, acMAC);
					wps_wfi_print_info("Free STA=%s!", acMAC);
				}

				free(ptSTA);
				if (ptPrev)
					ptSTA = ptPrev->ptNext;
				else
					ptSTA = ptWFI->ptSTAList;
			}
			else {
				ptPrev = ptSTA;
				ptSTA = ptPrev->ptNext;
			}
		}
	}
}

static void
wps_wfi_check_invite(void)
{
	wps_wfi_bss_t *ptBSS = ptWFI->ptInvitingBSS;
	wps_wfi_sta_t *ptSTA = ptWFI->ptSTABeingInvited;

	if (ptSTA && ptBSS) {
		if ((ptBSS->uiMode & WPS_WFI_BSS_MODE_NOWPS) == 0) {
			if ((ptBSS->uiMode & WPS_WFI_BSS_MODE_NOAUTOPIN) &&
				(ptSTA->siState & (WPS_WFI_STA_STATE_INVITE |
				WPS_WFI_STA_STATE_WPS_START)) ==	WPS_WFI_STA_STATE_INVITE) {
				if (wps_wfi_match_ui_var("wps_sta_pin", ""))
					return;
				if (wps_wfi_start_wps(ptSTA, ptBSS) < 0) {
					WPS_WFI_ERROR("Error in starting WPS...");
					wps_wfi_stop_invite(WPS_WFI_ERROR_START_WPS);
				}
				return;
			}
			/* Poll WPS to see if the configuration has finished */
			if (wps_wfi_match_ui_var("wps_config_command", "0")) {
				wps_wfi_cleanup_wps();
				wps_wfi_stop_invite(wps_wfi_get_wps_status());
			}
		}
		else {
			/* Timeout Mechanism for Non-WSP cases */
			time_t stNow;

			time(&stNow);
			if (stNow > ptWFI->stInviteTime + ptWFI->siInviteTimeout) {
				wps_wfi_stop_invite(WPS_WFI_ERROR_TIMEOUT);
			}
		}
	}
}

/*
*  Public functions:
*/
int
wps_wfi_process_prb_req(uint8 *pcWPSIE,
	int siIELen,
	uint8 *pcAddr,
	char *pcIFname)
/*
*  Return < 0 if error; >= 0 if OK.
*  pcWPSIE: Input, WPS IE content.
*  siIELen: Input, WPS IE total length.
*  pcAddr: Input, MAC Address of the STA sending ProbeReq.
*  pcIFname: Input, Interface where the ProbeReq comes from.
*/
{
	uint16 uwTLVLen = 0;
	tlvbase_s *ptTLV;
	wps_vndr_ext_t *ptVE;
	brcm_wfi_ie_t *ptWFIVE;

	if (ptWFI->biEnable == 0)
		return 0;

	if (pcAddr == NULL)
		return -1;

	if (pcWPSIE == NULL)
		return wps_wfi_del_sta((struct ether_addr *)pcAddr);

	if (WPS_WFI_DBG(WPS_WFI_DBG_LEVEL_PRBREQ)) {
		int i;

		WPS_WFI_PRINT("\n%s: %02x:%02x:%02x:%02x:%02x:%02x@%s(L=%d)", __FUNCTION__,
			pcAddr[0], pcAddr[1], pcAddr[2], pcAddr[3], pcAddr[4], pcAddr[5],
			pcIFname, siIELen);
		for (i = 0; i < siIELen; i++) {
			if (i % 16 == 0) WPS_WFI_INFO("\n  ");
			WPS_WFI_PRINT(" %02x", pcWPSIE[i]);
		}
	}

	ptTLV = (tlvbase_s *)pcWPSIE;
	ptWFIVE = NULL;
	while (siIELen >= 4) {
		uwTLVLen = ntohs(ptTLV->m_len);
		if (ntohs(ptTLV->m_type) == WPS_ID_VENDOR_EXT) {
			ptVE = (wps_vndr_ext_t *)((uint)ptTLV + 4);
			if (bcmp(ptVE->vndr_id, BRCM_VENDOR_ID, 3) == 0) {
				ptWFIVE = (brcm_wfi_ie_t *)&ptVE->vndr_data[0];
				if (ptWFIVE->type == WPS_WFI_TYPE)
					break;
				ptWFIVE = NULL;
			}
		}
		ptTLV = (tlvbase_s *)((uint)ptTLV + uwTLVLen + 4);
		siIELen -= (uwTLVLen + 4);
	}

	if (ptWFIVE == NULL) {
		/* Delete STA that used to include WFI Vendor Extension */
		wps_wfi_del_sta((struct ether_addr *)pcAddr);
		return 0;
	}

	if (uwTLVLen < (3 + (sizeof(brcm_wfi_ie_t) - 1))) {
		WPS_WFI_ERROR("Invalid WFI Vendor Extension: len=%d", uwTLVLen);
		return -1;
	}

	if (bcmp(pcAddr, ptWFIVE->mac_addr, ETHER_ADDR_LEN)) {
		char acMAC[18];

		ether_etoa(ptWFIVE->mac_addr, acMAC);
		WPS_WFI_INFO("MAC Address %s in VE mismatches! ", acMAC);
		return -2; /* Bogus MAC Address */
	}

	return wps_wfi_add_sta((struct ether_addr *)ptWFIVE->mac_addr,
		ptWFIVE,
		pcIFname);
}

int
wps_wfi_process_sta_ind(uint8 *pcData,
	int siLen,
	uint8 *ptAddr,
	uint8 *ptIFName,
	int biJoin)
/*
*  Return < 0 if error; >= 0 if OK.
*  ptData: Input, event data.
*  siLen: Input, event data total length.
*  pcAddr: Input, MAC Address of the STA sending ProbeReq.
*  pcIFname: Input, Interface where the ProbeReq comes from.
*  biJoin: 0: leaving event; !0: joining event.
*/
{
	wps_wfi_sta_t *ptSTA;
	wps_wfi_bss_t *ptBSS;

	if (ptWFI->biEnable == 0)
		return 0;

	if ((ptSTA = wps_wfi_find_sta((struct ether_addr *)ptAddr, NULL)) == NULL)
		return 0;

	if (WPS_WFI_DBG(WPS_WFI_DBG_LEVEL_INFO)) {
		char acMAC[18];

		ether_etoa(ptAddr, acMAC);
		wps_wfi_print_info("%s %s %s!", acMAC, biJoin? "joins": "leaves", ptIFName);
	}

	if (biJoin)
		ptSTA->siState |= WPS_WFI_STA_STATE_ASSOC; /* Joining */
	else
		ptSTA->siState &= ~WPS_WFI_STA_STATE_ASSOC; /* Leaving/Reject */

	if (ptSTA != ptWFI->ptSTABeingInvited)
		return 0;

	ptBSS = ptWFI->ptInvitingBSS;
	if (ptBSS && (ptBSS->uiMode & WPS_WFI_BSS_MODE_NOWPS)) {
		wps_wfi_stop_invite(biJoin ? WPS_WFI_ERROR_NONE: WPS_WFI_ERROR_REJECT);
	}

	return 0;
}

int
wps_wfi_cleanup(void)
{
	wps_wfi_sta_t *ptSTA;
	wps_wfi_bss_t *ptBSS;
	int i;

	wps_wfi_stop_wps();
	wps_wfi_stop_invite(WPS_WFI_ERROR_NONE);
	wps_wfi_set_nvram(WPS_WFI_CMD_NVNAME, "");
	wps_wfi_set_nvram(WPS_WFI_ERROR_NVNAME, "");

	/* Free all STAs */
	while (ptWFI->ptSTAList != NULL) {
		ptSTA = ptWFI->ptSTAList;
		ptWFI->ptSTAList = ptSTA->ptNext;
		free(ptSTA);
	}

	for (i = 0; i < WPS_WFI_MAX_BSS; i++) {
		if ((ptBSS = &ptWFI->atBSSList[i]) == NULL)
			break;
		wps_vndr_ext_obj_free(ptBSS->ptVEObj);
	}

	memset(ptWFI, 0, sizeof(*ptWFI));
	return 0;
}

int
wps_wfi_rejected(void)
{
	if (ptWFI->ptSTABeingInvited == NULL)
		return 0;

	wps_wfi_stop_wps();
	wps_wfi_stop_invite(WPS_WFI_ERROR_REJECT);

	wps_wfi_print_info("Done");
	return 1;
}

int
wps_wfi_init(void)
{
	char *pcValue;

	WPS_WFI_PRINT("\n### wps_wfi_init(): ");
	wps_wfi_cleanup();

	if (wps_wfi_init_bsslist() <= 0) {
		ptWFI->biEnable = 0;
		WPS_WFI_PRINT("WFI is not enabled ###\n");
		return 0;
	}

	time(&ptWFI->stLastCheckTime);

	/* STA List check interval */
	ptWFI->siListCheckInterval = 5; /* Sec */

	/* STA Timeout w/o ProbeReq */
	pcValue = wps_get_conf(WPS_WFI_STA_TO_NVNAME);
	if (pcValue)
		ptWFI->siSTAIdleTimeout = atoi(pcValue);
	else
		ptWFI->siSTAIdleTimeout = 150;

	/* Invitation Timeout */
	pcValue = wps_get_conf(WPS_WFI_INVITE_TO_NVNAME);
	if (pcValue)
		ptWFI->siInviteTimeout = atoi(pcValue);
	else
		ptWFI->siInviteTimeout = 120;

#if defined(WPS_WFI_DEBUG)
	ptWFI->uiDebug = WPS_WFI_DBG_LEVEL_INFO;
#endif /* WPS_WFI_DEBUG */

	ptWFI->biEnable = 1;
	WPS_WFI_PRINT("WFI is enabled ###\n");

	return 0;
}

void
wps_wfi_check(void)
{
	if (ptWFI->biEnable == 0)
		return;

	wps_wfi_check_invite();
	wps_wfi_process_command();
	wps_wfi_check_stalist();
}
