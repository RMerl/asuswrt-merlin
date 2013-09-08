/*
 * Broadcom 802.11 device interface
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_wl.c 383924 2013-02-08 04:14:39Z $
 */

#include <typedefs.h>
#include <unistd.h>
#include <string.h>

#include <bcmutils.h>
#include <wlutils.h>
#include <wlioctl.h>

#ifdef __linux__
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/types.h>
#include <linux/filter.h>
#endif

#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <net/if.h>

#include <wpscommon.h>
#include <wpsheaders.h>
#include <tutrace.h>
#include <wps_wl.h>
#include <shutils.h>
#include <wps.h>

static int
wps_wl_set_key(char *wl_if, unsigned char *sta_mac, uint8 *key, int len, int index, int tx)
{
	wl_wsec_key_t wep;
#ifdef _TUDEBUGTRACE
	char eabuf[ETHER_ADDR_STR_LEN];
#endif
	char ki[] = "index XXXXXXXXXXX";

	memset(&wep, 0, sizeof(wep));
	wep.index = index;
	if (sta_mac)
		memcpy(&wep.ea, sta_mac, ETHER_ADDR_LEN);
	else {
		wep.flags = tx ? WL_PRIMARY_KEY : 0;
		snprintf(ki, sizeof(ki), "index %d", index);
	}

	wep.len = len;
	if (len)
		memcpy(wep.data, key, MIN(len, DOT11_MAX_KEY_SIZE));

	TUTRACE((TUTRACE_INFO, "%s, flags %x, len %d",
		sta_mac ? (char *)ether_etoa(sta_mac, eabuf) : ki,
		wep.flags, wep.len));

	return wl_ioctl(wl_if, WLC_SET_KEY, &wep, sizeof(wep));
}

#ifdef _TUDEBUGTRACE
static char *
_pktflag_name(unsigned int pktflag)
{
	if (pktflag == VNDR_IE_BEACON_FLAG)
		return "Beacon";
	else if (pktflag == VNDR_IE_PRBRSP_FLAG)
		return "Probe Resp";
	else if (pktflag == VNDR_IE_ASSOCRSP_FLAG)
		return "Assoc Resp";
	else if (pktflag == VNDR_IE_AUTHRSP_FLAG)
		return "Auth Resp";
	else if (pktflag == VNDR_IE_PRBREQ_FLAG)
		return "Probe Req";
	else if (pktflag == VNDR_IE_ASSOCREQ_FLAG)
		return "Assoc Req";
	else if (pktflag == VNDR_IE_CUSTOM_FLAG)
		return "Custom";
	else
		return "Unknown";
}
#endif /* _TUDEBUGTRACE */

static int
wps_wl_del_vndr_ie(char *wl_if, char *bufaddr, int buflen, uint32 frametype)
{
	int iebuf_len;
	int iecount, err;
	vndr_ie_setbuf_t *ie_setbuf;
#ifdef _TUDEBUGTRACE
	int i;
	int frag_len = buflen - 6;
	unsigned char *frag = (unsigned char *)(bufaddr + 6);
	int wps_msglevel = wps_tutrace_get_msglevel();
#endif

	iebuf_len = buflen + sizeof(vndr_ie_setbuf_t) - sizeof(vndr_ie_t);
	ie_setbuf = (vndr_ie_setbuf_t *) malloc(iebuf_len);
	if (!ie_setbuf) {
		TUTRACE((TUTRACE_ERR, "memory alloc failure\n"));
		return -1;
	}

	/* Copy the vndr_ie SET command ("add"/"del") to the buffer */
	strcpy(ie_setbuf->cmd, "del");

	/* Buffer contains only 1 IE */
	iecount = 1;
	memcpy(&ie_setbuf->vndr_ie_buffer.iecount, &iecount, sizeof(int));
	memcpy(&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].pktflag, &frametype, sizeof(uint32));
	memcpy(&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data, bufaddr, buflen);

#ifdef _TUDEBUGTRACE
	if (wps_msglevel & TUDUMP_IE) {
		printf("\nwps_wl_del_vndr_ie (%s, frag_len=%d)\n",
			_pktflag_name(frametype), frag_len);
		for (i = 0; i < frag_len; i++) {
			if (i && !(i%16))
				printf("\n");
			printf("%02x ", frag[i]);
		}
		printf("\n");
	}
#endif

	err = wl_iovar_set(wl_if, "vndr_ie", ie_setbuf, iebuf_len);

	free(ie_setbuf);

	return err;
}

static int
wps_wl_set_vndr_ie(char *wl_if, unsigned char *frag, int frag_len, unsigned char ouitype,
	unsigned int pktflag)
{
	vndr_ie_setbuf_t *ie_setbuf;
	int buflen, iecount, i;
	int err = 0;
#ifdef _TUDEBUGTRACE
	int wps_msglevel = wps_tutrace_get_msglevel();
#endif

	buflen = sizeof(vndr_ie_setbuf_t) + frag_len;
	ie_setbuf = (vndr_ie_setbuf_t *) malloc(buflen);
	if (!ie_setbuf) {
		TUTRACE((TUTRACE_ERR, "memory alloc failure\n"));
		return -1;
	}

	/* Copy the vndr_ie SET command ("add"/"del") to the buffer */
	strcpy(ie_setbuf->cmd, "add");

	/* Buffer contains only 1 IE */
	iecount = 1;
	memcpy(&ie_setbuf->vndr_ie_buffer.iecount, &iecount, sizeof(int));

	/*
	 * The packet flag bit field indicates the packets that will
	 * contain this IE
	 */
	memcpy(&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].pktflag, &pktflag, sizeof(uint32));

	/* Now, add the IE to the buffer, +1: one byte OUI_TYPE */
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.len = (uint8) frag_len +
		VNDR_IE_MIN_LEN + 1;

	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[0] = 0x00;
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[1] = 0x50;
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[2] = 0xf2;
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.data[0] = ouitype;

	for (i = 0; i < frag_len; i++) {
		ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.data[i+1] = frag[i];
	}

#ifdef _TUDEBUGTRACE
	if (wps_msglevel & TUDUMP_IE) {
		printf("\nwps_wl_set_vndr_ie (%s, frag_len=%d)\n",
			_pktflag_name(pktflag), frag_len);
		for (i = 0; i < frag_len; i++) {
			if (i && !(i%16))
				printf("\n");
			printf("%02x ", frag[i]);
		}
		printf("\n");
		printf("buflen=%d\n", buflen);
	}
#endif
	err = wl_iovar_set(wl_if, "vndr_ie", ie_setbuf, buflen);

	free(ie_setbuf);

	return err;
}

/* Parsing TLV format WPS IE */
static unsigned char *
_get_frag_wps_ie(unsigned char *p_data, int length, int *frag_len, int max_frag_len)
{
	int next_tlv_len, total_len = 0;
	uint16 type;
	unsigned char *next;

	if (!p_data || !frag_len || max_frag_len < 4)
		return NULL;

	if (length <= max_frag_len) {
		*frag_len = length;
		return p_data;
	}

	next = p_data;
	while (1) {
		type = WpsNtohs(next);
		next += 2; /* Move to L */
		next_tlv_len = WpsNtohs(next) + 4; /* Include Type and Value 4 bytes */
		next += 2; /* Move to V */
		if (next_tlv_len > max_frag_len) {
			TUTRACE((TUTRACE_ERR, "Error, there is a TLV length %d bigger than "
				"Max fragment length %d. Unable to fragment it.\n",
				next_tlv_len, max_frag_len));
			return NULL;
		}

		/* Abnormal IE check */
		if ((total_len + next_tlv_len) > length) {
			TUTRACE((TUTRACE_ERR, "Error, Abnormal WPS IE.\n"));
			*frag_len = length;
			return p_data;
		}

		/* Fragment point check */
		if ((total_len + next_tlv_len) > max_frag_len) {
			*frag_len = total_len;
			return p_data;
		}

		/* Get this TLV length */
		total_len += next_tlv_len;
		next += (next_tlv_len - 4); /* Move to next TLV */
	}

}

int
wps_wl_deauthenticate(char *wl_if, unsigned char *sta_mac, int reason)
{
	scb_val_t scb_val;

	/* remove the key if one is installed */
	wps_wl_set_key(wl_if, sta_mac, NULL, 0, 0, 0);

	scb_val.val = (uint32) reason;
	memcpy(&scb_val.ea, sta_mac, ETHER_ADDR_LEN);

	return wl_ioctl(wl_if, WLC_SCB_DEAUTHENTICATE_FOR_REASON,
		&scb_val, sizeof(scb_val));
}

/* Delete ALL specified ouitype IEs */
int
wps_wl_del_wps_ie(char *wl_if, unsigned int cmdtype, unsigned char ouitype)
{
	int i;
	char getbuf[2048] = {0};
	vndr_ie_buf_t *iebuf;
	vndr_ie_info_t *ieinfo;
	char wps_oui[4] = {0x00, 0x50, 0xf2, 0};
	char *bufaddr;
	int buflen = 0;
	int found = 0;
	uint32 pktflag;
	uint32 frametype;

	wps_oui[3] = ouitype;

	switch (cmdtype) {
	case WPS_IE_TYPE_SET_BEACON_IE:
		frametype = VNDR_IE_BEACON_FLAG;
		break;
	case WPS_IE_TYPE_SET_PROBE_REQUEST_IE:
		frametype = VNDR_IE_PRBREQ_FLAG;
		break;
	case WPS_IE_TYPE_SET_PROBE_RESPONSE_IE:
		frametype = VNDR_IE_PRBRSP_FLAG;
		break;
	case WPS_IE_TYPE_SET_ASSOC_REQUEST_IE:
		frametype = VNDR_IE_ASSOCREQ_FLAG;
		break;
	case WPS_IE_TYPE_SET_ASSOC_RESPONSE_IE:
		frametype = VNDR_IE_ASSOCRSP_FLAG;
		break;
	default:
		TUTRACE((TUTRACE_ERR, "unknown frame type\n"));
		return -1;
	}

	if (wl_iovar_get(wl_if, "vndr_ie", getbuf, 2048))
		return -1;

	iebuf = (vndr_ie_buf_t *) getbuf;
	bufaddr = (char*) iebuf->vndr_ie_list;

	/* Delete ALL specified ouitype IEs */
	for (i = 0; i < iebuf->iecount; i++) {
		ieinfo = (vndr_ie_info_t*) bufaddr;
		bcopy(bufaddr, (char*)&pktflag, (int) sizeof(uint32));
		if (pktflag == frametype) {
			if (!memcmp(ieinfo->vndr_ie_data.oui, wps_oui, 4)) {
				found = 1;
				bufaddr = (char*) &ieinfo->vndr_ie_data;
				buflen = (int)ieinfo->vndr_ie_data.len + VNDR_IE_HDR_LEN;
				/* Delete one vendor IE */
				wps_wl_del_vndr_ie(wl_if, bufaddr, buflen, frametype);
			}
		}
		bufaddr = ieinfo->vndr_ie_data.oui + ieinfo->vndr_ie_data.len;
	}

	if (!found)
		return -1;

	return 0;
}

#ifdef WFA_WPS_20_TESTBED
static char *
_get_update_partial_ie(unsigned int cmdtype, uint8 *updie_setbuf, uint8 *updie_len)
{
	char *wps_updie;
	unsigned char *src, *dest;
	unsigned char val;
	int idx, len;
	char hexstr[3];

	if (updie_setbuf == NULL)
		return NULL;

	/* Get environment variable */
	switch (cmdtype) {
	case WPS_IE_TYPE_SET_BEACON_IE:
		wps_updie = wps_get_conf("wps_beacon");
		break;
	case WPS_IE_TYPE_SET_PROBE_REQUEST_IE:
		wps_updie = wps_get_conf("wps_prbreq");
		break;
	case WPS_IE_TYPE_SET_PROBE_RESPONSE_IE:
		wps_updie = wps_get_conf("wps_prbrsp");
		break;
	case WPS_IE_TYPE_SET_ASSOC_REQUEST_IE:
		wps_updie = wps_get_conf("wps_assocreq");
		break;
	case WPS_IE_TYPE_SET_ASSOC_RESPONSE_IE:
		wps_updie = wps_get_conf("wps_assocrsp");
		break;
	default:
		TUTRACE((TUTRACE_ERR, "unknown frame type\n"));
		return NULL;
	}

	if (wps_updie == NULL)
		return NULL;

	/* reset first */
	*updie_len = 0;

	/* Ensure in 2 characters long */
	len = strlen(wps_updie);
	if (len % 2) {
		TUTRACE((TUTRACE_ERR, "Please specify all the data bytes for this IE\n"));
		return NULL;
	}
	*updie_len = (uint8) (len / 2);

	/* string to hex */
	src = (unsigned char*)wps_updie;
	dest = updie_setbuf;
	for (idx = 0; idx < len; idx++) {
		hexstr[0] = src[0];
		hexstr[1] = src[1];
		hexstr[2] = '\0';

		val = (unsigned char) strtoul(hexstr, NULL, 16);

		*dest++ = val;
		src += 2;
	}

	return updie_setbuf;
}

/* NOTE: ie[0] is NOT a length */
static uint32
wps_update_partial_ie(uint8 *buff, int buflen, uint8 *ie, uint8 ie_len,
	uint8 *updie, uint8 updie_len)
{
	uint8 wps_iebuf[2048];
	uint32 ret = WPS_SUCCESS;
	BufferObj *bufObj = NULL, *ie_bufObj = NULL, *updie_bufObj = NULL;
	uint16 theType, m_len;
	int tlvtype;
	TlvObj_uint8 ie_tlv_uint8, updie_tlv_uint8;
	TlvObj_uint16 ie_tlv_uint16, updie_tlv_uint16;
	TlvObj_uint32 ie_tlv_uint32, updie_tlv_uint32;
	TlvObj_ptru ie_tlv_uint8_ptr, updie_tlv_uint8_ptr;
	TlvObj_ptr ie_tlv_char_ptr, updie_tlv_char_ptr;
	CTlvPrimDeviceType ie_primDeviceType, updie_primDeviceType;
	void *data_v;

	if (!buff || !ie || !updie)
		return WPS_ERR_SYSTEM;

	/* De-serialize the embedded ie data to get the TLVs */
	memcpy(wps_iebuf, ie, ie_len);
	ie_bufObj = buffobj_new();
	if (!ie_bufObj)
		return WPS_ERR_SYSTEM;
	buffobj_dserial(ie_bufObj, wps_iebuf, ie_len);

	/* De-serialize the update partial ie data to get the TLVs */
	updie_bufObj = buffobj_new();
	if (!updie_bufObj) {
		ret = WPS_ERR_SYSTEM;
		goto error;
	}
	buffobj_dserial(updie_bufObj, updie, updie_len);

	/*  fill in the WPA-WPS OUI  */
	buff[0] = 4;
	buff[1] = 0x00;
	buff[2] = 0x50;
	buff[3] = 0xf2;
	buff[4] = 0x04;

	/*  bufObj is final output */
	bufObj = buffobj_setbuf(buff+5, buflen-5);
	if (!bufObj) {
		ret = WPS_ERR_SYSTEM;
		goto error;
	}

	/* Add each TLVs */
	while ((theType = buffobj_NextType(ie_bufObj))) {
		/* Check update ie */
		tlvtype = tlv_gettype(theType);
		switch (tlvtype) {
		case TLV_UINT8:
			if (tlv_dserialize(&ie_tlv_uint8, theType, ie_bufObj, 0, 0) != 0) {
				ret = WPS_ERR_SYSTEM;
				goto error;
			}
			/* Prepare embedded one */
			data_v = &ie_tlv_uint8.m_data;
			m_len = ie_tlv_uint8.tlvbase.m_len;

			/* Check update one */
			if (tlv_find_dserialize(&updie_tlv_uint8, theType, updie_bufObj,
				0, 0) == 0) {
				/* Use update ie */
				data_v = &updie_tlv_uint8.m_data;
				m_len = updie_tlv_uint8.tlvbase.m_len;
			}
			break;
		case TLV_UINT16:
			if (tlv_dserialize(&ie_tlv_uint16, theType, ie_bufObj, 0, 0) != 0) {
				ret = WPS_ERR_SYSTEM;
				goto error;
			}
			/* Prepare embedded one */
			data_v = &ie_tlv_uint16.m_data;
			m_len = ie_tlv_uint16.tlvbase.m_len;

			/* Check update one */
			if (tlv_find_dserialize(&updie_tlv_uint16, theType, updie_bufObj,
				0, 0) == 0) {
				/* Use update ie */
				data_v = &updie_tlv_uint16.m_data;
				m_len = updie_tlv_uint16.tlvbase.m_len;
			}
			break;
		case TLV_UINT32:
			if (tlv_dserialize(&ie_tlv_uint32, theType, ie_bufObj, 0, 0) != 0) {
				ret = WPS_ERR_SYSTEM;
				goto error;
			}
			/* Prepare embedded one */
			data_v = &ie_tlv_uint32.m_data;
			m_len = ie_tlv_uint32.tlvbase.m_len;

			/* Check update one */
			if (tlv_find_dserialize(&updie_tlv_uint32, theType, updie_bufObj,
				0, 0) == 0) {
				/* Use update ie */
				data_v = &updie_tlv_uint32.m_data;
				m_len = updie_tlv_uint32.tlvbase.m_len;
			}
			break;
		case TLV_UINT8_PTR:
			if (tlv_dserialize(&ie_tlv_uint8_ptr, theType, ie_bufObj, 0, 0) != 0) {
				ret = WPS_ERR_SYSTEM;
				goto error;
			}
			/* Prepare embedded one */
			data_v = ie_tlv_uint8_ptr.m_data;
			m_len = ie_tlv_uint8_ptr.tlvbase.m_len;

			/* Check update one */
			if (tlv_find_dserialize(&updie_tlv_uint8_ptr, theType, updie_bufObj,
				0, 0) == 0) {
				/* Use update ie */
				data_v = updie_tlv_uint8_ptr.m_data;
				m_len = updie_tlv_uint8_ptr.tlvbase.m_len;
			}
			break;
		case TLV_CHAR_PTR:
			if (tlv_dserialize(&ie_tlv_char_ptr, theType, ie_bufObj, 0, 0) != 0) {
				ret = WPS_ERR_SYSTEM;
				goto error;
			}
			/* Prepare embedded one */
			data_v = ie_tlv_char_ptr.m_data;
			m_len = ie_tlv_char_ptr.tlvbase.m_len;

			/* Check update one */
			if (tlv_find_dserialize(&updie_tlv_char_ptr, theType, updie_bufObj,
				0, 0) == 0) {
				/* Use update ie */
				data_v = updie_tlv_char_ptr.m_data;
				m_len = updie_tlv_char_ptr.tlvbase.m_len;
			}
			break;
		default:
			if (theType == WPS_ID_PRIM_DEV_TYPE) {
				if (tlv_primDeviceTypeParse(&ie_primDeviceType,	ie_bufObj) != 0) {
					ret = WPS_ERR_SYSTEM;
					goto error;
				}

				/* Check update one */
				if (tlv_find_primDeviceTypeParse(&updie_primDeviceType,
					updie_bufObj) == 0) {
					/* Use update ie */
					tlv_primDeviceTypeWrite(&updie_primDeviceType, bufObj);
				}
				else {
					/* Use embedded one */
					tlv_primDeviceTypeWrite(&ie_primDeviceType, bufObj);
				}
				continue;
			}

			ret = WPS_ERR_SYSTEM;
			goto error;
		}

		tlv_serialize(theType, bufObj, data_v, m_len);
	}

	/* store length */
	buff[0] += bufObj->m_dataLength;

error:
	if (bufObj)
		buffobj_del(bufObj);
	if (ie_bufObj)
		buffobj_del(ie_bufObj);
	if (updie_bufObj)
		buffobj_del(updie_bufObj);

	return ret;
}
#endif /* WFA_WPS_20_TESTBED */

/* The p_data and length does not contains the one byte OUI_TPYE */
int
wps_wl_set_wps_ie(char *wl_if, unsigned char *p_data, int length, unsigned int cmdtype,
	unsigned char ouitype)
{
	unsigned int pktflag;
	unsigned char *frag;
#ifdef WFA_WPS_20_TESTBED
	char *wps_ie_frag;
	int frag_threshold;
	uint8 wps_ie_setbuf[2048];
	uint8 updie_setbuf[2048];
	uint8 updie_len = 0;
#endif /* WFA_WPS_20_TESTBED */
	int frag_len;
	int err = 0;
	int frag_max = WLC_IOCTL_SMLEN - sizeof(vndr_ie_setbuf_t) - strlen("vndr_ie") - 1;

	switch (cmdtype) {
	case WPS_IE_TYPE_SET_BEACON_IE:
		pktflag = VNDR_IE_BEACON_FLAG;
		break;
	case WPS_IE_TYPE_SET_PROBE_REQUEST_IE:
		pktflag = VNDR_IE_PRBREQ_FLAG;
		break;
	case WPS_IE_TYPE_SET_PROBE_RESPONSE_IE:
		pktflag = VNDR_IE_PRBRSP_FLAG;
		break;
	case WPS_IE_TYPE_SET_ASSOC_REQUEST_IE:
		pktflag = VNDR_IE_ASSOCREQ_FLAG;
		break;
	case WPS_IE_TYPE_SET_ASSOC_RESPONSE_IE:
		pktflag = VNDR_IE_ASSOCRSP_FLAG;
		break;
	default:
		TUTRACE((TUTRACE_ERR, "unknown frame type\n"));
		return -1;
	}

	/* Delete specified IE first before add. */
	wps_wl_del_wps_ie(wl_if, cmdtype, ouitype);

	/* For testing purpose */
#ifdef WFA_WPS_20_TESTBED
	/* WPS IE fragment threshold */
	wps_ie_frag = wps_get_conf("wps_ie_frag");
	if (wps_ie_frag) {
		frag_threshold = atoi(wps_ie_frag);
		/* 72 = OUI + OUITYPE + TLV, V <=64 case */
		if (frag_threshold > frag_max || frag_threshold < 72) {
			TUTRACE((TUTRACE_ERR, "NVRAM specified WPS IE fragment threshold "
				"wps_ie_frag %s is invalid.\n"));
		}
		else {
			frag_max = frag_threshold;
		}
	}

	/* Update partial WPS IE */
	if (_get_update_partial_ie(cmdtype, updie_setbuf, &updie_len) != NULL) {
		if (wps_update_partial_ie(wps_ie_setbuf, sizeof(wps_ie_setbuf),
			p_data, (uint8)length, updie_setbuf, updie_len) != WPS_SUCCESS) {
			printf("Failed to update partial WPS IE in probe request\n");
			return -1;
		}

		/* Update new length */
		p_data = &wps_ie_setbuf[5];
		length = wps_ie_setbuf[0] - 4;
	}
#endif /* WFA_WPS_20_TESTBED */

	/* Separate a big IE to fragment IEs */
	frag = p_data;
	frag_len = length;
	while (length > 0) {
		if (length > frag_max)
			/* Find a appropriate fragment point */
			frag = _get_frag_wps_ie(frag, length, &frag_len, frag_max);

		if (!frag)
			return -1;

		/* Set fragment WPS IE */
		err |= wps_wl_set_vndr_ie(wl_if, frag, frag_len, ouitype, pktflag);

		/* Move to next */
		length -= frag_len;
		frag += frag_len;
		frag_len = length;
	}

	return (err);
}

/* There is another wps_wet_wsec in wps_linux */
int
wps_wl_open_wps_window(char *ifname)
{
	uint32 wsec;

	wl_ioctl(ifname, WLC_GET_WSEC, &wsec, sizeof(wsec));
	wsec = dtoh32(wsec);
	wsec |= SES_OW_ENABLED;
	wsec = htod32(wsec);

	return (wl_ioctl(ifname, WLC_SET_WSEC, &wsec, sizeof(wsec)));
}

int
wps_wl_close_wps_window(char *ifname)
{
	uint32 wsec;

	wl_ioctl(ifname, WLC_GET_WSEC, &wsec, sizeof(wsec));
	wsec = dtoh32(wsec);
	wsec &= ~SES_OW_ENABLED;
	wsec = htod32(wsec);

	return (wl_ioctl(ifname, WLC_SET_WSEC, &wsec, sizeof(wsec)));
}

int
wps_wl_get_maclist(char *ifname, char *buf, int count)
{
	struct maclist *mac_list;
	int mac_list_size;
	int num;

	mac_list_size = sizeof(mac_list->count) + count * sizeof(struct ether_addr);
	mac_list = malloc(mac_list_size);
	if (mac_list == NULL)
		return -1;

	/* query wl for authenticated sta list */
	strcpy((char*)mac_list, "authe_sta_list");
	if (wl_ioctl(ifname, WLC_GET_VAR, mac_list, mac_list_size)) {
		TUTRACE((TUTRACE_ERR, "GET authe_sta_list error!!!\n"));
		free(mac_list);
		return -1;
	}

	num = mac_list->count;
	memcpy(buf, mac_list->ea, num * sizeof(struct ether_addr));

	free(mac_list);
	return num;
}


#ifdef BCMWPSAPSTA
int
wps_wl_bss_config(char *ifname, int enabled)
{
	struct {int bsscfg_idx; int enable;} setbuf;
	int val;
	char tmp[100];
	char wl_name[IFNAMSIZ];
	char *value;

	snprintf(tmp, sizeof(tmp), "%s_ure_mode", wl_name);
	value = wps_safe_get_conf(tmp);

	/* WRE mode need to disable STA interface */
	if (strcmp(value, "wre") != 0)
		return -1;

	wl_iovar_get(ifname, "bsscfg_idx", &val, sizeof(int));
	val = dtoh32(val);

	TUTRACE((TUTRACE_INFO, "Set bss of %s (bsscfg_idx (%d)) to %s.\n",
		ifname, val, enabled?"up":"down"));

	setbuf.bsscfg_idx = htod32(val);
	setbuf.enable = htod32(enabled);

	if (wl_iovar_set(ifname, "bss", &setbuf, sizeof(setbuf))) {
		TUTRACE((TUTRACE_ERR, "Config BSS to %s (%d) failed!\n", enabled?"up":"down", val));
	}
	sprintf(tmp, "%s_bss_enabled", wl_name);
	if (enabled)
		wps_set_conf(tmp, "1");
	else
		wps_set_conf(tmp, "0");
	return 0;
}
#endif /* BCMWPSAPSTA */
