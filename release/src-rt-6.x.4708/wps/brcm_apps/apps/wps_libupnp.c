/*
 * WPS libupnp
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_libupnp.c 402957 2013-05-17 08:58:04Z $
 */

#include <stdio.h>
#include <tutrace.h>
#include <time.h>
#include <wps_wl.h>
#include <wps_ui.h>
#include <shutils.h>
#include <wlif_utils.h>
#include <wps_ap.h>
#include <security_ipc.h>
#include <wps.h>
#include <sminfo.h>
#include <reg_proto.h>
#include <wps_upnp.h>
#include <wps_ie.h>
#include <wps_staeapsm.h>
#include <ap_upnp_sm.h>
#include <ap_ssr.h>

#include <upnp.h>
#include <WFADevice.h>

static upnp_attached_if *upnpif_head = 0;
static unsigned long upnpssr_time = 0;

static wps_hndl_t libupnp_hndl;
static int libupnp_retcode = WPS_CONT;
static char libupnp_out[WPS_EAPD_READ_MAX_LEN];
static int libupnp_out_len = 0;


/*
 * Base64 encoding
 * We should think about put this part in bcmcrypto, because
 * httpd, bcmupnp and wps all need it.
 */
static const char cb64[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char cd64[] =
	"|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";
/*
 * Base64 block encoding,
 * encode 3 8-bit binary bytes as 4 '6-bit' characters
 */
static void
wps_upnp_base64_encode_block(unsigned char in[3], unsigned char out[4], int len)
{
	switch (len) {
	case 3:
		out[0] = cb64[ in[0] >> 2 ];
		out[1] = cb64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
		out[2] = cb64[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ];
		out[3] = cb64[ in[2] & 0x3f ];
		break;
	case 2:
		out[0] = cb64[ in[0] >> 2 ];
		out[1] = cb64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
		out[2] = cb64[ ((in[1] & 0x0f) << 2) ];
		out[3] = (unsigned char) '=';
		break;
	case 1:
		out[0] = cb64[ in[0] >> 2 ];
		out[1] = cb64[ ((in[0] & 0x03) << 4)  ];
		out[2] = (unsigned char) '=';
		out[3] = (unsigned char) '=';
		break;
	default:
		break;
		/* do nothing */
	}
}

/*
 * Base64 encode a stream adding padding and line breaks as per spec.
 * input	- stream to encode
 * inputlen	- length of the input stream
 * target	- stream encoded with null ended.
 *
 * Returns The length of the encoded stream.
 */
static int
wps_upnp_base64_encode(unsigned char *input, const int inputlen, unsigned char *target)
{
	unsigned char *out;
	unsigned char *in;

	out = target;
	in  = input;

	if (input == NULL || inputlen == 0) {
		*out = 0;
		return 0;
	}

	while ((in+3) <= (input+inputlen)) {
		wps_upnp_base64_encode_block(in, out, 3);
		in += 3;
		out += 4;
	}

	if ((input+inputlen) - in == 1) {
		wps_upnp_base64_encode_block(in, out, 1);
		out += 4;
	}
	else {
		if ((input+inputlen)-in == 2) {
			wps_upnp_base64_encode_block(in, out, 2);
			out += 4;
		}
	}

	*out = 0;
	return (int)(out - target);
}

/* Perform wlan event notification */
void
wps_upnp_update_wlan_event(int if_instance, unsigned char *macaddr,
	char *databuf, int datalen, int init, char event_type)
{
	char *base64buf;
	char *buf;
	char *ssr_ipaddr, ipaddr[sizeof("255.255.255.255")] = {"*"};
	char *service_name = "urn:schemas-wifialliance-org:service:WFAWLANConfig";
	char *headers[4] = {0};
	char *pipaddr = ipaddr;
	int num = 0, bytes = 0;

	if (libupnp_hndl.private == NULL) {
		TUTRACE((TUTRACE_ERR, "UPnP WFADevice uninitialized\n"));
		return;
	}

	/* Fix bug in PF#3 */
	/* Allocate data buffer */
	if ((buf = (char *)malloc(datalen * 2 + 128)) == NULL)
		return;

	WPS_HexDumpAscii("==> To UPnP", UPNP_WPS_TYPE_WE, databuf, datalen);

	/* Build base64 encoded event */
	buf[bytes++] = event_type;
	sprintf(buf+bytes, "%02X:%02X:%02X:%02X:%02X:%02X",
	        macaddr[0], macaddr[1], macaddr[2], macaddr[3],
	        macaddr[4], macaddr[5]);
	bytes += 17;
	memcpy(buf+bytes, databuf, datalen);

	/* Allocate base buffer */
	if ((base64buf = (char *)malloc((datalen+bytes) * 2 + 8)) == NULL) {
		free(buf);
		return;
	}

	wps_upnp_base64_encode(buf, datalen+bytes, base64buf);

	if (databuf && datalen > WPS_MSGTYPE_OFFSET) {
		switch (databuf[WPS_MSGTYPE_OFFSET]) {
		case WPS_ID_MESSAGE_M3:
		case WPS_ID_MESSAGE_M5:
		case WPS_ID_MESSAGE_M7:
			ssr_ipaddr = ap_ssr_get_ipaddr(NULL, NULL);
			if (strlen(ssr_ipaddr) != 0)
				strcpy(ipaddr, ssr_ipaddr);
			break;

		default:
			break;
		}
	}

	memset(buf, 0, sizeof(datalen * 2 + 128));
	bytes = sprintf(buf, "WLANEvent=%s", base64buf);
	headers[num++] = buf;

	/* Append APStatus and STAStatus if init */
	if (init) {
		headers[num++] = &buf[bytes+1];
		bytes = sprintf(&buf[bytes+1], "APStatus=1");

		headers[num++] = &buf[bytes+1];
		bytes = sprintf(&buf[bytes+1], "STAStatus=1");
	}

	if (strcmp(ipaddr, "*") == 0)
		pipaddr = 0;

	gena_event_alarm((UPNP_CONTEXT *)libupnp_hndl.private, service_name,
		num, headers, pipaddr);

	/* Free buffer */
	free(base64buf);
	free(buf);
	return;
}

void
wps_upnp_update_init_wlan_event(int if_instance, char *mac, int init)
{
	uint8 message = WPS_ID_MESSAGE_ACK;
	uint8 version = WPS_VERSION;
	uint8 null_nonce[16] = {0};
	BufferObj *bufObj;

	bufObj = buffobj_new();
	if (!bufObj)
		return;

	/* Compose wps event */
	tlv_serialize(WPS_ID_VERSION, bufObj, &version,   WPS_ID_VERSION_S);
	tlv_serialize(WPS_ID_MSG_TYPE, bufObj, &message,   WPS_ID_MSG_TYPE_S);
	tlv_serialize(WPS_ID_ENROLLEE_NONCE, bufObj, null_nonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_REGISTRAR_NONCE, bufObj, null_nonce, SIZE_128_BITS);

	TUTRACE((TUTRACE_INFO, "Init WLAN event\n"));

	wps_upnp_update_wlan_event(if_instance, mac,
		bufObj->pBase, bufObj->m_dataLength, init,
		WPS_WLAN_EVENT_TYPE_EAP_FRAME);

	buffobj_del(bufObj);
	return;
}

void
wps_upnp_forward_preb_req(int if_instance, unsigned char *macaddr,
	char *databuf, int datalen)
{
	if (upnpif_head == NULL)
		return;

	TUTRACE((TUTRACE_INFO, "Forward PROBE REQUEST %02x:%02x:%02x:%02x:%02x:%02x\n",
		macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]));

	wps_upnp_update_wlan_event(if_instance, macaddr, databuf, datalen, 0,
		WPS_WLAN_EVENT_TYPE_PROBE_REQ_FRAME);

	return;
}

/*
 * Functions called by wps monitor
 */
void
wps_upnp_device_uuid(unsigned char *uuid)
{
	int i;
	char *value;
	unsigned int wps_uuid[16];

	/* Convert string to hex */
	value = wps_safe_get_conf("wps_uuid");
	sscanf(value, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		&wps_uuid[0], &wps_uuid[1], &wps_uuid[2], &wps_uuid[3],
		&wps_uuid[4], &wps_uuid[5], &wps_uuid[6], &wps_uuid[7],
		&wps_uuid[8], &wps_uuid[9], &wps_uuid[10], &wps_uuid[11],
		&wps_uuid[12], &wps_uuid[13], &wps_uuid[14], &wps_uuid[15]);

	for (i = 0; i < 16; i++)
		uuid[i] = (wps_uuid[i] & 0xff);

	return;
}

/*
 * WPS upnp ssr related functions
 */
static int
wps_upnp_get_ssr_ifname(int ess_id, char *client_ip, char *wps_ifname)
{
	unsigned char mac[6] = {0};
	unsigned char mac_list[128 * 6];
	int count;
	int i;
	char ifname[IFNAMSIZ];
	char *wlnames;
	char tmp[100];
	char *next;
	char *value;

	/* Get from arp table for ip to mac */
	if (wps_osl_arp_get(client_ip, mac) == -1)
		return -1;

	/* Only search for br0 and br1 */
	sprintf(tmp, "ess%d_wlnames", ess_id);
	wlnames = wps_safe_get_conf(tmp);

	foreach(ifname, wlnames, next) {
		/* Query wl for authenticated sta list */
		count = wps_wl_get_maclist(ifname, mac_list, (sizeof(mac_list)/6));
		if (count == -1) {
			TUTRACE((TUTRACE_ERR, "GET authe_sta_list error!!!\n"));
			return -1;
		}

		for (i = 0; i < count; i++) {
			if (!memcmp(mac, &mac_list[i*6], 6)) {
				/* WPS ifname found */
				strcpy(wps_ifname, ifname);
				TUTRACE((TUTRACE_INFO, "wps_ifname = %s\n", wps_ifname));
				return 0;
			}
		}
	}

	/*
	 * Mac address is not belong to any wireless client.
	 * Get first wps enabled interrface in the same bridge
	 */
	sprintf(tmp, "ess%d_wlnames", ess_id);
	wlnames = wps_safe_get_conf(tmp);

	/* Find first wps available non-STA interface of target bridge */
	foreach(ifname, wlnames, next) {
		sprintf(tmp, "%s_mode", ifname);
		value = wps_safe_get_conf(tmp);
		if (strcmp(value, "ap") != 0)
			continue;

		strcpy(wps_ifname, ifname);
		return 0;
	} /* foreach().... */

	return -1;
}

static int
wps_upnp_set_ssr(int ess_id, void *data, int len, char *ssr_client_ip)
{
	char wps_ifname[IFNAMSIZ] = {0};
	CTlvSsrIE ssrmsg;
	char authorizedMacs_buf[SIZE_MAC_ADDR * SIZE_AUTHORIZEDMACS_NUM] = {0};
	int authorizedMacs_len = 0;
	int scb_num;
	BufferObj *authorizedMacs_bufObj = NULL;
	unsigned long now = time(0);
	bool b_wps_version2 = FALSE;
	BufferObj *wlidcardMac_bufObj = NULL;
	BufferObj *vendorExt_bufObj = NULL;

	/* De-serialize the data to get the TLVs */
	BufferObj *bufObj = buffobj_new();
	if (!bufObj)
		return -1;

	buffobj_dserial(bufObj, (uint8 *)data, len);

	memset(&ssrmsg, 0, sizeof(CTlvSsrIE));

	/* Version */
	tlv_dserialize(&ssrmsg.version, WPS_ID_VERSION, bufObj, 0, 0);
	/* SCState */
	ssrmsg.scState.m_data = WPS_SCSTATE_CONFIGURED;
	/* Selected Registrar */
	tlv_dserialize(&ssrmsg.selReg, WPS_ID_SEL_REGISTRAR, bufObj, 0, 0);
	/* Device Password ID */
	tlv_dserialize(&ssrmsg.devPwdId, WPS_ID_DEVICE_PWD_ID, bufObj, 0, 0);
	/* Selected Registrar Config Methods */
	tlv_dserialize(&ssrmsg.selRegCfgMethods,
		WPS_ID_SEL_REG_CFG_METHODS, bufObj, 0, 0);

	/* WSC 2.0,  support WPS V2 or not */
	if (strcmp(wps_safe_get_conf("wps_version2"), "enabled") == 0)
		b_wps_version2 = TRUE;

	if (b_wps_version2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&ssrmsg.vendorExt, bufObj,
			(uint8 *)WFA_VENDOR_EXT_ID) != 0)
			goto add_wildcard_mac;

		/* Deserialize subelement */
		vendorExt_bufObj = buffobj_new();
		if (!vendorExt_bufObj) {
			buffobj_del(bufObj);
			return -1;
		}

		buffobj_dserial(vendorExt_bufObj, ssrmsg.vendorExt.vendorData,
			ssrmsg.vendorExt.dataLength);

		/* Get Version2 and AuthorizedMACs subelement */
		if (subtlv_dserialize(&ssrmsg.version2, WPS_WFA_SUBID_VERSION2,
			vendorExt_bufObj, 0, 0) == 0) {
			/* AuthorizedMACs, <= 30B */
			subtlv_dserialize(&ssrmsg.authorizedMacs,
				WPS_WFA_SUBID_AUTHORIZED_MACS,
				vendorExt_bufObj, 0, 0);
		}

		/* Add wildcard MAC when authorized mac not specified */
		if (ssrmsg.authorizedMacs.subtlvbase.m_len == 0) {
add_wildcard_mac:
			/*
			 * If the External Registrar is WSC version 1.0 it
			 * will not have included an AuthorizedMACs subelement.
			 * In this case the AP shall add the wildcard MAC Address
			 * (FF:FF:FF:FF:FF:FF) in an AuthorizedMACs subelement in
			 * Beacon and Probe Response frames
			 */
			wlidcardMac_bufObj = buffobj_new();
			if (!wlidcardMac_bufObj) {
				buffobj_del(vendorExt_bufObj);
				buffobj_del(bufObj);
				return -1;
			}

			/* Serialize the wildcard_authorizedMacs to wlidcardMac_Obj */
			subtlv_serialize(WPS_WFA_SUBID_AUTHORIZED_MACS, wlidcardMac_bufObj,
				(char *)wildcard_authorizedMacs, SIZE_MAC_ADDR);
			buffobj_Rewind(wlidcardMac_bufObj);
			/*
			 * De-serialize the wlidcardMac_Obj data to get the TLVs
			 * Do allocation, because wlidcardMac_bufObj will be freed here but
			 * ssrmsg->authorizedMacs return and used by caller.
			 */
			subtlv_dserialize(&ssrmsg.authorizedMacs, WPS_WFA_SUBID_AUTHORIZED_MACS,
				wlidcardMac_bufObj, 0, 1); /* Need to free it */
		}

		/* Get UUID-R after WFA-Vendor Extension (wpa_supplicant use this way) */
		tlv_find_dserialize(&ssrmsg.uuid_R, WPS_ID_UUID_R, bufObj, 0, 0);
	}

	/* Other... */

	TUTRACE((TUTRACE_INFO, "SSR request: reg = %x, id = %x, method = %x\n",
		ssrmsg.selReg.m_data, ssrmsg.devPwdId.m_data, ssrmsg.selRegCfgMethods.m_data));

	if (wps_upnp_get_ssr_ifname(ess_id, ssr_client_ip, wps_ifname) < 0) {
		TUTRACE((TUTRACE_ERR, "Can't find SSR interface unit, stop SSR setting!!\n"));
		return -1;
	}
	else {
		TUTRACE((TUTRACE_INFO, "SSR interface unit = %s\n", wps_ifname));
	}

	/* Update or Remove(when selreg is FALSE)  select registrar scb information */
	ap_ssr_set_scb(ssr_client_ip, &ssrmsg, wps_ifname, now);

	/* Reset ssrmsg.authorizedMacs */
	ssrmsg.authorizedMacs.m_data = NULL;
	ssrmsg.authorizedMacs.subtlvbase.m_len = 0;

	if (b_wps_version2) {
		/*
		 * Get union (WSC 2.0 page 79, 8.4.1) of information received from all Registrars,
		 * use recently authorizedMACs list and update union attriabutes to ssrmsg
		 */
		scb_num = ap_ssr_get_union_attributes(&ssrmsg, authorizedMacs_buf,
			&authorizedMacs_len);

		/*
		 * No any SSR exist, no any selreg is TRUE, set UPnP SSR time expired
		 * if it is activing
		 */
		if (scb_num == 0) {
			if (upnpssr_time)
				upnpssr_time += WPS_MAX_TIMEOUT;
			goto no_ssr;
		}

		/* Dserialize ssrmsg.authorizedMacs from authorizedMacs_buf */
		if (authorizedMacs_len) {
			authorizedMacs_bufObj = buffobj_new();
			if (authorizedMacs_bufObj) {
				/* Serialize authorizedMacs_buf to authorizedMacs_bufObj */
				subtlv_serialize(WPS_WFA_SUBID_AUTHORIZED_MACS,
					authorizedMacs_bufObj,
					authorizedMacs_buf, authorizedMacs_len);
				buffobj_Rewind(authorizedMacs_bufObj);
				/* De-serialize the authorizedMacs data to get the TLVs */
				subtlv_dserialize(&ssrmsg.authorizedMacs,
					WPS_WFA_SUBID_AUTHORIZED_MACS,
					authorizedMacs_bufObj, 0, 0);
			}
		}
	}

	/* WSC 2.0, store the first SSR happen time */
	if (upnpssr_time == 0)
		upnpssr_time = now;

	/* Update WPS IE */
	wps_ie_set(wps_ifname, &ssrmsg);

no_ssr:
	if (bufObj)
		buffobj_del(bufObj);

	if (wlidcardMac_bufObj)
		buffobj_del(wlidcardMac_bufObj);

	if (authorizedMacs_bufObj)
		buffobj_del(authorizedMacs_bufObj);

	return 0;
}

static int
wps_upnp_ssr_disconnect(int ess_id, char *ssr_client_ip)
{
	CTlvSsrIE ssrmsg;
	char wps_ifname[IFNAMSIZ];
	char authorizedMacs_buf[SIZE_MAC_ADDR * SIZE_AUTHORIZEDMACS_NUM] = {0};
	int authorizedMacs_len = 0;
	int scb_num;
	unsigned long now = time(0);
	BufferObj *authorizedMacs_bufObj = NULL;


	memset(&ssrmsg, 0, sizeof(CTlvSsrIE));

	/* Get this scb's wps_ifname */
	if (ap_ssr_get_wps_ifname(ssr_client_ip, wps_ifname) == NULL) {
		TUTRACE((TUTRACE_ERR, "Can't find SSR interface unit!!\n"));
		return -1; /* scb not exist */
	}

	/* Get this scb info first */
	ap_ssr_get_scb_info(ssr_client_ip, &ssrmsg);

	/* Set selReg FALSE to remove this scb */
	ssrmsg.selReg.m_data = 0;
	if (ap_ssr_set_scb(ssr_client_ip, &ssrmsg, NULL, now) == -1)
		return -1; /* scb not exist */

	/* Get union attributes */
	scb_num = ap_ssr_get_union_attributes(&ssrmsg, authorizedMacs_buf, &authorizedMacs_len);

	/* If scb_num is not 0 means some ER is doing SSR TRUE */
	if (scb_num == 0) {
		if (upnpssr_time)
			upnpssr_time += WPS_MAX_TIMEOUT;
		return 0;
	} else {
		/* Use newest scb's information */
		ap_ssr_get_recent_scb_info(&ssrmsg);
	}

	/* Reset ssrmsg.authorizedMacs */
	ssrmsg.authorizedMacs.m_data = NULL;
	ssrmsg.authorizedMacs.subtlvbase.m_len = 0;

	/* Dserialize ssrmsg.authorizedMacs from authorizedMacs_buf */
	if (authorizedMacs_len) {
		authorizedMacs_bufObj = buffobj_new();
		if (authorizedMacs_bufObj) {
			/*
			 * Serialize authorizedMacs_buf to
			 * authorizedMacs_bufObj
			 */
			subtlv_serialize(WPS_WFA_SUBID_AUTHORIZED_MACS,
				authorizedMacs_bufObj,
				authorizedMacs_buf,
				authorizedMacs_len);
			buffobj_Rewind(authorizedMacs_bufObj);
			/*
			 * De-serialize the authorizedMacs data
			 * to get the TLVs
			 */
			subtlv_dserialize(&ssrmsg.authorizedMacs,
				WPS_WFA_SUBID_AUTHORIZED_MACS,
				authorizedMacs_bufObj, 0, 0);
		}
	}

	TUTRACE((TUTRACE_INFO, "Update SSR infor: reg = %x, id = %x, method = %x\n",
		ssrmsg.selReg.m_data, ssrmsg.devPwdId.m_data, ssrmsg.selRegCfgMethods.m_data));

	/* Update WPS IE */
	TUTRACE((TUTRACE_INFO, "Update SSR infor on interface %s\n", wps_ifname));
	wps_ie_set(wps_ifname, &ssrmsg);

	/* Free authorizedMacs_bufObj */
	if (authorizedMacs_bufObj)
		buffobj_del(authorizedMacs_bufObj);

	return 0;
}

void
wps_upnp_clear_ssr_timer()
{
	upnpssr_time = 0;
}

void
wps_upnp_clear_ssr()
{
	wps_upnp_clear_ssr_timer();
	ap_ssr_free_all_scb();
}

int
wps_upnp_ssr_expire()
{
	unsigned long now;
	wps_app_t *wps_app = get_wps_app();

	if (!wps_app->wksp && upnpssr_time) {
		now = time(0);
		if ((now < upnpssr_time) || ((now - upnpssr_time) > WPS_MAX_TIMEOUT))
			return 1;
	}
	return 0;
}

/*
 * Function with regard to process messages
 */
static uint32
wps_upnp_create_device_info(upnp_attached_if *upnp_if)
{
	BufferObj *outMsg = NULL;
	void *databuf = upnp_if->m1_buf;
	int *datalen = &upnp_if->m1_len;
	RegData regInfo;
	DevInfo devInfo;
	char tmp[32], prefix[] = "wlXXXXXXXXXX_";
	unsigned char wps_uuid[16] = {0};
	char *pc_data;
	uint32 err = WPS_SUCCESS;
	bool b_wps_version2 = FALSE;

	/*
	 * Prepare regInfo and devInfo JUST ENOUGH for creating M1
	 */

	/* Get prefix */
	snprintf(prefix, sizeof(prefix), "%s_", upnp_if->wl_name);

	/* Create M1 */
	outMsg = buffobj_setbuf(databuf, *datalen);
	if (outMsg == NULL)
		return -1;

	memset(&regInfo, 0, sizeof(RegData));
	memset(&devInfo, 0, sizeof(DevInfo));

	regInfo.enrollee = &devInfo;
	regInfo.e_lastMsgSent = MNONE;
	regInfo.outMsg = buffobj_new();
	if (!regInfo.outMsg) {
		buffobj_del(outMsg);
		return -1;
	}

	/* WSC 2.0,  support WPS V2 or not */
	if (strcmp(wps_safe_get_conf("wps_version2"), "enabled") == 0)
		b_wps_version2 = TRUE;

	/* Version */
	devInfo.version = WPS_VERSION;

	/* Start to setup device info */
	wps_upnp_device_uuid(wps_uuid);
	memcpy(devInfo.uuid, wps_uuid, sizeof(devInfo.uuid));

	memcpy(devInfo.macAddr, upnp_if->mac, SIZE_6_BYTES);

	if (b_wps_version2) {
		/* WSC 2.0 */
		devInfo.configMethods = (WPS_CONFMET_VIRT_PBC | WPS_CONFMET_PHY_PBC |
			WPS_CONFMET_VIRT_DISPLAY);
	} else {
		devInfo.configMethods = (WPS_CONFMET_PBC | WPS_CONFMET_DISPLAY);
	}

	pc_data = wps_get_conf("wps_config_method");
	if (pc_data)
		devInfo.configMethods = (uint16)(strtoul(pc_data, NULL, 16));

	/* AUTH_TYPE_FLAGS */
	/* WSC 2.0, WPS-PSK and SHARED are deprecated.
	 * When both the Registrar and the Enrollee are using protocol version 2.0
	 * or newer, this variable can use the value 0x0022 to indicate mixed mode
	 * operation (both WPA-Personal and WPA2-Personal enabled)
	 */
	if (b_wps_version2) {
		devInfo.authTypeFlags = (uint16)(WPS_AUTHTYPE_OPEN | WPS_AUTHTYPE_WPAPSK |
			WPS_AUTHTYPE_WPA2PSK);
	} else {
		devInfo.authTypeFlags = (uint16)(WPS_AUTHTYPE_OPEN | WPS_AUTHTYPE_WPAPSK |
			WPS_AUTHTYPE_SHARED | WPS_AUTHTYPE_WPA2PSK);
	}

	/* ENCR_TYPE_FLAGS */
	/*
	 * WSC 2.0, deprecated WEP. TKIP can only be advertised on the AP when
	 * Mixed Mode is enabled (Encryption Type is 0x000c)
	 */
	if (b_wps_version2) {
		devInfo.encrTypeFlags = (uint16)(WPS_ENCRTYPE_NONE | WPS_ENCRTYPE_TKIP |
			WPS_ENCRTYPE_AES);
	} else {
		devInfo.encrTypeFlags = (uint16)(WPS_ENCRTYPE_NONE | WPS_ENCRTYPE_WEP |
			WPS_ENCRTYPE_TKIP | WPS_ENCRTYPE_AES);
	}

	devInfo.connTypeFlags = WPS_CONNTYPE_ESS;

	/* According to WPS 2.0 section "Wi-Fi Simple Configuration State"
	 * Note: The Internal Registrar waits until successful completion of the protocol
	 * before applying the automatically generated credentials to avoid an accidental
	 * transition from "Not Configured" to "Configured" in the case that a
	 * neighboring device tries to run WSC
	 */
	sprintf(tmp, "ess%d_wps_oob", upnp_if->ess_id);
	if (!strcmp(wps_safe_get_conf(tmp), "disabled") ||
	    !strcmp(wps_safe_get_conf("wps_oob_configured"), "1"))
		devInfo.scState = WPS_SCSTATE_CONFIGURED;
	else
		devInfo.scState = WPS_SCSTATE_UNCONFIGURED;

	wps_strncpy(devInfo.manufacturer, wps_safe_get_conf("wps_mfstring"),
		sizeof(devInfo.manufacturer));

	wps_strncpy(devInfo.modelName, wps_safe_get_conf("wps_modelname"),
		sizeof(devInfo.modelName));

	wps_strncpy(devInfo.modelNumber, wps_safe_get_conf("wps_modelnum"),
		sizeof(devInfo.modelNumber));

	wps_strncpy(devInfo.serialNumber, wps_safe_get_conf("boardnum"),
		sizeof(devInfo.serialNumber));

	wps_strncpy(devInfo.deviceName, wps_safe_get_conf("wps_device_name"),
		sizeof(devInfo.deviceName));

	devInfo.primDeviceCategory = WPS_DEVICE_TYPE_CAT_NW_INFRA;
	devInfo.primDeviceOui = 0x0050f204;
	devInfo.primDeviceSubCategory = WPS_DEVICE_TYPE_SUB_CAT_NW_AP;
	/*
	 * Set b_ap TRUE,
	 * For DTM1.5 /1.6 test case "WCN Wireless Compare Probe Response and M1 messages",
	 * Configuration Methods in M1 must matched in Probe Response.
	 */
	devInfo.b_ap = TRUE;

	sprintf(tmp, "ess%d_band", upnp_if->ess_id);
	pc_data = wps_safe_get_conf(tmp);
	devInfo.rfBand = atoi(pc_data);

	devInfo.osVersion = 0x80000000;

	devInfo.assocState = WPS_ASSOC_NOT_ASSOCIATED;
	devInfo.configError = WPS_ERROR_NO_ERROR;
	devInfo.devPwdId = WPS_DEVICEPWDID_DEFAULT;

	/* WSC 2.0 */
	if (b_wps_version2) {
		/* Version 2 */
		pc_data = wps_get_conf("wps_version2_num");
		devInfo.version2 = (uint8)(strtoul(pc_data, NULL, 16));

		/* Request to Enroll */
		devInfo.b_reqToEnroll = FALSE;
	}

	err = reg_proto_create_m1(&regInfo, outMsg);
	if (err != WPS_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "UPNP: Created M1 fail (%d) \n", err));
		goto error;
	}

	memcpy(upnp_if->enr_nonce, regInfo.enrolleeNonce, SIZE_128_BITS);
	if (reg_proto_BN_bn2bin(devInfo.DHSecret->priv_key,
		upnp_if->private_key) != SIZE_PUB_KEY) {
		err = RPROT_ERR_CRYPTO;
		goto error;
	}

	*datalen = outMsg->m_dataLength;

error:
	if (outMsg)
		buffobj_del(outMsg);
	if (regInfo.outMsg)
		buffobj_del(regInfo.outMsg);

	if (devInfo.DHSecret)
		DH_free(devInfo.DHSecret);

	return err;
}

int
wps_upnp_send_msg(int if_instance, char *buf, int len, int type)
{
	UPNP_WPS_CMD cmd;

	if (!buf || !len) {
		TUTRACE((TUTRACE_ERR, "Invalid Parameters\n"));
		return WPS_ERR_INVALID_PARAMETERS;
	}

	/* Construct msg */
	memset(&cmd, 0, UPNP_WPS_CMD_SIZE);

	cmd.type = type;
	cmd.length = len;

	TUTRACE((TUTRACE_INFO, "Out %s buffer Length = %d\n", __FUNCTION__, len));

	WPS_HexDumpAscii("==> To UPnP", type, buf, len);

	/*
	 * Save the data and length here, the wfa_PutMessage() and wfa_GetDeviceInfo() will
	 * call wps_libupnp_GetOutMsgLen() and wps_libupnp_GetOutMsg() to retrieve it.
	 */
	memcpy(libupnp_out, buf, len);
	libupnp_out_len = len;
	return WPS_SUCCESS;
}

char *
wps_upnp_parse_msg(char *upnpmsg, int upnpmsg_len, int *len, int *type, char *addr)
{
	UPNP_WPS_CMD *cmd = (UPNP_WPS_CMD *)upnpmsg;

	if (addr)
		strcpy(addr, cmd->dst_addr);
	if (len)
		*len = cmd->length;
	if (type)
		*type = cmd->type;

	return cmd->data;
}

/*
 * WPS upnp init/deinit functions
 */
static int
wps_upnp_if_init(char *lan_ifname, char *ifname, char *mac, int ess_id, int instance)
{
	upnp_attached_if *upnp_if;

	/* Create upnp_if entry to retrieve msg from upnp */
	if ((upnp_if = calloc(sizeof(*upnp_if), 1)) == NULL) {
		TUTRACE((TUTRACE_ERR, "Allocate upnpif fail\n"));
		return -1;
	}

	upnp_if->ess_id = ess_id;
	upnp_if->instance = instance;

	/* Save wl interface informaction */
	memcpy(upnp_if->mac, mac, SIZE_6_BYTES);
	wps_strncpy(upnp_if->wl_name, ifname, sizeof(upnp_if->wl_name));
	wps_strncpy(upnp_if->ifname, lan_ifname, sizeof(upnp_if->ifname));

	upnp_if->next = upnpif_head;
	upnpif_head = upnp_if;

	return 0;
}

static void
wps_upnp_if_deinit(upnp_attached_if *upnp_if)
{
	/* Clean up allocated data if any */
	if (upnp_if->m1_buf)
		free(upnp_if->m1_buf);
	if (upnp_if->enr_nonce)
		free(upnp_if->enr_nonce);
	if (upnp_if->private_key)
		free(upnp_if->private_key);

	/* Free it */
	free(upnp_if);
	return;
}

void
wps_upnp_init()
{
	int i, imax, instance;
	char ifname[80], tmp[100];
	char prefix[] = "wlXXXXXXXXXX_";
	char wl_mac[SIZE_MAC_ADDR];
	char *wl_hwaddr;
	char *wlnames, *next, *value;
	int num = 0;
	int wfa_port = 0;
	int wfa_adv_time = 0;
	WFA_NET *netlist = 0;
	char **lan_ifnamelist = 0;
	UPNP_CONTEXT *context;

	/* Get wfa port */
	value = wps_get_conf("wfa_port");
	if (value)
		wfa_port = atoi(value);
	if (wfa_port <= 0 || wfa_port > 65535)
		wfa_port = 1990;

	/* Get wfa adv time */
	value = wps_get_conf("wfa_adv_time");
	if (value)
		wfa_adv_time = atoi(value);
	if (wfa_adv_time <= 0) {
		/* ssdp alive notify every 10 minutes */
		wfa_adv_time = 600;
	}

	/* Get ess number */
	imax = wps_get_ess_num();

	/* Allocate wfa netlist */
	if (imax) {
		netlist = (WFA_NET *)calloc(imax + 1, sizeof(WFA_NET));
		if (netlist == NULL)
			return;
	}

	/*
	 * Get netlist lan_ifname
	 */
	for (i = 0, num = 0; i < imax; i++) {
		sprintf(tmp, "ess%d_wlnames", i);
		wlnames = wps_get_conf(tmp);
		if (wlnames == NULL)
			continue;

		/* Get ESS UPnP instance */
		sprintf(tmp, "ess%d_ap_index", i);
		value = wps_get_conf(tmp);
		if (value == NULL)
			continue;

		instance = atoi(value);

		/* Get ESS UPnP ifname */
		sprintf(tmp, "ess%d_ifname", i);
		value = wps_get_conf(tmp);
		if (value == NULL)
			continue;

		foreach(ifname, wlnames, next) {
			/* Get configured ethernet MAC address */
			snprintf(prefix, sizeof(prefix), "%s_", ifname);

			/* Get wireless interface mac address */
			wl_hwaddr = wps_get_conf(strcat_r(prefix, "hwaddr", tmp));
			if (wl_hwaddr == NULL)
				continue;

			netlist[num++].lan_ifname = value;

			break; /* break foreach */
		} /* foreach().... */
	} /* For loop */

	if (!num)
		goto err_exit;

	/*
	 * Call libupnp upnp_init
	 */
	lan_ifnamelist = (char **)calloc(num + 1, sizeof(char *));
	if (lan_ifnamelist == NULL)
		goto err_exit;

	/* Setup lan ifname list */
	for (i = 0; i < num; i++)
		lan_ifnamelist[i] = netlist[i].lan_ifname;

	WFADevice.private = (void *)netlist;

	/* Init upnp context */
	memset(&libupnp_hndl, 0, sizeof(libupnp_hndl));
	context = upnp_init(wfa_port, wfa_adv_time, lan_ifnamelist, &WFADevice);
	if (context == NULL) {
		TUTRACE((TUTRACE_ERR, "UPnP WFADevice initialization fail\n"));
		goto err_exit;
	}

	/* Add libupnp context handle */
	libupnp_hndl.type = WPS_RECEIVE_PKT_UPNP;
	libupnp_hndl.private = (void *)context;
	wps_hndl_add(&libupnp_hndl);

	/*
	 * Send initial notify wlan event
	 */
	for (i = 0, num = 0; i < imax; i++) {
		/* Get ESS UPnP instance */
		sprintf(tmp, "ess%d_ap_index", i);
		value = wps_get_conf(tmp);
		if (value == NULL)
			continue;

		sprintf(tmp, "ess%d_wlnames", i);
		wlnames = wps_get_conf(tmp);
		if (wlnames == NULL)
			continue;

		instance = atoi(value);

		/* Find first AP mode interface for wps_upnp_if_init */
		foreach(ifname, wlnames, next) {
			/* Get configured ethernet MAC address */
			snprintf(prefix, sizeof(prefix), "%s_", ifname);

			/* Get wireless interface mac address */
			wl_hwaddr = wps_get_conf(strcat_r(prefix, "hwaddr", tmp));
			if (wl_hwaddr == NULL)
				continue;

			ether_atoe(wl_hwaddr, (unsigned char *)wl_mac);

			/* Send initial notify wlan event */
			wps_upnp_update_init_wlan_event(instance, wl_mac, 0);

			wps_upnp_if_init(lan_ifnamelist[num++], ifname, wl_mac, i, instance);
			break; /* break foreach */
		} /* foreach().... */
	} /* For loop */

err_exit:

	/* Clean up */
	if (lan_ifnamelist)
		free(lan_ifnamelist);

	return;
}

void
wps_upnp_deinit()
{
	upnp_attached_if *next;

	while (upnpif_head) {
		next = upnpif_head->next;

		wps_upnp_if_deinit(upnpif_head);

		upnpif_head = next;
	}

	/* Delete libupnp context handle */
	wps_hndl_del(&libupnp_hndl);

	/* Deinit libupnp */
	if (libupnp_hndl.private) {
		upnp_deinit(libupnp_hndl.private);
		libupnp_hndl.private = 0;
	}

	/* Free private */
	if (WFADevice.private)
		free(WFADevice.private);
}

/*
 * wps_upnp_process_msg() is used to retrieve wps_libupnp_ProcessMsg
 * return code by wps_process_msg() in wps_monitor.c
 */
int
wps_upnp_process_msg(char *upnpmsg, int upnpmsg_len)
{
	int ret;

	/* Get */
	ret = libupnp_retcode;

	/* Reset */
	libupnp_retcode = WPS_CONT;

	return ret;
}

/*
 * Following wps_libupnp_ProcessMsg(), wps_libupnp_GetOutMsgLen() and
 * wps_libupnp_GetOutMsgLen() functions are invoked by WFADevice.c belong
 * to WPS module
 */
int
wps_libupnp_ProcessMsg(char *ifname, char *upnpmsg, int upnpmsg_len)
{
	int ret = WPS_CONT;
	unsigned long now = time(0);
	char *data;
	int data_len, type;
	char peer_addr[sizeof("255.255.255.255")] = {0};
	upnp_attached_if *upnp_if;
	wps_app_t *wps_app = get_wps_app();
	bool b_wps_version2 = FALSE;


	/* WSC 2.0,  support WPS V2 or not */
	if (strcmp(wps_safe_get_conf("wps_version2"), "enabled") == 0)
		b_wps_version2 = TRUE;

	/* Find upnp_if by instance index */
	upnp_if = upnpif_head;
	while (upnp_if) {
		if (strcmp(ifname, upnp_if->ifname) == 0)
			break;
		upnp_if = upnp_if->next;
	}
	if (upnp_if == NULL) {
		TUTRACE((TUTRACE_ERR, "upnp interface instance not exist!!\n"));
		goto out;
	}

	/* Forward upnp message if wps_ap exists */
	if (wps_app->wksp) {
		if (upnp_if->ess_id != ((wpsap_wksp_t *)wps_app->wksp)->ess_id) {
			TUTRACE((TUTRACE_ERR, "Expect UPnP from ESS %d, rather than ESS %d\n",
				((wpsap_wksp_t *)wps_app->wksp)->ess_id,
				upnp_if->ess_id));
			goto out;
		}

		ret = (*wps_app->process)(wps_app->wksp, upnpmsg, upnpmsg_len,
			TRANSPORT_TYPE_UPNP_DEV);
		goto out;
	}

	if (!(data = wps_upnp_parse_msg(upnpmsg, upnpmsg_len, &data_len,
		&type, peer_addr))) {
		TUTRACE((TUTRACE_ERR, "Received data without valid message "
			"from upnp\n"));
		goto out;
	}

	TUTRACE((TUTRACE_INFO, "In %s buffer Length = %d\n", __FUNCTION__, data_len));

	WPS_HexDumpAscii("<== From UPnP", type, data, data_len);

	switch (type) {
	case UPNP_WPS_TYPE_SSR:
		TUTRACE((TUTRACE_INFO, "receive SSR request\n"));
		wps_upnp_set_ssr(upnp_if->ess_id, data, data_len, peer_addr);
		break;

	case UPNP_WPS_TYPE_GDIR:
		TUTRACE((TUTRACE_INFO, "receive GDIR request\n"));

		/*
		  * Create M1 message.
		  * Note: we don't maintain per client nounce and private key.  Within 5 seconds,
		  *          the nounce, private key and M1 will keep unchanged.
		  */
		if (upnp_if->m1_buf == NULL || (now - upnp_if->m1_built_time) >= 5) {
			/* Allocate nonce and private key buffer */
			if (upnp_if->enr_nonce == NULL) {
				upnp_if->enr_nonce = (char *)malloc(SIZE_128_BITS);
				if (upnp_if->enr_nonce == NULL)
					break;
			}

			if (upnp_if->private_key == NULL) {
				upnp_if->private_key = (char *)malloc(SIZE_PUB_KEY);
				if (upnp_if->private_key == NULL)
					break;
			}


			if (upnp_if->m1_buf == NULL) {
				upnp_if->m1_buf = (char *)malloc(WPS_EAPD_READ_MAX_LEN);
				if (upnp_if->m1_buf == NULL)
					break;
			}

			upnp_if->m1_len = WPS_EAPD_READ_MAX_LEN;
			memset(upnp_if->m1_buf, 0, WPS_EAPD_READ_MAX_LEN);

			/* Generate device info */
			if (wps_upnp_create_device_info(upnp_if) != WPS_SUCCESS) {
				free(upnp_if->m1_buf);
				upnp_if->m1_buf = 0;
				break;
			}

			upnp_if->m1_built_time = now;
		}

		/* Send out device info */
		wps_upnp_send_msg(upnp_if->instance,
			upnp_if->m1_buf, upnp_if->m1_len, UPNP_WPS_TYPE_GDIR);
		break;

	case UPNP_WPS_TYPE_PMR:
		TUTRACE((TUTRACE_INFO, "receive PMR request\n"));

		/* Weak-up wps_ap to process external registrar configuring */
		TUTRACE((TUTRACE_INFO, "wps_ap is unavaliabled!! Wake-up wps_ap now !!\n"));

		/* Open ap session, WSC 2.0 */
		ret = wpsap_open_session(wps_app, SCMODE_AP_ENROLLEE, (unsigned char *)upnp_if->mac,
			NULL, upnp_if->wl_name, upnp_if->enr_nonce, upnp_if->private_key,
			NULL, 0, 0, 0);

		if (ret == WPS_CONT) {
			TUTRACE((TUTRACE_INFO, "wps_ap is ready. !!send PMR request to "
				"wps_ap. data len = %d\n", upnpmsg_len));

			ret = (*wps_app->process)(wps_app->wksp, upnpmsg, upnpmsg_len,
				TRANSPORT_TYPE_UPNP_DEV);
		}
		else {
			TUTRACE((TUTRACE_INFO, "wps_ap opening Fail !!"));
		}

		free(upnp_if->enr_nonce);
		upnp_if->enr_nonce = NULL;
		free(upnp_if->private_key);
		upnp_if->private_key = NULL;
		free(upnp_if->m1_buf);
		upnp_if->m1_buf = NULL;

		break;

	/* WSC 2.0 */
	case UPNP_WPS_TYPE_DISCONNECT:
		if (b_wps_version2 && ap_ssr_get_scb_num())
			wps_upnp_ssr_disconnect(upnp_if->ess_id, peer_addr);

		break;

	default:
		break;
	}

out:
	libupnp_retcode = ret;

	return ret;
}

int
wps_libupnp_GetOutMsgLen(char *ifname)
{
	return libupnp_out_len;
}

char *
wps_libupnp_GetOutMsg(char *ifname)
{
	if (libupnp_out_len == 0)
		return NULL;

	/* Take message and reset the len */
	libupnp_out_len = 0;
	return libupnp_out;
}
