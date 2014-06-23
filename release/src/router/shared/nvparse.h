/*
 * Routines for managing persistent storage of port mappings, etc.
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
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
 *
 * $Id: nvparse.h 465779 2014-03-28 09:48:53Z $
 */

#ifndef _nvparse_h_
#define _nvparse_h_

/* 256 entries per list */
#ifndef MAX_NVPARSE
#define MAX_NVPARSE 16
#endif

/* Maximum  number of Traffic Management rules */
#define MAX_NUM_TRF_MGMT_RULES 10

/* 802.11u venue name */
#define VENUE_LANGUAGE_CODE_SIZE		3
#define VENUE_NAME_SIZE					255


#define BCM_DECODE_ANQP_MCC_LENGTH	3
#define BCM_DECODE_ANQP_MNC_LENGTH	3

#define BCM_DECODE_ANQP_MAX_OI_LENGTH	15
#define BCM_DECODE_ANQP_MAX_URL_LENGTH	128

/* WAN info - link status */
#define HSPOT_WAN_LINK_STATUS_SHIFT		0
#define HSPOT_WAN_LINK_STATUS_MASK		(0x03 << HSPOT_WAN_LINK_STATUS_SHIFT)
#define	HSPOT_WAN_LINK_UP				0x01
#define HSPOT_WAN_LINK_DOWN				0x02
#define HSPOT_WAN_LINK_TEST				0x03

/* WAN info - symmetric link */
#define HSPOT_WAN_SYMMETRIC_LINK_SHIFT	2
#define HSPOT_WAN_SYMMETRIC_LINK_MASK	(0x01 << HSPOT_WAN_SYMMETRIC_LINK_SHIFT)
#define HSPOT_WAN_SYMMETRIC_LINK		0x01
#define HSPOT_WAN_NOT_SYMMETRIC_LINK	0x00

/* WAN info - at capacity */
#define HSPOT_WAN_AT_CAPACITY_SHIFT		3
#define HSPOT_WAN_AT_CAPACITY_MASK		(0x01 << HSPOT_WAN_AT_CAPACITY_SHIFT)
#define HSPOT_WAN_AT_CAPACITY			0x01
#define HSPOT_WAN_NOT_AT_CAPACITY		0x00

/* Passpoint release numbers */
#define HSPOT_RELEASE_1		0
#define HSPOT_RELEASE_2		1

/* OSU method */
#define HSPOT_OSU_METHOD_OMA_DM			0
#define HSPOT_OSU_METHOD_SOAP_XML		1

#define BCM_DECODE_HSPOT_ANQP_MAX_OPERATOR_NAME	4

#define NVRAM_MAX_VAL_LEN		(2 * 1024)

#define ICONPATH					"/bin/"
#define LANG_ZXX					"zxx"
#define ENGLISH						"eng"

typedef struct {
	uint8 langLen;
	char lang[VENUE_LANGUAGE_CODE_SIZE + 1];	/* null terminated */
	uint8 nameLen;
	char name[VENUE_NAME_SIZE + 1];				/* null terminated */
} oper_name_t;

typedef struct {
	uint8 encoding;
	uint8 nameLen;
	char name[VENUE_NAME_SIZE + 1];				/* null terminated */
} nai_home_realm_t;

typedef struct
{
	uint8 langLen;
	char lang[VENUE_LANGUAGE_CODE_SIZE + 1];	/* null terminated */
	uint8 nameLen;
	char name[VENUE_NAME_SIZE + 1];				/* null terminated */
} venue_name_t;

typedef struct
{
	char mcc[BCM_DECODE_ANQP_MCC_LENGTH + 1];
	char mnc[BCM_DECODE_ANQP_MNC_LENGTH + 1];
} plmn_name_t;

typedef struct
{
	uint8 oiLen;
	uint8 oi[BCM_DECODE_ANQP_MAX_OI_LENGTH];
} oui_name_t;

typedef struct
{
	uint8 type;
	uint16 urlLen;
	uint8 url[BCM_DECODE_ANQP_MAX_URL_LENGTH + 1];		/* null terminated */
} net_auth_t;

typedef struct {
	uint8 linkStatus;
	uint8 symmetricLink;
	uint8 atCapacity;
	uint32 dlinkSpeed;
	uint32 ulinkSpeed;
	uint8 dlinkLoad;
	uint8 ulinkLoad;
	uint16 lmd;
	uint32 dlinkAvailable;
	uint32 ulinkAvailable;
} wan_metrics_t;

#define BCM_DECODE_AUTH_PARAM_TYPE_UNKNOWN	-1
#define BCM_DECODE_AUTH_PARAM_TYPE_1		1
#define BCM_DECODE_AUTH_PARAM_TYPE_2		2
#define BCM_DECODE_AUTH_PARAM_TYPE_3		3

#define BCM_DECODE_ANQP_MAX_AUTH_PARAM		16
typedef struct
{
	uint8 id;
	uint8 len;
	uint8 value[BCM_DECODE_ANQP_MAX_AUTH_PARAM];
} bcm_decode_anqp_auth_t;

#define BCM_DECODE_ANQP_MAX_AUTH			4
typedef struct
{
	uint8 eapMethod;
	uint8 authCount;
	bcm_decode_anqp_auth_t auth[BCM_DECODE_ANQP_MAX_AUTH];
} bcm_decode_anqp_eap_method_t;

#define REALM_INFO_LENGTH			1024
#define BCM_DECODE_ANQP_MAX_REALM_LENGTH	255
#define BCM_DECODE_ANQP_MAX_EAP_METHOD		4
typedef struct
{
	uint8 encoding;
	uint8 realmLen;
	uint8 realm[BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1];	/* null terminated */
	uint8 eapCount;
	uint8 realmInfoLen;
	uint8 realmInfo[REALM_INFO_LENGTH + 1];	/* null terminated */
	uint8 eapInfo[BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1];
	bcm_decode_anqp_eap_method_t eap[BCM_DECODE_ANQP_MAX_EAP_METHOD];
} bcm_decode_anqp_nai_realm_data_t;

#define BCM_DECODE_ANQP_MAX_REALM			16
typedef struct
{
	int isDecodeValid;
	uint16 realmCount;
	bcm_decode_anqp_nai_realm_data_t realm[BCM_DECODE_ANQP_MAX_REALM];
} realm_list_t;

typedef struct {
	uint8 langLen;
	char lang[VENUE_LANGUAGE_CODE_SIZE + 1];	/* null terminated */
	uint8 nameLen;
	char name[VENUE_NAME_SIZE + 1];		/* null terminated */
} bcm_decode_hspot_anqp_name_duple_t;

#define BCM_DECODE_HSPOT_ANQP_MAX_OPERATOR_NAME	4
typedef struct {
	int isDecodeValid;
	int numName;
	bcm_decode_hspot_anqp_name_duple_t duple[BCM_DECODE_HSPOT_ANQP_MAX_OPERATOR_NAME];
} bcm_decode_hspot_anqp_operator_friendly_name_t;

#define BCM_DECODE_HSPOT_ANQP_MAX_ICON_TYPE_LENGTH	128
#define BCM_DECODE_HSPOT_ANQP_MAX_ICON_FILENAME_LENGTH	128
typedef struct
{
	uint16 width;
	uint16 height;
	char lang[VENUE_LANGUAGE_CODE_SIZE + 1];	/* null terminated */
	uint8 typeLength;
	uint8 type[BCM_DECODE_HSPOT_ANQP_MAX_ICON_TYPE_LENGTH + 1];	/* null terminated */
	uint8 filenameLength;
	char filename[BCM_DECODE_HSPOT_ANQP_MAX_ICON_FILENAME_LENGTH + 1];
} bcm_decode_hspot_anqp_icon_metadata_t;

#define BCM_DECODE_HSPOT_ANQP_MAX_NAI_LENGTH		128
#define BCM_DECODE_HSPOT_ANQP_MAX_METHOD_LENGTH		2
#define BCM_DECODE_HSPOT_ANQP_MAX_ICON_METADATA_LENGTH	8
#define BCM_DECODE_HSPOT_ANQP_MAX_URI_LENGTH		128
#define FRIENDLY_NAME_INFO_LENGTH			2048
typedef struct
{
	bcm_decode_hspot_anqp_operator_friendly_name_t name;
	uint8 uriLength;
	uint8 uri[BCM_DECODE_HSPOT_ANQP_MAX_URI_LENGTH + 1];	/* null terminated */
	uint8 methodLength;
	uint8 method[BCM_DECODE_HSPOT_ANQP_MAX_METHOD_LENGTH];
	int iconMetadataCount;
	bcm_decode_hspot_anqp_icon_metadata_t iconMetadata[
		BCM_DECODE_HSPOT_ANQP_MAX_ICON_METADATA_LENGTH];
	uint8 naiLength;
	uint8 nai[BCM_DECODE_HSPOT_ANQP_MAX_NAI_LENGTH + 1];	/* null terminated */
	bcm_decode_hspot_anqp_operator_friendly_name_t desc;
	char nameInfo[FRIENDLY_NAME_INFO_LENGTH + 1];
	char descInfo[FRIENDLY_NAME_INFO_LENGTH + 1];
	char iconInfo[FRIENDLY_NAME_INFO_LENGTH + 1];
} bcm_decode_hspot_anqp_osu_provider_t;

#define BCM_DECODE_HSPOT_ANQP_MAX_OSU_SSID_LENGTH	255
#define BCM_DECODE_HSPOT_ANQP_MAX_OSU_PROVIDER		16
typedef struct
{
	int isDecodeValid;
	uint8 osuSsidLength;
	uint8 osuSsid[BCM_DECODE_HSPOT_ANQP_MAX_OSU_SSID_LENGTH + 1];	/* null terminated */
	uint8 osuProviderCount;
	bcm_decode_hspot_anqp_osu_provider_t osuProvider[BCM_DECODE_HSPOT_ANQP_MAX_OSU_PROVIDER];
} osu_provider_list_t;

/* 20 DSCP + 0x0 default */
#define MAX_NUM_TRF_MGMT_DWM_RULES 21

#if !defined(AUTOFW_PORT_DEPRECATED)
/*
 * Automatic (application specific) port forwards are described by a
 * netconf_app_t structure. A specific outbound connection triggers
 * the expectation of one or more inbound connections which may be
 * optionally remapped to a different port range.
 */
extern bool valid_autofw_port(const netconf_app_t *app);
extern bool get_autofw_port(int which, netconf_app_t *app);
extern bool set_autofw_port(int which, const netconf_app_t *app);
extern bool del_autofw_port(int which);
#endif /* !AUTOFW_PORT_DEPRECATED */

/*
 * Persistent (static) port forwards are described by a netconf_nat_t
 * structure. On Linux, a netconf_filter_t that matches the target
 * parameters of the netconf_nat_t should also be added to the INPUT
 * and FORWARD tables to ACCEPT the forwarded connection.
 */
extern bool valid_forward_port(const netconf_nat_t *nat);
extern bool get_forward_port(int which, netconf_nat_t *nat);
extern bool set_forward_port(int which, const netconf_nat_t *nat);
extern bool del_forward_port(int which);

/*
 * Client filters are described by two netconf_filter_t structures that
 * differ in match.src.ipaddr alone (to support IP address ranges)
 */
extern bool valid_filter_client(const netconf_filter_t *start, const netconf_filter_t *end);
extern bool get_filter_client(int which, netconf_filter_t *start, netconf_filter_t *end);
extern bool set_filter_client(int which, const netconf_filter_t *start,
                              const netconf_filter_t *end);
extern bool del_filter_client(int which);

#ifdef __CONFIG_URLFILTER__
/*
 * URL filters are described by two netconf_urlfilter_t structures that
 * differ in match.src.ipaddr alone (to support IP address ranges)
 */
extern bool valid_filter_url(const netconf_urlfilter_t *start, const netconf_urlfilter_t *end);
extern bool get_filter_url(int which, netconf_urlfilter_t *start, netconf_urlfilter_t *end);
extern bool set_filter_url(int which, const netconf_urlfilter_t *start,
                              const netconf_urlfilter_t *end);
extern bool del_filter_url(int which);
#endif /* __CONFIG_URLFILTER__ */

extern bool valid_trf_mgmt_port(const netconf_trmgmt_t *trmgmt);
extern bool set_trf_mgmt_port(char *prefix, int which, const netconf_trmgmt_t *trmgmt);
extern bool get_trf_mgmt_port(char *prefix, int which, netconf_trmgmt_t *trmgmt);
extern bool del_trf_mgmt_port(char *prefix, int which);

extern bool set_trf_mgmt_dwm(char *prefix, int which, const netconf_trmgmt_t *trmgmt);
extern bool get_trf_mgmt_dwm(char *prefix, int which, netconf_trmgmt_t *trmgmt);
extern bool del_trf_mgmt_dwm(char *prefix, int which);

/*
* WPA/WDS per link configuration. Parameters after 'auth' are
* authentication algorithm dependant:
*
* When auth is "psk", the parameter list is:
*
* 	bool get_wds_wsec(int unit, int which, char *mac, char *role,
*		char *crypto, char *auth, char *ssid, char *passphrase);
*/
extern bool get_wds_wsec(int unit, int which, char *mac, char *role,
                         char *crypto, char *auth, ...);
extern bool set_wds_wsec(int unit, int which, char *mac, char *role,
                         char *crypto, char *auth, ...);
extern bool del_wds_wsec(int unit, int which);

/* Conversion routine for deprecated variables */
extern void convert_deprecated(void);

#endif /* _nvparse_h_ */
