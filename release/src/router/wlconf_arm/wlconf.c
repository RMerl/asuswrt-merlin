/*
 * Wireless Network Adapter Configuration Utility
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlconf.c 454872 2014-02-12 02:49:54Z $
 */

#include <typedefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <bcmnvram.h>
#include <bcmutils.h>
#include <bcmparams.h>
#include <bcmdevs.h>
#include <shutils.h>
#include <wlutils.h>
#include <wlioctl.h>
#include <proto/802.1d.h>
#include <bcmconfig.h>
#include <bcmwifi_channels.h>
#include <netconf.h>
#include <nvparse.h>
#include <arpa/inet.h>

#if defined(linux)
#include <sys/utsname.h>
#endif

/* phy types */
#define	PHY_TYPE_A		0
#define	PHY_TYPE_B		1
#define	PHY_TYPE_G		2
#define	PHY_TYPE_N		4
#define	PHY_TYPE_LP		5
#define PHY_TYPE_SSN		6
#define	PHY_TYPE_HT		7
#define	PHY_TYPE_LCN		8
#define PHY_TYPE_AC		11
#define	PHY_TYPE_NULL		0xf

/* how many times to attempt to bring up a virtual i/f when
 * we are in APSTA mode and IOVAR set of "bss" "up" returns busy
 */
#define MAX_BSS_UP_RETRIES 5

/* notify the average dma xfer rate (in kbps) to the driver */
#define AVG_DMA_XFER_RATE 120000

/* parts of an idcode: */
#define	IDCODE_MFG_MASK		0x00000fff
#define	IDCODE_MFG_SHIFT	0
#define	IDCODE_ID_MASK		0x0ffff000
#define	IDCODE_ID_SHIFT		12
#define	IDCODE_REV_MASK		0xf0000000
#define	IDCODE_REV_SHIFT	28

/*
 * Debugging Macros
 */
#ifdef BCMDBG
#define WLCONF_DBG(fmt, arg...)	printf("%s: "fmt, __FUNCTION__ , ## arg)
#define WL_IOCTL(ifname, cmd, buf, len)					\
	if ((ret = wl_ioctl(ifname, cmd, buf, len)))			\
		fprintf(stderr, "%s:%d:(%s): %s failed, err = %d\n",	\
		        __FUNCTION__, __LINE__, ifname, #cmd, ret);
#define WL_SETINT(ifname, cmd, val)								\
	if ((ret = wlconf_setint(ifname, cmd, val)))						\
		fprintf(stderr, "%s:%d:(%s): setting %s to %d (0x%x) failed, err = %d\n",	\
		        __FUNCTION__, __LINE__, ifname, #cmd, (int)val, (unsigned int)val, ret);
#define WL_GETINT(ifname, cmd, pval)								\
	if ((ret = wlconf_getint(ifname, cmd, pval)))						\
		fprintf(stderr, "%s:%d:(%s): getting %s failed, err = %d\n",			\
		        __FUNCTION__, __LINE__, ifname, #cmd, ret);
#define WL_IOVAR_SET(ifname, iovar, param, paramlen)					\
	if ((ret = wl_iovar_set(ifname, iovar, param, paramlen)))			\
		fprintf(stderr, "%s:%d:(%s): setting iovar \"%s\" failed, err = %d\n",	\
		        __FUNCTION__, __LINE__, ifname, iovar, ret);
#define WL_IOVAR_GET(ifname, iovar, param, paramlen)					\
	if ((ret = wl_iovar_get(ifname, iovar, param, paramlen)))			\
		fprintf(stderr, "%s:%d:(%s): getting iovar \"%s\" failed, err = %d\n",	\
		        __FUNCTION__, __LINE__, ifname, iovar, ret);
#define WL_IOVAR_SETINT(ifname, iovar, val)							\
	if ((ret = wl_iovar_setint(ifname, iovar, val)))					\
		fprintf(stderr, "%s:%d:(%s): setting iovar \"%s\" to 0x%x failed, err = %d\n",	\
		        __FUNCTION__, __LINE__, ifname, iovar, (unsigned int)val, ret);
#define WL_IOVAR_GETINT(ifname, iovar, val)							\
	if ((ret = wl_iovar_getint(ifname, iovar, val)))					\
		fprintf(stderr, "%s:%d:(%s): getting iovar \"%s\" failed, err = %d\n",	\
		        __FUNCTION__, __LINE__, ifname, iovar, ret);
#define WL_BSSIOVAR_SETBUF(ifname, iovar, bssidx, param, paramlen, buf, buflen)			\
	if ((ret = wl_bssiovar_setbuf(ifname, iovar, bssidx, param, paramlen, buf, buflen)))	\
		fprintf(stderr, "%s:%d:(%s): setting bsscfg #%d iovar \"%s\" failed, err = %d\n", \
		        __FUNCTION__, __LINE__, ifname, bssidx, iovar, ret);
#define WL_BSSIOVAR_SET(ifname, iovar, bssidx, param, paramlen)					\
	if ((ret = wl_bssiovar_set(ifname, iovar, bssidx, param, paramlen)))			\
		fprintf(stderr, "%s:%d:(%s): setting bsscfg #%d iovar \"%s\" failed, err = %d\n", \
		        __FUNCTION__, __LINE__, ifname, bssidx, iovar, ret);
#define WL_BSSIOVAR_GET(ifname, iovar, bssidx, param, paramlen)					\
	if ((ret = wl_bssiovar_get(ifname, iovar, bssidx, param, paramlen)))			\
		fprintf(stderr, "%s:%d:(%s): getting bsscfg #%d iovar \"%s\" failed, err = %d\n", \
		        __FUNCTION__, __LINE__, ifname, bssidx, iovar, ret);
#define WL_BSSIOVAR_SETINT(ifname, iovar, bssidx, val)						\
	if ((ret = wl_bssiovar_setint(ifname, iovar, bssidx, val)))				\
		fprintf(stderr, "%s:%d:(%s): setting bsscfg #%d iovar \"%s\" " \
				"to val 0x%x failed, err = %d\n",	\
		        __FUNCTION__, __LINE__, ifname, bssidx, iovar, (unsigned int)val, ret);
#else
#define WLCONF_DBG(fmt, arg...)
#define WL_IOCTL(name, cmd, buf, len)			(ret = wl_ioctl(name, cmd, buf, len))
#define WL_SETINT(name, cmd, val)			(ret = wlconf_setint(name, cmd, val))
#define WL_GETINT(name, cmd, pval)			(ret = wlconf_getint(name, cmd, pval))
#define WL_IOVAR_SET(ifname, iovar, param, paramlen)	(ret = wl_iovar_set(ifname, iovar, \
							param, paramlen))
#define WL_IOVAR_GET(ifname, iovar, param, paramlen)	(ret = wl_iovar_get(ifname, iovar, \
							param, paramlen))
#define WL_IOVAR_SETINT(ifname, iovar, val)		(ret = wl_iovar_setint(ifname, iovar, val))
#define WL_IOVAR_GETINT(ifname, iovar, val)		(ret = wl_iovar_getint(ifname, iovar, val))
#define WL_BSSIOVAR_SETBUF(ifname, iovar, bssidx, param, paramlen, buf, buflen) \
		(ret = wl_bssiovar_setbuf(ifname, iovar, bssidx, param, paramlen, buf, buflen))
#define WL_BSSIOVAR_SET(ifname, iovar, bssidx, param, paramlen) \
		(ret = wl_bssiovar_set(ifname, iovar, bssidx, param, paramlen))
#define WL_BSSIOVAR_GET(ifname, iovar, bssidx, param, paramlen) \
		(ret = wl_bssiovar_get(ifname, iovar, bssidx, param, paramlen))
#define WL_BSSIOVAR_SETINT(ifname, iovar, bssidx, val)	(ret = wl_bssiovar_setint(ifname, iovar, \
			bssidx, val))
#endif /* BCMDBG */

#define CHECK_PSK(mode) ((mode) & (WPA_AUTH_PSK | WPA2_AUTH_PSK))

/* prototypes */
struct bsscfg_list *wlconf_get_bsscfgs(char* ifname, char* prefix);
int wlconf(char *name);
int wlconf_down(char *name);

static int
wlconf_getint(char* ifname, int cmd, int *pval)
{
	return wl_ioctl(ifname, cmd, pval, sizeof(int));
}

static int
wlconf_setint(char* ifname, int cmd, int val)
{
	return wl_ioctl(ifname, cmd, &val, sizeof(int));
}

static int
wlconf_wds_clear(char *name)
{
	struct maclist maclist;
	int    ret;

	maclist.count = 0;
	WL_IOCTL(name, WLC_SET_WDSLIST, &maclist, sizeof(maclist));

	return ret;
}

/* set WEP key */
static int
wlconf_set_wep_key(char *name, char *prefix, int bsscfg_idx, int i)
{
	wl_wsec_key_t key;
	char wl_key[] = "wlXXXXXXXXXX_keyXXXXXXXXXX";
	char *keystr, hex[] = "XX";
	unsigned char *data = key.data;
	int ret = 0;

	memset(&key, 0, sizeof(key));
	key.index = i - 1;
	sprintf(wl_key, "%skey%d", prefix, i);
	keystr = nvram_safe_get(wl_key);

	switch (strlen(keystr)) {
	case WEP1_KEY_SIZE:
	case WEP128_KEY_SIZE:
		key.len = strlen(keystr);
		strcpy((char *)key.data, keystr);
		break;
	case WEP1_KEY_HEX_SIZE:
	case WEP128_KEY_HEX_SIZE:
		key.len = strlen(keystr) / 2;
		while (*keystr) {
			strncpy(hex, keystr, 2);
			*data++ = (unsigned char) strtoul(hex, NULL, 16);
			keystr += 2;
		}
		break;
	default:
		key.len = 0;
		break;
	}

	/* Set current WEP key */
	if (key.len && i == atoi(nvram_safe_get(strcat_r(prefix, "key", wl_key))))
		key.flags = WL_PRIMARY_KEY;

	WL_BSSIOVAR_SET(name, "wsec_key", bsscfg_idx, &key, sizeof(key));

	return ret;
}

static int
wlconf_akm_options(char *prefix)
{
	char comb[32];
	char *wl_akm;
	int akm_ret_val = 0;
	char akm[32];
	char *next;

	wl_akm = nvram_safe_get(strcat_r(prefix, "akm", comb));
	foreach(akm, wl_akm, next) {
		if (!strcmp(akm, "wpa"))
			akm_ret_val |= WPA_AUTH_UNSPECIFIED;
		if (!strcmp(akm, "psk"))
			akm_ret_val |= WPA_AUTH_PSK;
		if (!strcmp(akm, "wpa2"))
			akm_ret_val |= WPA2_AUTH_UNSPECIFIED;
		if (!strcmp(akm, "psk2"))
			akm_ret_val |= WPA2_AUTH_PSK;
		if (!strcmp(akm, "brcm_psk"))
			akm_ret_val |= BRCM_AUTH_PSK;
	}
	return akm_ret_val;
}

/* Set up wsec */
static int
wlconf_set_wsec(char *ifname, char *prefix, int bsscfg_idx)
{
	char tmp[100];
	int val = 0;
	int akm_val;
	int ret;

	/* Set wsec bitvec */
	akm_val = wlconf_akm_options(prefix);
	if (akm_val != 0) {
		if (nvram_match(strcat_r(prefix, "crypto", tmp), "tkip"))
			val = TKIP_ENABLED;
		else if (nvram_match(strcat_r(prefix, "crypto", tmp), "aes"))
			val = AES_ENABLED;
		else if (nvram_match(strcat_r(prefix, "crypto", tmp), "tkip+aes"))
			val = TKIP_ENABLED | AES_ENABLED;
	}
	if (nvram_match(strcat_r(prefix, "wep", tmp), "enabled"))
		val |= WEP_ENABLED;
	WL_BSSIOVAR_SETINT(ifname, "wsec", bsscfg_idx, val);
	/* Set wsec restrict if WSEC_ENABLED */
	WL_BSSIOVAR_SETINT(ifname, "wsec_restrict", bsscfg_idx, val ? 1 : 0);

	return 0;
}

static int
wlconf_set_preauth(char *name, int bsscfg_idx, int preauth)
{
	uint cap;
	int ret;

	WL_BSSIOVAR_GET(name, "wpa_cap", bsscfg_idx, &cap, sizeof(uint));
	if (ret != 0) return -1;

	if (preauth)
		cap |= WPA_CAP_WPA2_PREAUTH;
	else
		cap &= ~WPA_CAP_WPA2_PREAUTH;

	WL_BSSIOVAR_SETINT(name, "wpa_cap", bsscfg_idx, cap);

	return ret;
}

static void
wlconf_set_radarthrs(char *name, char *prefix)
{
	wl_radar_thr_t  radar_thr;
	int  i, ret, len;
	char nv_buf[NVRAM_MAX_VALUE_LEN], *rargs, *v, *endptr;
	char buf[WLC_IOCTL_SMLEN];

	char *version = NULL;
	char *thr0_20_lo = NULL, *thr1_20_lo = NULL;
	char *thr0_40_lo = NULL, *thr1_40_lo = NULL;
	char *thr0_80_lo = NULL, *thr1_80_lo = NULL;
	char *thr0_20_hi = NULL, *thr1_20_hi = NULL;
	char *thr0_40_hi = NULL, *thr1_40_hi = NULL;
	char *thr0_80_hi = NULL, *thr1_80_hi = NULL;

	char **locals[] = { &version, &thr0_20_lo, &thr1_20_lo, &thr0_40_lo, &thr1_40_lo,
	&thr0_80_lo, &thr1_80_lo, &thr0_20_hi, &thr1_20_hi,
	&thr0_40_hi, &thr1_40_hi, &thr0_80_hi, &thr1_80_hi };

	rargs = nvram_safe_get(strcat_r(prefix, "radarthrs", nv_buf));
	if (!rargs)
		goto err;

	len = strlen(rargs);
	if ((len > NVRAM_MAX_VALUE_LEN) || (len == 0))
		goto err;

	memset(nv_buf, 0, sizeof(nv_buf));
	strncpy(nv_buf, rargs, len);
	v = nv_buf;
	for (i = 0; i < (sizeof(locals) / sizeof(locals[0])); i++) {
		*locals[i] = v;
		while (*v && *v != ' ') {
			v++;
		}
		if (*v) {
			*v = 0;
			v++;
		}
		if (v >= (nv_buf + len)) /* Check for complete list, if not caught later */
			break;
	}

	/* Start building request */
	memset(buf, 0, sizeof(buf));
	strcpy(buf, "radarthrs");
	/* Retrieve radar thrs parameters */
	if (!version)
		goto err;
	radar_thr.version = atoi(version);
	if (radar_thr.version > WL_RADAR_THR_VERSION)
		goto err;

	/* Retrieve ver 0 params */
	if (!thr0_20_lo)
		goto err;
	radar_thr.thresh0_20_lo = (uint16)strtol(thr0_20_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr1_20_lo)
		goto err;
	radar_thr.thresh1_20_lo = (uint16)strtol(thr1_20_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr0_40_lo)
		goto err;
	radar_thr.thresh0_40_lo = (uint16)strtol(thr0_40_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr1_40_lo)
		goto err;
	radar_thr.thresh1_40_lo = (uint16)strtol(thr1_40_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr0_80_lo)
		goto err;
	radar_thr.thresh0_80_lo = (uint16)strtol(thr0_80_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr1_80_lo)
		goto err;
	radar_thr.thresh1_80_lo = (uint16)strtol(thr1_80_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;


	if (radar_thr.version == 0) {
		/*
		 * Attempt a best effort update of ver 0 to ver 1 by updating
		 * the appropriate values with the specified defaults.  The defaults
		 * are from the reference design.
		 */
		radar_thr.version = WL_RADAR_THR_VERSION; /* avoid driver rejecting it */
		radar_thr.thresh0_20_hi = 0x6ac;
		radar_thr.thresh1_20_hi = 0x6cc;
		radar_thr.thresh0_40_hi = 0x6bc;
		radar_thr.thresh1_40_hi = 0x6e0;
		radar_thr.thresh0_80_hi = 0x6b0;
		radar_thr.thresh1_80_hi = 0x30;
	} else {
		/* Retrieve ver 1 params */
		if (!thr0_20_hi)
			goto err;
		radar_thr.thresh0_20_hi = (uint16)strtol(thr0_20_hi, &endptr, 0);
		if (*endptr != '\0')
			goto err;

		if (!thr1_20_hi)
			goto err;
		radar_thr.thresh1_20_hi = (uint16)strtol(thr1_20_hi, &endptr, 0);
		if (*endptr != '\0')
			goto err;

		if (!thr0_40_hi)
			goto err;
		radar_thr.thresh0_40_hi = (uint16)strtol(thr0_40_hi, &endptr, 0);
		if (*endptr != '\0')
			goto err;

		if (!thr1_40_hi)
			goto err;
		radar_thr.thresh1_40_hi = (uint16)strtol(thr1_40_hi, &endptr, 0);
		if (*endptr != '\0')
			goto err;

		if (!thr0_80_hi)
			goto err;
		radar_thr.thresh0_80_hi = (uint16)strtol(thr0_80_hi, &endptr, 0);
		if (*endptr != '\0')
			goto err;

		if (!thr1_80_hi)
			goto err;
		radar_thr.thresh1_80_hi = (uint16)strtol(thr1_80_hi, &endptr, 0);
		if (*endptr != '\0')
			goto err;

	}

	/* Copy radar parameters into buffer and plug them to the driver */
	memcpy((char*)(buf + strlen(buf) + 1), (char*)&radar_thr, sizeof(wl_radar_thr_t));
	WL_IOCTL(name, WLC_SET_VAR, buf, sizeof(buf));

	return;

err:
	WLCONF_DBG("Did not parse radar thrs params, using driver defaults\n");
	return;
}

/*
 * This allows phy antenna selection to be retrieved from NVRAM
 */
static void
wlconf_set_antsel(char *name, char *prefix)
{
	int	i, j, len, argc, ret;
	char	buf[WLC_IOCTL_SMLEN];
	wlc_antselcfg_t val = { {0}, 0};
	char	*argv[ANT_SELCFG_MAX] = {};
	char	nv_buf[NVRAM_MAX_VALUE_LEN], *argstr, *v, *endptr;

	argstr = nvram_safe_get(strcat_r(prefix, "phy_antsel", nv_buf));
	if (!argstr) {
		return;
	}
	len = strlen(argstr);
	if ((len == 0) || (len > NVRAM_MAX_VALUE_LEN)) {
		return;
	}

	memset(nv_buf, 0, sizeof(nv_buf));
	strncpy(nv_buf, argstr, len);
	v = nv_buf;
	for (argc = 0; argc < ANT_SELCFG_MAX; ) {
		argv[argc++] = v;
		while (*v && *v != ' ') {
			v++;
		}
		if (*v) {
			*v = 0;
			v++;
		}
		if (v >= (nv_buf + len)) {
			break;
		}
	}
	if ((argc != 1) && (argc != ANT_SELCFG_MAX)) {
		WLCONF_DBG("phy_antsel requires 1 or %d arguments\n", ANT_SELCFG_MAX);
		return;
	}

	memset(buf, 0, sizeof(buf));
	strcpy(buf, "phy_antsel");
	for (i = 0, j = 0; i < ANT_SELCFG_MAX; i++) {
		val.ant_config[i] = (uint8)strtol(argv[j], &endptr, 0);
		if (*endptr != '\0') {
			WLCONF_DBG("Invalid antsel argument\n");
			return;
		}
		if (argc > 1) {
			/* ANT_SELCFG_MAX argument format */
			j++;
		}
	}

	/* Copy antsel parameters into buffer and plug them to the driver */
	memcpy((char*)(buf + strlen(buf) + 1), (char*)&val, sizeof(wlc_antselcfg_t));
	WL_IOCTL(name, WLC_SET_VAR, buf, sizeof(buf));

	return;
}



static void
wlconf_set_current_txparam_into_nvram(char *name, char *prefix)
{
	int ret, aci;
	wme_tx_params_t txparams[AC_COUNT];
	char *nv[] = {"wme_txp_be", "wme_txp_bk", "wme_txp_vi", "wme_txp_vo"};
	char data[50], tmp[50];

	/* get the WME tx parameters */
	WL_IOVAR_GET(name, "wme_tx_params", txparams, sizeof(txparams));

	/* Set nvram accordingly */
	for (aci = 0; aci < AC_COUNT; aci++) {
		sprintf(data, "%d %d %d %d %d", txparams[aci].short_retry,
			txparams[aci].short_fallback,
			txparams[aci].long_retry,
			txparams[aci].long_fallback,
			txparams[aci].max_rate);

		nvram_set(strcat_r(prefix, nv[aci], tmp), data);
	}
}

/* Set up WME */
static void
wlconf_set_wme(char *name, char *prefix)
{
	int i, j, k;
	int val, ret;
	int phytype, gmode, no_ack, apsd, dp[2];
	edcf_acparam_t *acparams;
	/* Pay attention to buffer length requirements when using this */
	char buf[WLC_IOCTL_SMLEN*2];
	char *v, *nv_value, nv[100];
	char nv_name[] = "%swme_%s_%s";
	char *ac[] = {"be", "bk", "vi", "vo"};
	char *cwmin, *cwmax, *aifsn, *txop_b, *txop_ag, *admin_forced, *oldest_first;
	char **locals[] = { &cwmin, &cwmax, &aifsn, &txop_b, &txop_ag, &admin_forced,
	                    &oldest_first };
	struct {char *req; char *str;} mode[] = {{"wme_ac_ap", "ap"}, {"wme_ac_sta", "sta"},
	                                         {"wme_tx_params", "txp"}};

	/* query the phy type */
	WL_IOCTL(name, WLC_GET_PHYTYPE, &phytype, sizeof(phytype));
	/* get gmode */
	gmode = atoi(nvram_safe_get(strcat_r(prefix, "gmode", nv)));

	/* WME sta setting first */
	for (i = 0; i < 2; i++) {
		/* build request block */
		memset(buf, 0, sizeof(buf));
		strcpy(buf, mode[i].req);
		/* put push wmeac params after "wme-ac" in buf */
		acparams = (edcf_acparam_t *)(buf + strlen(buf) + 1);
		dp[i] = 0;
		for (j = 0; j < AC_COUNT; j++) {
			/* get packed nvram parameter */
			snprintf(nv, sizeof(nv), nv_name, prefix, mode[i].str, ac[j]);
			nv_value = nvram_safe_get(nv);
			strcpy(nv, nv_value);
			/* unpack it */
			v = nv;
			for (k = 0; k < (sizeof(locals) / sizeof(locals[0])); k++) {
				*locals[k] = v;
				while (*v && *v != ' ')
					v++;
				if (*v) {
					*v = 0;
					v++;
				}
			}

			/* update CWmin */
			acparams->ECW &= ~EDCF_ECWMIN_MASK;
			val = atoi(cwmin);
			for (val++, k = 0; val; val >>= 1, k++);
			acparams->ECW |= (k ? k - 1 : 0) & EDCF_ECWMIN_MASK;
			/* update CWmax */
			acparams->ECW &= ~EDCF_ECWMAX_MASK;
			val = atoi(cwmax);
			for (val++, k = 0; val; val >>= 1, k++);
			acparams->ECW |= ((k ? k - 1 : 0) << EDCF_ECWMAX_SHIFT) & EDCF_ECWMAX_MASK;
			/* update AIFSN */
			acparams->ACI &= ~EDCF_AIFSN_MASK;
			acparams->ACI |= atoi(aifsn) & EDCF_AIFSN_MASK;
			/* update ac */
			acparams->ACI &= ~EDCF_ACI_MASK;
			acparams->ACI |= j << EDCF_ACI_SHIFT;
			/* update TXOP */
			if (phytype == PHY_TYPE_B || gmode == 0)
				val = atoi(txop_b);
			else
				val = atoi(txop_ag);
			acparams->TXOP = val / 32;
			/* update acm */
			acparams->ACI &= ~EDCF_ACM_MASK;
			val = strcmp(admin_forced, "on") ? 0 : 1;
			acparams->ACI |= val << 4;

			/* configure driver */
			WL_IOCTL(name, WLC_SET_VAR, buf, sizeof(buf));
		}
	}

	/* set no-ack */
	v = nvram_safe_get(strcat_r(prefix, "wme_no_ack", nv));
	no_ack = strcmp(v, "on") ? 0 : 1;
	WL_IOVAR_SETINT(name, "wme_noack", no_ack);

	/* set APSD */
	v = nvram_safe_get(strcat_r(prefix, "wme_apsd", nv));
	apsd = strcmp(v, "on") ? 0 : 1;
	WL_IOVAR_SETINT(name, "wme_apsd", apsd);

	/* set per-AC discard policy */
	strcpy(buf, "wme_dp");
	WL_IOVAR_SETINT(name, "wme_dp", dp[1]);

	/* WME Tx parameters setting */
	{
		wme_tx_params_t txparams[AC_COUNT];
		char *srl, *sfbl, *lrl, *lfbl, *maxrate;
		char **locals[] = { &srl, &sfbl, &lrl, &lfbl, &maxrate };

		/* build request block */
		memset(txparams, 0, sizeof(txparams));

		for (j = 0; j < AC_COUNT; j++) {
			/* get packed nvram parameter */
			snprintf(nv, sizeof(nv), nv_name, prefix, mode[2].str, ac[j]);
			nv_value = nvram_safe_get(nv);
			strcpy(nv, nv_value);
			/* unpack it */
			v = nv;
			for (k = 0; k < (sizeof(locals) / sizeof(locals[0])); k++) {
				*locals[k] = v;
				while (*v && *v != ' ')
					v++;
				if (*v) {
					*v = 0;
					v++;
				}
			}

			/* update short retry limit */
			txparams[j].short_retry = atoi(srl);

			/* update short fallback limit */
			txparams[j].short_fallback = atoi(sfbl);

			/* update long retry limit */
			txparams[j].long_retry = atoi(lrl);

			/* update long fallback limit */
			txparams[j].long_fallback = atoi(lfbl);

			/* update max rate */
			txparams[j].max_rate = atoi(maxrate);
		}

		/* set the WME tx parameters */
		WL_IOVAR_SET(name, mode[2].req, txparams, sizeof(txparams));
	}

	return;
}

#if defined(linux) || defined(__NetBSD__)
#include <unistd.h>
static void
sleep_ms(const unsigned int ms)
{
	usleep(1000*ms);
}
#elif defined(__ECOS)
static void
sleep_ms(const unsigned int ms)
{
	cyg_tick_count_t ostick;

	ostick = ms / 10;
	cyg_thread_delay(ostick);
}
#else
#error "sleep_ms() not defined for this OS!!!"
#endif /* defined(linux) */

/*
* The following condition(s) must be met when Auto Channel Selection
* is enabled.
*  - the I/F is up (change radio channel requires it is up?)
*  - the AP must not be associated (setting SSID to empty should
*    make sure it for us)
*/
static uint8
wlconf_auto_channel(char *name)
{
	int chosen = 0;
	wl_uint32_list_t request;
	int phytype;
	int ret;
	int i;

	/* query the phy type */
	WL_GETINT(name, WLC_GET_PHYTYPE, &phytype);

	request.count = 0;	/* let the ioctl decide */
	WL_IOCTL(name, WLC_START_CHANNEL_SEL, &request, sizeof(request));
	if (!ret) {
		sleep_ms(phytype == PHY_TYPE_A ? 1000 : 750);
		for (i = 0; i < 100; i++) {
			WL_GETINT(name, WLC_GET_CHANNEL_SEL, &chosen);
			if (!ret)
				break;
			sleep_ms(100);
		}
	}
	WLCONF_DBG("interface %s: channel selected %d\n", name, chosen);
	return chosen;
}

static chanspec_t
wlconf_auto_chanspec(char *name)
{
	chanspec_t chosen = 0;
	int temp = 0;
	wl_uint32_list_t request;
	int ret;
	int i;

	request.count = 0;	/* let the ioctl decide */
	WL_IOCTL(name, WLC_START_CHANNEL_SEL, &request, sizeof(request));
	if (!ret) {
		/* this time needs to be < 1000 to prevent mpc kicking in for 2nd radio */
		sleep_ms(500);
		for (i = 0; i < 100; i++) {
			WL_IOVAR_GETINT(name, "apcschspec", &temp);
			if (!ret)
				break;
			sleep_ms(100);
		}
	}

	chosen = (chanspec_t) temp;
	WLCONF_DBG("interface %s: chanspec selected %04x\n", name, chosen);
	return chosen;
}

/* PHY type/BAND conversion */
#define WLCONF_PHYTYPE2BAND(phy)	((phy) == PHY_TYPE_A ? WLC_BAND_5G : WLC_BAND_2G)
/* PHY type conversion */
#define WLCONF_PHYTYPE2STR(phy)	((phy) == PHY_TYPE_A ? "a" : \
				 (phy) == PHY_TYPE_B ? "b" : \
				 (phy) == PHY_TYPE_LP ? "l" : \
				 (phy) == PHY_TYPE_G ? "g" : \
				 (phy) == PHY_TYPE_SSN ? "s" : \
				 (phy) == PHY_TYPE_HT ? "h" : \
				 (phy) == PHY_TYPE_AC ? "v" : \
				 (phy) == PHY_TYPE_LCN ? "c" : "n")
#define WLCONF_STR2PHYTYPE(ch)	((ch) == 'a' ? PHY_TYPE_A : \
				 (ch) == 'b' ? PHY_TYPE_B : \
				 (ch) == 'l' ? PHY_TYPE_LP : \
				 (ch) == 'g' ? PHY_TYPE_G : \
				 (ch) == 's' ? PHY_TYPE_SSN : \
				 (ch) == 'h' ? PHY_TYPE_HT : \
				 (ch) == 'v' ? PHY_TYPE_AC : \
				 (ch) == 'c' ? PHY_TYPE_LCN : PHY_TYPE_N)

#define PREFIX_LEN 32			/* buffer size for wlXXX_ prefix */

#define WLCONF_PHYTYPE_11N(phy) ((phy) == PHY_TYPE_N 	|| (phy) == PHY_TYPE_SSN || \
				 (phy) == PHY_TYPE_LCN 	|| (phy) == PHY_TYPE_HT || \
				 (phy) == PHY_TYPE_AC)

struct bsscfg_info {
	int idx;			/* bsscfg index */
	char ifname[PREFIX_LEN];	/* OS name of interface (debug only) */
	char prefix[PREFIX_LEN];	/* prefix for nvram params (eg. "wl0.1_") */
};

struct bsscfg_list {
	int count;
	struct bsscfg_info bsscfgs[WL_MAXBSSCFG];
};

struct bsscfg_list *
wlconf_get_bsscfgs(char* ifname, char* prefix)
{
	char var[80];
	char tmp[100];
	char *next;

	struct bsscfg_list *bclist;
	struct bsscfg_info *bsscfg;

	bclist = (struct bsscfg_list*)malloc(sizeof(struct bsscfg_list));
	if (bclist == NULL)
		return NULL;
	memset(bclist, 0, sizeof(struct bsscfg_list));

	/* Set up Primary BSS Config information */
	bsscfg = &bclist->bsscfgs[0];
	bsscfg->idx = 0;
	strncpy(bsscfg->ifname, ifname, PREFIX_LEN-1);
	strcpy(bsscfg->prefix, prefix);
	bclist->count = 1;

	/* additional virtual BSS Configs from wlX_vifs */
	foreach(var, nvram_safe_get(strcat_r(prefix, "vifs", tmp)), next) {
		if (bclist->count == WL_MAXBSSCFG) {
			WLCONF_DBG("wlconf(%s): exceeded max number of BSS Configs (%d)"
			           "in nvram %s\n"
			           "while configuring interface \"%s\"\n",
			           ifname, WL_MAXBSSCFG, strcat_r(prefix, "vifs", tmp), var);
			continue;
		}
		bsscfg = &bclist->bsscfgs[bclist->count];
		if (get_ifname_unit(var, NULL, &bsscfg->idx) != 0) {
			WLCONF_DBG("wlconfg(%s): unable to parse unit.subunit in interface "
			           "name \"%s\"\n",
			           ifname, var);
			continue;
		}
		strncpy(bsscfg->ifname, var, PREFIX_LEN-1);
		snprintf(bsscfg->prefix, PREFIX_LEN, "%s_", bsscfg->ifname);
		bclist->count++;
	}

	return bclist;
}

static void
wlconf_config_join_pref(char *name, int bsscfg_idx, int auth_val)
{
	int ret = 0, i = 0;

	if ((auth_val & (WPA_AUTH_UNSPECIFIED | WPA2_AUTH_UNSPECIFIED)) ||
	    CHECK_PSK(auth_val)) {
		uchar pref[] = {
		/* WPA pref, 14 tuples */
		0x02, 0xaa, 0x00, 0x0e,
		/* WPA2                 AES  (unicast)          AES (multicast) */
		0x00, 0x0f, 0xac, 0x01, 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x04,
		/* WPA                  AES  (unicast)          AES (multicast) */
		0x00, 0x50, 0xf2, 0x01, 0x00, 0x50, 0xf2, 0x04, 0x00, 0x50, 0xf2, 0x04,
		/* WPA2                 AES  (unicast)          TKIP (multicast) */
		0x00, 0x0f, 0xac, 0x01, 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x02,
		/* WPA                  AES  (unicast)          TKIP (multicast) */
		0x00, 0x50, 0xf2, 0x01, 0x00, 0x50, 0xf2, 0x04, 0x00, 0x50, 0xf2, 0x02,
		/* WPA2                 AES  (unicast)          WEP-40 (multicast) */
		0x00, 0x0f, 0xac, 0x01, 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x01,
		/* WPA                  AES  (unicast)          WEP-40 (multicast) */
		0x00, 0x50, 0xf2, 0x01, 0x00, 0x50, 0xf2, 0x04, 0x00, 0x50, 0xf2, 0x01,
		/* WPA2                 AES  (unicast)          WEP-128 (multicast) */
		0x00, 0x0f, 0xac, 0x01, 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x05,
		/* WPA                  AES  (unicast)          WEP-128 (multicast) */
		0x00, 0x50, 0xf2, 0x01, 0x00, 0x50, 0xf2, 0x04, 0x00, 0x50, 0xf2, 0x05,
		/* WPA2                 TKIP (unicast)          TKIP (multicast) */
		0x00, 0x0f, 0xac, 0x01, 0x00, 0x0f, 0xac, 0x02, 0x00, 0x0f, 0xac, 0x02,
		/* WPA                  TKIP (unicast)          TKIP (multicast) */
		0x00, 0x50, 0xf2, 0x01, 0x00, 0x50, 0xf2, 0x02, 0x00, 0x50, 0xf2, 0x02,
		/* WPA2                 TKIP (unicast)          WEP-40 (multicast) */
		0x00, 0x0f, 0xac, 0x01, 0x00, 0x0f, 0xac, 0x02, 0x00, 0x0f, 0xac, 0x01,
		/* WPA                  TKIP (unicast)          WEP-40 (multicast) */
		0x00, 0x50, 0xf2, 0x01, 0x00, 0x50, 0xf2, 0x02, 0x00, 0x50, 0xf2, 0x01,
		/* WPA2                 TKIP (unicast)          WEP-128 (multicast) */
		0x00, 0x0f, 0xac, 0x01, 0x00, 0x0f, 0xac, 0x02, 0x00, 0x0f, 0xac, 0x05,
		/* WPA                  TKIP (unicast)          WEP-128 (multicast) */
		0x00, 0x50, 0xf2, 0x01, 0x00, 0x50, 0xf2, 0x02, 0x00, 0x50, 0xf2, 0x05,
		/* RSSI pref */
		0x01, 0x02, 0x00, 0x00,
		};

		if (CHECK_PSK(auth_val)) {
			for (i = 0; i < pref[3]; i ++)
				pref[7 + i * 12] = 0x02;
		}

		WL_BSSIOVAR_SET(name, "join_pref", bsscfg_idx, pref, sizeof(pref));
	}

}

static void
wlconf_security_options(char *name, char *prefix, int bsscfg_idx, bool id_supp,
                        bool check_join_pref)
{
	int i;
	int val;
	int ret;
	char tmp[100];
	bool need_join_pref = FALSE;
#define AUTOWPA(cfg) ((cfg) == (WPA_AUTH_PSK | WPA2_AUTH_PSK))

	/* Set WSEC */
	/*
	* Need to check errors (card may have changed) and change to
	* defaults since the new chip may not support the requested
	* encryptions after the card has been changed.
	*/
	if (wlconf_set_wsec(name, prefix, bsscfg_idx)) {
		/* change nvram only, code below will pass them on */
		nvram_restore_var(prefix, "auth_mode");
		nvram_restore_var(prefix, "auth");
		/* reset wep to default */
		nvram_restore_var(prefix, "crypto");
		nvram_restore_var(prefix, "wep");
		wlconf_set_wsec(name, prefix, bsscfg_idx);
	}

	val = wlconf_akm_options(prefix);
	if (!nvram_match(strcat_r(prefix, "mode", tmp), "ap"))
		need_join_pref = (check_join_pref || id_supp) && AUTOWPA(val);

	if (need_join_pref)
		wlconf_config_join_pref(name, bsscfg_idx, val);

	/* enable in-driver wpa supplicant? */
	if (id_supp && (CHECK_PSK(val))) {
		wsec_pmk_t psk;
		char *key;

		if (((key = nvram_get(strcat_r(prefix, "wpa_psk", tmp))) != NULL) &&
		    (strlen(key) < WSEC_MAX_PSK_LEN)) {
			psk.key_len = (ushort) strlen(key);
			psk.flags = WSEC_PASSPHRASE;
			strcpy((char *)psk.key, key);
			WL_IOCTL(name, WLC_SET_WSEC_PMK, &psk, sizeof(psk));
		}
		wl_iovar_setint(name, "sup_wpa", 1);
	}

	if (!need_join_pref)
		WL_BSSIOVAR_SETINT(name, "wpa_auth", bsscfg_idx, val);

	/* EAP Restrict if we have an AKM or radius authentication */
	val = ((val != 0) || (nvram_match(strcat_r(prefix, "auth_mode", tmp), "radius")));
	WL_BSSIOVAR_SETINT(name, "eap_restrict", bsscfg_idx, val);

	/* Set WEP keys */
	if (nvram_match(strcat_r(prefix, "wep", tmp), "enabled")) {
		for (i = 1; i <= DOT11_MAX_DEFAULT_KEYS; i++)
			wlconf_set_wep_key(name, prefix, bsscfg_idx, i);
	}

	/* Set 802.11 authentication mode - open/shared */
	val = atoi(nvram_safe_get(strcat_r(prefix, "auth", tmp)));
	WL_BSSIOVAR_SETINT(name, "auth", bsscfg_idx, val);
#ifdef MFP
		/* Set MFP */
	val = WPA_AUTH_DISABLED;
	WL_BSSIOVAR_GET(name, "wpa_auth", bsscfg_idx, &val, sizeof(val));
	if (val & (WPA2_AUTH_PSK | WPA2_AUTH_UNSPECIFIED)) {
		val = atoi(nvram_safe_get(strcat_r(prefix, "mfp", tmp)));
		WL_BSSIOVAR_SETINT(name, "mfp", bsscfg_idx, val);
	}
#endif
}

static void
wlconf_set_ampdu_retry_limit(char *name, char *prefix)
{
	int i, j, ret, nv_len;
	struct ampdu_retry_tid retry_limit;
	char *nv_name[2] = {"ampdu_rtylimit_tid", "ampdu_rr_rtylimit_tid"};
	char *iov_name[2] = {"ampdu_retry_limit_tid", "ampdu_rr_retry_limit_tid"};
	char *retry, *v, *nv_value, nv[100], tmp[100];

	/* Get packed AMPDU (rr) retry limit per-tid from NVRAM if present */
	for (i = 0; i < 2; i++) {
		nv_value = nvram_safe_get(strcat_r(prefix, nv_name[i], tmp));
		nv_len = strlen(nv_value);
		strcpy(nv, nv_value);

		/* unpack it */
		v = nv;
		for (j = 0; nv_len >= 0 && j < NUMPRIO; j++) {
			retry = v;
			while (*v && *v != ' ') {
				v++;
				nv_len--;
			}
			if (*v) {
				*v = 0;
				v++;
				nv_len--;
			}
			/* set the AMPDU retry limit per-tid */
			retry_limit.tid = j;
			retry_limit.retry = atoi(retry);
			WL_IOVAR_SET(name, iov_name[i], &retry_limit, sizeof(retry_limit));
		}
	}

	return;
}

/*
 * When N-mode is ON, AMPDU, AMSDU are enabled/disabled
 * based on the nvram setting. Only one of the AMPDU or AMSDU options is enabled any
 * time. When N-mode is OFF or the device is non N-phy, AMPDU and AMSDU are turned off.
 *
 * WME/WMM is also set in this procedure as it depends on N.
 *     N ==> WMM is on by default
 *
 * Returns WME setting.
 */
static int
wlconf_ampdu_amsdu_set(char *name, char prefix[PREFIX_LEN], int nmode, int btc_mode, int ap)
{
	bool ampdu_valid_option = FALSE;
	bool amsdu_valid_option = FALSE;
	int  val, ampdu_option_val = OFF, amsdu_option_val = OFF;
	int rx_amsdu_in_ampdu_option_val = OFF;
	int wme_option_val = ON;  /* On by default */
	char caps[WLC_IOCTL_MEDLEN], var[80], *next, *wme_val;
	char buf[WLC_IOCTL_SMLEN];
	int len = (sizeof("amsdu") - 1);
	int ret, phytype;
	wlc_rev_info_t rev;
#ifdef linux
	struct utsname unamebuf;

	uname(&unamebuf);
#endif

	WL_IOCTL(name, WLC_GET_REVINFO, &rev, sizeof(rev));

	WL_GETINT(name, WLC_GET_PHYTYPE, &phytype);

	/* First, clear WMM settings to avoid conflicts */
	WL_IOVAR_SETINT(name, "wme", OFF);

	/* Get WME setting from NVRAM if present */
	wme_val = nvram_get(strcat_r(prefix, "wme", caps));
	if (wme_val && !strcmp(wme_val, "off")) {
		wme_option_val = OFF;
	}

	/* Set options based on capability */
	wl_iovar_get(name, "cap", (void *)caps, sizeof(caps));
	foreach(var, caps, next) {
		char *nvram_str;
		bool amsdu = 0;

		/* Check for the capabilitiy 'amsdutx' */
		if (strncmp(var, "amsdutx", sizeof(var)) == 0) {
			var[len] = '\0';
			amsdu = 1;
		}
		nvram_str = nvram_get(strcat_r(prefix, var, buf));
		if (!nvram_str)
			continue;

		if (!strcmp(nvram_str, "on"))
			val = ON;
		else if (!strcmp(nvram_str, "off"))
			val = OFF;
		else if (!strcmp(nvram_str, "auto"))
			val = AUTO;
		else
			continue;

		if (strncmp(var, "ampdu", sizeof(var)) == 0) {
			ampdu_valid_option = TRUE;
			ampdu_option_val = val;
		}

		if (amsdu) {
			amsdu_valid_option = TRUE;
			amsdu_option_val = val;

			nvram_str = nvram_get(strcat_r(prefix, "rx_amsdu_in_ampdu", buf));
			if (nvram_str) {
				if (!strcmp(nvram_str, "on"))
					rx_amsdu_in_ampdu_option_val = ON;
				else if (!strcmp(nvram_str, "auto"))
					rx_amsdu_in_ampdu_option_val = AUTO;
			}

			/*
			 * Some tests show problems when AMSDU is enabled on TX
			 * side together with ps-pretend.
			 * Ps-pretend is used on AP only.
			 * Let's disable AMSDU downstream unconditionally
			 * in devicemode=1.
			 * When ps-pretend be OK with AMSDU, below statement
			 * need to be removed.
			 */
			if (ap && !strcmp(nvram_safe_get("devicemode"), "1")) {
				amsdu_option_val = OFF;
			}
		}
	}

	if (nmode != OFF) { /* N-mode is ON/AUTO */
		if (ampdu_valid_option) {
			if (ampdu_option_val != OFF) {
				WL_IOVAR_SETINT(name, "amsdu", OFF);
				WL_IOVAR_SETINT(name, "ampdu", ampdu_option_val);
			} else {
				WL_IOVAR_SETINT(name, "ampdu", OFF);
			}

			wlconf_set_ampdu_retry_limit(name, prefix);

#ifdef linux
			/* For MIPS routers set the num mpdu per ampdu limit to 32. We observed
			 * that having this value at 32 helps with bi-di thru'put as well.
			 */
			if ((phytype == PHY_TYPE_AC) &&
			    (strncmp(unamebuf.machine, "mips", 4) == 0)) {
#ifndef __CONFIG_USBAP__
				WL_IOVAR_SETINT(name, "ampdu_mpdu", 32);
#endif /* __CONFIG_USBAP__ */
			}
#endif /* linux */
		}

		if (amsdu_valid_option) {
			if (amsdu_option_val != OFF) { /* AMPDU (above) has priority over AMSDU */
				if (rev.corerev >= 40) {
					WL_IOVAR_SETINT(name, "amsdu", amsdu_option_val);
				} else if (ampdu_option_val == OFF) {
					WL_IOVAR_SETINT(name, "ampdu", OFF);
					WL_IOVAR_SETINT(name, "amsdu", amsdu_option_val);
				}
			} else
				WL_IOVAR_SETINT(name, "amsdu", OFF);
		}

		WL_IOVAR_SETINT(name, "rx_amsdu_in_ampdu", rx_amsdu_in_ampdu_option_val);
	} else {
		/* When N-mode is off or for non N-phy device, turn off AMPDU, AMSDU;
		 */
		wl_iovar_setint(name, "amsdu", OFF);
		wl_iovar_setint(name, "ampdu", OFF);
	}

	if (wme_option_val) {
		WL_IOVAR_SETINT(name, "wme", wme_option_val);
		wlconf_set_wme(name, prefix);
	}

	return wme_option_val;
}

/* Get configured bandwidth cap. */
static int
wlconf_bw_cap(char *prefix, int bandtype)
{
	char *str, tmp[100];
	int bw_cap = WLC_BW_CAP_20MHZ;

	if ((str = nvram_get(strcat_r(prefix, "bw_cap", tmp))) != NULL)
		bw_cap = atoi(str);
	else {
		/* Backward compatibility. Map to bandwidth cap bitmap values. */
		int val = atoi(nvram_safe_get(strcat_r(prefix, "nbw_cap", tmp)));

		if (((bandtype == WLC_BAND_2G) && (val == WLC_N_BW_40ALL)) ||
		    ((bandtype == WLC_BAND_5G) &&
		     (val == WLC_N_BW_40ALL || val == WLC_N_BW_20IN2G_40IN5G)))
			bw_cap = WLC_BW_CAP_40MHZ;
		else
			bw_cap = WLC_BW_CAP_20MHZ;
	}

	return bw_cap;
}

/* Set up TxBF. Called when i/f is down. */
static void wlconf_set_txbf(char *name, char *prefix)
{
	char *str, tmp[100];
	wlc_rev_info_t rev;
	uint32 txbf_bfe_cap = 0;
	uint32 txbf_bfr_cap = 0;
	int ret = 0;

	WL_IOCTL(name, WLC_GET_REVINFO, &rev, sizeof(rev));

	if (rev.corerev < 40) return;	/* TxBF unsupported */

	if ((str = nvram_get(strcat_r(prefix, "txbf_bfr_cap", tmp))) != NULL) {
		txbf_bfr_cap = atoi(str);

		if (txbf_bfr_cap) {
			/* Turning TxBF on (order matters) */
			WL_IOVAR_SETINT(name, "txbf_bfr_cap", 1);
			WL_IOVAR_SETINT(name, "txbf", 1);
		} else {
			/* Similarly, turning TxBF off in reverse order */
			WL_IOVAR_SETINT(name, "txbf", 0);
			WL_IOVAR_SETINT(name, "txbf_bfr_cap", 0);
		}
	}

	if ((str = nvram_get(strcat_r(prefix, "txbf_bfe_cap", tmp))) != NULL) {
		txbf_bfe_cap = atoi(str);

		WL_IOVAR_SETINT(name, "txbf_bfe_cap", txbf_bfe_cap ? 1 : 0);
	}
}

/* Set up TxBF timer. Called when i/f is up. */
static void wlconf_set_txbf_timer(char *name, char *prefix)
{
	char *str, tmp[100];
	wlc_rev_info_t rev;
	uint32 txbf_timer = 0;
	int ret = 0;

	WL_IOCTL(name, WLC_GET_REVINFO, &rev, sizeof(rev));

	if (rev.corerev < 40) return;	/* TxBF unsupported */

	if ((str = nvram_get(strcat_r(prefix, "txbf_timer", tmp))) != NULL) {
		txbf_timer = (uint32) atoi(str);
		WL_IOVAR_SETINT(name, "txbf_timer", txbf_timer);
	}
}

/* Apply Traffic Management filter settings stored in NVRAM */
static void
trf_mgmt_settings(char *prefix, bool dwm_supported)
{
	char buffer[sizeof(trf_mgmt_filter_t)*(MAX_NUM_TRF_MGMT_RULES+1)];
	char iobuff[sizeof(trf_mgmt_filter_t)*(MAX_NUM_TRF_MGMT_RULES+1)+32];
	int i, filterlen, ret = 0;
	trf_mgmt_config_t trf_mgmt_config;
	trf_mgmt_filter_list_t *trf_mgmt_filter_list;
	netconf_trmgmt_t nettrm;
	trf_mgmt_filter_t *trfmgmt;
	char *wlifname;
	struct in_addr ipaddr, ipmask;
	char nvram_ifname[32];
	bool tm_filters_configured = FALSE;
	/* DWM variables */
	bool dwm_filters_configured = FALSE;
	char dscp_filter_buffer[sizeof(trf_mgmt_filter_t)*(MAX_NUM_TRF_MGMT_DWM_RULES)];
	char dscp_filter_iobuff[sizeof(trf_mgmt_filter_t)*(MAX_NUM_TRF_MGMT_DWM_RULES)+
		sizeof("trf_mgmt_filters_add") + OFFSETOF(trf_mgmt_filter_list_t, filter)];
	int dscp_filterlen;
	trf_mgmt_filter_t *trf_mgmt_dwm_filter;
	trf_mgmt_filter_list_t *trf_mgmt_dwm_filter_list = NULL;
	netconf_trmgmt_t nettrm_dwm;

	snprintf(nvram_ifname, sizeof(nvram_ifname), "%sifname", prefix);
	wlifname = nvram_get(nvram_ifname);
	if (!wlifname) {
		return;
	}

	trf_mgmt_filter_list = (trf_mgmt_filter_list_t *)buffer;
	trfmgmt = &trf_mgmt_filter_list->filter[0];

	/* Initialize the common parameters */
	memset(buffer, 0, sizeof(buffer));
	memset(&trf_mgmt_config, 0, sizeof(trf_mgmt_config_t));

	/* no-rx packets, local subnet, don't override priority and no-traffic shape */
	trf_mgmt_config.flags = (TRF_MGMT_FLAG_NO_RX |
		TRF_MGMT_FLAG_MANAGE_LOCAL_TRAFFIC |
		TRF_MGMT_FLAG_DISABLE_SHAPING);
	(void) inet_aton("192.168.1.1", &ipaddr);         /* Dummy value */
	(void) inet_aton("255.255.255.0", &ipmask);       /* Dummy value */
	trf_mgmt_config.host_ip_addr = ipaddr.s_addr;     /* Dummy value */
	trf_mgmt_config.host_subnet_mask = ipmask.s_addr; /* Dummy value */
	trf_mgmt_config.downlink_bandwidth = 1;           /* Dummy value */
	trf_mgmt_config.uplink_bandwidth = 1;             /* Dummy value */

	/* Read up to NUM_TFF_MGMT_FILTERS entries from NVRAM */
	for (i = 0; i < MAX_NUM_TRF_MGMT_RULES; i++) {
		if (get_trf_mgmt_port(prefix, i, &nettrm) == FALSE) {
			continue;
		}
		if (nettrm.match.flags == NETCONF_DISABLED) {
			continue;
		}
		trfmgmt->dst_port = ntohs(nettrm.match.dst.ports[0]);
		trfmgmt->src_port = ntohs(nettrm.match.src.ports[0]);
		trfmgmt->prot = nettrm.match.ipproto;
		if (nettrm.favored) {
			trfmgmt->flags |= TRF_FILTER_FAVORED;
		}
		trfmgmt->priority = nettrm.prio;
		memcpy(&trfmgmt->dst_ether_addr, &nettrm.match.mac, sizeof(struct ether_addr));
		if (nettrm.match.ipproto == IPPROTO_IP) {
			/* Enable MAC filter */
			trf_mgmt_config.flags |= TRF_MGMT_FLAG_FILTER_ON_MACADDR;
			trfmgmt->flags |= TRF_FILTER_MAC_ADDR;
		}
		trf_mgmt_filter_list->num_filters += 1;
		trfmgmt++;
	}
	if (trf_mgmt_filter_list->num_filters)
		tm_filters_configured = TRUE;

	if (dwm_supported) {
		trf_mgmt_dwm_filter_list = (trf_mgmt_filter_list_t *)dscp_filter_buffer;
		trf_mgmt_dwm_filter = &trf_mgmt_dwm_filter_list->filter[0];
		memset(dscp_filter_buffer, 0, sizeof(dscp_filter_buffer));

		/* Read up to NUM_TFF_MGMT_FILTERS entries from NVRAM */
		for (i = 0; i < MAX_NUM_TRF_MGMT_DWM_RULES; i++) {
			if (get_trf_mgmt_dwm(prefix, i, &nettrm_dwm) == FALSE) {
				continue;
			}
			if (nettrm_dwm.match.flags == NETCONF_DISABLED) {
				continue;
			}

			trf_mgmt_dwm_filter->dscp = nettrm_dwm.match.dscp;
			if (nettrm_dwm.favored)
				trf_mgmt_dwm_filter->flags |= TRF_FILTER_FAVORED;
			trf_mgmt_dwm_filter->flags |= TRF_FILTER_DWM;
			trf_mgmt_dwm_filter->priority = nettrm_dwm.prio;
			trf_mgmt_dwm_filter++;
			trf_mgmt_dwm_filter_list->num_filters += 1;
		}
		if (trf_mgmt_dwm_filter_list->num_filters)
			dwm_filters_configured = TRUE;
	}

	/* Disable traffic management module to initial known state */
	trf_mgmt_config.trf_mgmt_enabled = 0;
	WL_IOVAR_SET(wlifname, "trf_mgmt_config", &trf_mgmt_config, sizeof(trf_mgmt_config_t));

	/* Add traffic management filter entries */
	if (tm_filters_configured || dwm_filters_configured) {
		/* Enable traffic management module  before adding filter mappings */
		trf_mgmt_config.trf_mgmt_enabled = 1;
		WL_IOVAR_SET(wlifname, "trf_mgmt_config", &trf_mgmt_config,
			sizeof(trf_mgmt_config_t));

		if (tm_filters_configured) {
			/* Configure TM module filters mappings */
			filterlen = trf_mgmt_filter_list->num_filters *
				sizeof(trf_mgmt_filter_t) +
				OFFSETOF(trf_mgmt_filter_list_t, filter);
			wl_iovar_setbuf(wlifname, "trf_mgmt_filters_add", trf_mgmt_filter_list,
				filterlen, iobuff, sizeof(iobuff));
		}

		if (dwm_filters_configured) {
			dscp_filterlen =  trf_mgmt_dwm_filter_list->num_filters *
				sizeof(trf_mgmt_filter_t) +
				OFFSETOF(trf_mgmt_filter_list_t, filter);
			wl_iovar_setbuf(wlifname, "trf_mgmt_filters_add", trf_mgmt_dwm_filter_list,
			    dscp_filterlen, dscp_filter_iobuff, sizeof(dscp_filter_iobuff));
		}
	}
}

#ifdef TRAFFIC_MGMT_RSSI_POLICY
static void
trf_mgmt_rssi_policy(char *prefix)
{
	char *wlifname;
	char nvram_ifname[32];
	char rssi_policy[64];
	uint32 rssi_policy_value, ret;

	/* Get wl interface name */
	snprintf(nvram_ifname, sizeof(nvram_ifname), "%sifname", prefix);
	if ((wlifname = nvram_get(nvram_ifname)) == NULL) {
		return;
	}

	/* Get RSSI policy from NVRAM variable wlx_trf_mgmt_rssi_policy */
	snprintf(rssi_policy, sizeof(rssi_policy), "%strf_mgmt_rssi_policy", prefix);
	if (!nvram_invmatch(rssi_policy, ""))
		return;
	rssi_policy_value = atoi(nvram_get(rssi_policy));

	/* Enable/Disable RSSI policy depending on value of  rssi_policy_value */
	WL_IOVAR_SETINT(wlifname, "trf_mgmt_rssi_policy", rssi_policy_value);
}
#endif /* TRAFFIC_MGMT_RSSI_POLICY */

static int
wlconf_del_brcm_syscap_ie(char *name, int bsscfg_idx, char *oui)
{
	int iebuf_len = 0;
	vndr_ie_setbuf_t *ie_setbuf = NULL;
	int iecount, i;

	char getbuf[2048] = {0};
	vndr_ie_buf_t *iebuf;
	vndr_ie_info_t *ieinfo;
	char *bufaddr;
	int buflen = 0;
	int found = 0;
	uint32 pktflag;
	uint32 frametype;
	int ret = 0;

	frametype = VNDR_IE_BEACON_FLAG;

	WL_BSSIOVAR_GET(name, "vndr_ie", bsscfg_idx, getbuf, 2048);
	iebuf = (vndr_ie_buf_t *)getbuf;

	bufaddr = (char*)iebuf->vndr_ie_list;

	for (i = 0; i < iebuf->iecount; i++) {
		ieinfo = (vndr_ie_info_t *)bufaddr;
		bcopy((char*)&ieinfo->pktflag, (char*)&pktflag, (int)sizeof(uint32));
		if (pktflag == frametype) {
			if (!memcmp(ieinfo->vndr_ie_data.oui, oui, DOT11_OUI_LEN)) {
				found = 1;
				bufaddr = (char*) &ieinfo->vndr_ie_data;
				buflen = (int)ieinfo->vndr_ie_data.len + VNDR_IE_HDR_LEN;
				break;
			}
		}
		bufaddr = (char *)(ieinfo->vndr_ie_data.oui + ieinfo->vndr_ie_data.len);
	}

	if (!found)
		goto err;

	iebuf_len = buflen + sizeof(vndr_ie_setbuf_t) - sizeof(vndr_ie_t);
	ie_setbuf = (vndr_ie_setbuf_t *)malloc(iebuf_len);
	if (!ie_setbuf) {
		WLCONF_DBG("memory alloc failure\n");
		ret = -1;
		goto err;
	}

	memset(ie_setbuf, 0, iebuf_len);

	/* Copy the vndr_ie SET command ("add"/"del") to the buffer */
	strcpy(ie_setbuf->cmd, "del");

	/* Buffer contains only 1 IE */
	iecount = 1;
	memcpy(&ie_setbuf->vndr_ie_buffer.iecount, &iecount, sizeof(int));

	memcpy(&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].pktflag, &frametype, sizeof(uint32));

	memcpy(&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data, bufaddr, buflen);

	WL_BSSIOVAR_SET(name, "vndr_ie", bsscfg_idx, ie_setbuf, iebuf_len);

err:
	if (ie_setbuf)
		free(ie_setbuf);

	return ret;
}

static int
wlconf_set_brcm_syscap_ie(char *name, int bsscfg_idx, char *oui, uchar *data, int datalen)
{
	vndr_ie_setbuf_t *ie_setbuf = NULL;
	unsigned int pktflag;
	int buflen, iecount;
	int ret = 0;

	pktflag = VNDR_IE_BEACON_FLAG;

	buflen = sizeof(vndr_ie_setbuf_t) + datalen - 1;
	ie_setbuf = (vndr_ie_setbuf_t *)malloc(buflen);
	if (!ie_setbuf) {
		WLCONF_DBG("memory alloc failure\n");
		ret = -1;
		goto err;
	}

	memset(ie_setbuf, 0, buflen);

	/* Copy the vndr_ie SET command ("add"/"del") to the buffer */
	strcpy(ie_setbuf->cmd, "add");

	/* Buffer contains only 1 IE */
	iecount = 1;
	memcpy(&ie_setbuf->vndr_ie_buffer.iecount, &iecount, sizeof(int));

	/* The packet flag bit field indicates the packets that will contain this IE */
	memcpy(&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].pktflag, &pktflag, sizeof(uint32));

	/* Now, add the IE to the buffer, +1: one byte OUI_TYPE */
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.len = DOT11_OUI_LEN + datalen;

	memcpy(&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[0], oui, DOT11_OUI_LEN);
	if (datalen > 0)
		memcpy(&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.data[0], data,
		       datalen);

	ret = wlconf_del_brcm_syscap_ie(name, bsscfg_idx, oui);
	if (ret)
		goto err;

	WL_BSSIOVAR_SET(name, "vndr_ie", (int)bsscfg_idx, ie_setbuf, buflen);

err:
	if (ie_setbuf)
		free(ie_setbuf);

	return (ret);
}

/*
 * wlconf_process_sta_config_entry() - process a single sta_config settings entry.
 *
 *	This function processes a single sta_config settings entry by parsing the entry and
 *	calling the appropriate IOVAR(s) to apply the settings in the driver.
 *
 * Inputs:
 *	params	- address of a nul terminated ascii string buffer containing a single sta_config
 *		  settings entry in the form "xx:xx:xx:xx:xx:xx,prio[,steerflag]".
 *
 *	At least the mac address of the station to which the settings are to be applied needs
 *	to be present, with one or more setting values, in a specific order. New settings must
 *	be added at the end. Alternatively, we could allow "prio=<value>,steerflag=<value>" and
 *	so on, but that would take some more parsing and use more nvram space.
 *
 * Outputs:
 *	Driver settings may be updated.
 *
 * Returns:
 *	 This function returns a BCME_xxx status indicating success (BCME_OK) or failure.
 *
 * Side effects: The input buffer is trashed by the strsep() function.
 *
 */
static int
wlconf_process_sta_config_entry(char *ifname, char *param_list)
{
	enum {	/* Parameter index in comma-separated list of settings. */
		PARAM_MACADDRESS = 0,
		PARAM_PRIO	 = 1,
		PARAM_STEERFLAG	 = 2,
		PARAM_COUNT
	} param_idx = PARAM_MACADDRESS;
	char *param;
	struct ether_addr ea;
	char *end;
	uint32 value;

	while ((param = strsep(&param_list, ","))) {
		switch (param_idx) {

		case PARAM_MACADDRESS: /* MAC Address - parse into ea */
			if (!param || !ether_atoe(param, &ea.octet[0])) {
				return BCME_BADADDR;
			}
			break;

		case PARAM_PRIO: /* prio value - parse and apply through "staprio" iovar */
			if (*param) { /* If no value is provided, do not configure the prio */
				wl_staprio_cfg_t staprio_arg;
				int ret;

				value = strtol(param, &end, 0);
				if (*end != '\0') {
					return BCME_BADARG;
				}
				memset(&staprio_arg, 0, sizeof(staprio_arg));
				memcpy(&staprio_arg.ea, &ea, sizeof(ea));
				staprio_arg.prio = value; /* prio is byte sized, no htod() needed */
				WL_IOVAR_SET(ifname, "staprio", &staprio_arg, sizeof(staprio_arg));
			}
			break;

		case PARAM_STEERFLAG:
			if (*param) {
				value = strtol(param, &end, 0);
				if (*end != '\0') {
					return BCME_BADARG;
				}
			}
			break;

		default:
			/* Future use parameter already set in nvram config - ignore. */
			break;
		}
		++param_idx;
	}

	if (param_idx <= PARAM_PRIO) { /* No mac address and/or no parameters at all, forget it. */
		return BCME_BADARG;
	}

	return BCME_OK;
}

#define VIFNAME_LEN 16

/* configure the specified wireless interface */
int
wlconf(char *name)
{
	int restore_defaults, val, unit, phytype, bandtype, gmode = 0, ret = 0;
	int bcmerr;
	int error_bg, error_a;
	struct bsscfg_list *bclist = NULL;
	struct bsscfg_info *bsscfg = NULL;
	char tmp[100], tmp2[100], prefix[PREFIX_LEN];
	char var[80], *next, *str, *addr = NULL;
	/* Pay attention to buffer length requirements when using this */
	char buf[WLC_IOCTL_SMLEN] __attribute__ ((aligned(4)));
	char *country;
	char *country_rev;
	wlc_rev_info_t rev;
	channel_info_t ci;
	struct maclist *maclist;
	struct ether_addr *ea;
	wlc_ssid_t ssid;
	wl_rateset_t rs;
	unsigned int i;
	char eaddr[32];
	int ap, apsta, wds, sta = 0, wet = 0, mac_spoof = 0, wmf = 0;
	int rxchain_pwrsave = 0, radio_pwrsave = 0;
	wl_country_t country_spec = {{0}, 0, {0}};
	char *ba;
	char *preauth;
	int set_preauth;
	int wlunit = -1;
	int wlsubunit = -1;
	int wl_ap_build = 0; /* wl compiled with AP capabilities */
	char cap[WLC_IOCTL_SMLEN];
	char caps[WLC_IOCTL_MEDLEN];
	int btc_mode;
	uint32 leddc;
	uint nbw = WL_CHANSPEC_BW_20;
	int nmode = OFF; /* 802.11n support */
	char vif_addr[WLC_IOCTL_SMLEN];
	int max_no_vifs = 0;
	int wme_global;
	int max_assoc = -1;
	bool ure_enab = FALSE;
	bool radar_enab = FALSE;
	bool obss_coex = FALSE, psta, psr;
	chanspec_t chanspec = 0;
	int wet_tunnel_cap = 0, wet_tunnel_enable = 0;
	brcm_prop_ie_t brcm_syscap_ie;

	/* wlconf doesn't work for virtual i/f, so if we are given a
	 * virtual i/f return 0 if that interface is in it's parent's "vifs"
	 * list otherwise return -1
	 */
	if (get_ifname_unit(name, &wlunit, &wlsubunit) == 0) {
		if (wlsubunit >= 0) {
			/* we have been given a virtual i/f,
			 * is it in it's parent i/f's virtual i/f list?
			 */
			sprintf(tmp, "wl%d_vifs", wlunit);

			if (strstr(nvram_safe_get(tmp), name) == NULL)
				return -1; /* config error */
			else
				return 0; /* okay */
		}
	} else {
		return -1;
	}

	/* clean up tmp */
	memset(tmp, 0, sizeof(tmp));

	/* because of ifdefs in wl driver,  when we don't have AP capabilities we
	 * can't use the same iovars to configure the wl.
	 * so we use "wl_ap_build" to help us know how to configure the driver
	 */
	if (wl_iovar_get(name, "cap", (void *)caps, sizeof(caps)))
		return -1;

	foreach(cap, caps, next) {
		if (!strcmp(cap, "ap")) {
			wl_ap_build = 1;
		} else if (!strcmp(cap, "mbss16"))
			max_no_vifs = 16;
		else if (!strcmp(cap, "mbss8"))
			max_no_vifs = 8;
		else if (!strcmp(cap, "mbss4"))
			max_no_vifs = 4;
		else if (!strcmp(cap, "wmf"))
			wmf = 1;
		else if (!strcmp(cap, "rxchain_pwrsave"))
			rxchain_pwrsave = 1;
		else if (!strcmp(cap, "radio_pwrsave"))
			radio_pwrsave = 1;
		else if (!strcmp(cap, "wet_tunnel"))
			wet_tunnel_cap = 1;
	}

	/* Check interface (fail silently for non-wl interfaces) */
	if ((ret = wl_probe(name)))
		return ret;

	/* Get MAC address */
	(void) wl_hwaddr(name, (uchar *)buf);
	memcpy(vif_addr, buf, ETHER_ADDR_LEN);

	/* Get instance */
	WL_IOCTL(name, WLC_GET_INSTANCE, &unit, sizeof(unit));
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	/* Restore defaults if per-interface parameters do not exist */
	restore_defaults = !nvram_get(strcat_r(prefix, "ifname", tmp));
	nvram_validate_all(prefix, restore_defaults);
	nvram_set(strcat_r(prefix, "ifname", tmp), name);
	nvram_set(strcat_r(prefix, "hwaddr", tmp), ether_etoa((uchar *)buf, eaddr));
	snprintf(buf, sizeof(buf), "%d", unit);
	nvram_set(strcat_r(prefix, "unit", tmp), buf);

	if (restore_defaults) {
		wlconf_set_current_txparam_into_nvram(name, prefix);
	}
#ifdef BCMDBG
	/* Apply message level */
	if (nvram_invmatch("wl_msglevel", "")) {
		val = (int)strtoul(nvram_get("wl_msglevel"), NULL, 0);

		if (nvram_invmatch("wl_msglevel2", "")) {
			struct wl_msglevel2 msglevel64;
			msglevel64.low = val;
			val = (int)strtoul(nvram_get("wl_msglevel2"), NULL, 0);
			msglevel64.high = val;
			WL_IOVAR_SET(name, "msglevel", &msglevel64, sizeof(struct wl_msglevel2));
		}
		else
			WL_IOCTL(name, WLC_SET_MSGLEVEL, &val, sizeof(val));
	}
#endif

	/* Bring the interface down */
	WL_IOCTL(name, WLC_DOWN, NULL, sizeof(val));

	/* Disable all BSS Configs */
	for (i = 0; i < WL_MAXBSSCFG; i++) {
		struct {int bsscfg_idx; int enable;} setbuf;
		setbuf.bsscfg_idx = i;
		setbuf.enable = 0;

		ret = wl_iovar_set(name, "bss", &setbuf, sizeof(setbuf));
		if (ret) {
			wl_iovar_getint(name, "bcmerror", &bcmerr);
			/* fail quietly on a range error since the driver may
			 * support fewer bsscfgs than we are prepared to configure
			 */
			if (bcmerr == BCME_RANGE)
				break;
		}
		if (ret) {
			WLCONF_DBG("%d:(%s): setting bsscfg #%d iovar \"bss\" to 0"
			           " (down) failed, ret = %d, bcmerr = %d\n",
			           __LINE__, name, i, ret, bcmerr);
		}
	}

	/* Get the list of BSS Configs */
	bclist = wlconf_get_bsscfgs(name, prefix);
	if (bclist == NULL) {
		ret = -1;
		goto exit;
	}

#ifdef BCMDBG
	strcat_r(prefix, "vifs", tmp);
	printf("BSS Config summary: primary -> \"%s\", %s -> \"%s\"\n", name, tmp,
	       nvram_safe_get(tmp));
	for (i = 0; i < bclist->count; i++) {
		printf("BSS Config \"%s\": index %d\n",
		       bclist->bsscfgs[i].ifname, bclist->bsscfgs[i].idx);
	}
#endif

	/* create a wlX.Y_ifname nvram setting */
	for (i = 1; i < bclist->count; i++) {
		bsscfg = &bclist->bsscfgs[i];
#if defined(linux) || defined(__ECOS) || defined(__NetBSD__)
		strcpy(var, bsscfg->ifname);
#endif
		nvram_set(strcat_r(bsscfg->prefix, "ifname", tmp), var);
	}

	str = nvram_safe_get(strcat_r(prefix, "mode", tmp));

	/* If ure_disable is not present or is 1, ure is not enabled;
	 * that is, if it is present and 0, ure is enabled.
	 */
	if (!strcmp(nvram_safe_get("ure_disable"), "0")) { /* URE is enabled */
		ure_enab = TRUE;
	}
	if (wl_ap_build) {
		/* Enable MBSS mode if appropriate. */
		if (!ure_enab && strcmp(str, "psr")) {
#ifndef __CONFIG_USBAP__
			WL_IOVAR_SETINT(name, "mbss", (bclist->count >= 1));
#else
			WL_IOVAR_SETINT(name, "mbss", (bclist->count >= 2));
#endif
		} else
			WL_IOVAR_SETINT(name, "mbss", 0);

		/*
		 * Set SSID for each BSS Config
		 */
		for (i = 0; i < bclist->count; i++) {
			bsscfg = &bclist->bsscfgs[i];
			strcat_r(bsscfg->prefix, "ssid", tmp);
			ssid.SSID_len = strlen(nvram_safe_get(tmp));
			if (ssid.SSID_len > sizeof(ssid.SSID))
				ssid.SSID_len = sizeof(ssid.SSID);
			strncpy((char *)ssid.SSID, nvram_safe_get(tmp), ssid.SSID_len);
			WLCONF_DBG("wlconfig(%s): configuring bsscfg #%d (%s) "
			           "with SSID \"%s\"\n", name, bsscfg->idx,
			           bsscfg->ifname, nvram_safe_get(tmp));
			WL_BSSIOVAR_SET(name, "ssid", bsscfg->idx, &ssid,
			                sizeof(ssid));
		}
	}

	/* Create addresses for VIFs */
	if (!ure_enab && strcmp(str, "psr")) {
		/* set local bit for our MBSS vif base */
		ETHER_SET_LOCALADDR(vif_addr);

		/* construct and set other wlX.Y_hwaddr */
		for (i = 1; i < max_no_vifs; i++) {
			snprintf(tmp, sizeof(tmp), "wl%d.%d_hwaddr", unit, i);
			addr = nvram_safe_get(tmp);
			if (!strcmp(addr, "")) {
				vif_addr[5] = (vif_addr[5] & ~(max_no_vifs-1))
				        | ((max_no_vifs-1) & (vif_addr[5]+1));

				nvram_set(tmp, ether_etoa((uchar *)vif_addr, eaddr));
			}
		}

		for (i = 0; i < bclist->count; i++) {
			bsscfg = &bclist->bsscfgs[i];
			/* Ignore primary */
			if (bsscfg->idx == 0)
				continue;

			snprintf(tmp, sizeof(tmp), "wl%d.%d_hwaddr", unit, bsscfg->idx);
			ether_atoe(nvram_safe_get(tmp), (unsigned char *)eaddr);
			WL_BSSIOVAR_SET(name, "cur_etheraddr", bsscfg->idx, eaddr, ETHER_ADDR_LEN);
		}
	} else { /* One of URE or Proxy STA Repeater is enabled */
		/* URE/PSR is on, so set wlX.1 hwaddr is same as that of primary interface */
		snprintf(tmp, sizeof(tmp), "wl%d.1_hwaddr", unit);
		WL_BSSIOVAR_SET(name, "cur_etheraddr", 1, vif_addr,
		                ETHER_ADDR_LEN);
		nvram_set(tmp, ether_etoa((uchar *)vif_addr, eaddr));
	}

	/* wlX_mode settings: AP, STA, WET, BSS/IBSS, APSTA */
	str = nvram_safe_get(strcat_r(prefix, "mode", tmp));
	ap = (!strcmp(str, "") || !strcmp(str, "ap"));
	apsta = (!strcmp(str, "apsta") ||
	         ((!strcmp(str, "sta") || !strcmp(str, "psr") || !strcmp(str, "wet")) &&
	          bclist->count > 1));
	sta = (!strcmp(str, "sta") && bclist->count == 1);
	wds = !strcmp(str, "wds");
	wet = !strcmp(str, "wet");
	mac_spoof = !strcmp(str, "mac_spoof");
	psta = !strcmp(str, "psta");
	psr = !strcmp(str, "psr");

	/* set apsta var first, because APSTA mode takes precedence */
	WL_IOVAR_SETINT(name, "apsta", apsta);

	/* Set AP mode */
	val = (ap || apsta || wds) ? 1 : 0;
	WL_IOCTL(name, WLC_SET_AP, &val, sizeof(val));

	/* Turn WET mode ON or OFF based on selected mode */
	WL_IOCTL(name, WLC_SET_WET, &wet, sizeof(wet));

	if (mac_spoof) {
		sta = 1;
		WL_IOVAR_SETINT(name, "mac_spoof", 1);
	}

	/* For STA configurations, configure association retry time.
	 * Use specified time (capped), or mode-specific defaults.
	 */
	if (sta || wet || apsta || psta || psr) {
		char *sta_retry_time_name = "sta_retry_time";
		char *assoc_retry_max_name = "assoc_retry_max";
		struct {
			int val;
			int band;
		} roam;

		str = nvram_safe_get(strcat_r(prefix, sta_retry_time_name, tmp));
		WL_IOVAR_SETINT(name, sta_retry_time_name, atoi(str));

		/* Set the wlX_assoc_retry_max, but only if one was specified. */
		if ((str = nvram_get(strcat_r(prefix, assoc_retry_max_name, tmp)))) {
			WL_IOVAR_SETINT(name, assoc_retry_max_name, atoi(str));
		}

		roam.val = WLC_ROAM_NEVER_ROAM_TRIGGER;
		roam.band = WLC_BAND_ALL;
		WL_IOCTL(name, WLC_SET_ROAM_TRIGGER, &roam, sizeof(roam));
	}

	/* Retain remaining WET effects only if not APSTA */
	wet &= !apsta;

	/* Set infra: BSS/IBSS (IBSS only for WET or STA modes) */
	val = 1;
	if (wet || sta || psta || psr)
		val = atoi(nvram_safe_get(strcat_r(prefix, "infra", tmp)));
	WL_IOCTL(name, WLC_SET_INFRA, &val, sizeof(val));

	/* Set DWDS only for AP or STA modes */
	for (i = 0; i < bclist->count; i++) {
		val = 0;
		bsscfg = &bclist->bsscfgs[i];

		if (ap || sta || psta || psr || (apsta && !wet)) {
			strcat_r(bsscfg->prefix, "dwds", tmp);
			val = atoi(nvram_safe_get(tmp));
		}
		WL_BSSIOVAR_SETINT(name, "dwds", bsscfg->idx, val);
	}

	/* Set The AP MAX Associations Limit */
	if (ap || apsta) {
		max_assoc = val = atoi(nvram_safe_get(strcat_r(prefix, "maxassoc", tmp)));
		if (val > 0) {
			WL_IOVAR_SETINT(name, "maxassoc", val);
		} else { /* Get value from driver if not in nvram */
			WL_IOVAR_GETINT(name, "maxassoc", &max_assoc);
		}
	}
	if (!wet && !sta)
		WL_IOVAR_SETINT(name, "mpc", OFF);

	/* Set the Proxy STA or Repeater mode */
	if (psta) {
		WL_IOVAR_SETINT(name, "psta", PSTA_MODE_PROXY);
	} else if (psr) {
		WL_IOVAR_SETINT(name, "psta", PSTA_MODE_REPEATER);
		val = atoi(nvram_safe_get(strcat_r(prefix, "psr_mrpt", tmp)));
		WL_IOVAR_SETINT(name, "psta_mrpt", val);
	} else {
		WL_IOVAR_SETINT(name, "psta", PSTA_MODE_DISABLED);
	}

	/* Turn WET tunnel mode ON or OFF */
	if ((ap || apsta) && (wet_tunnel_cap)) {
		if (atoi(nvram_safe_get(strcat_r(prefix, "wet_tunnel", tmp))) == 1) {
			WL_IOVAR_SETINT(name, "wet_tunnel", 1);
			wet_tunnel_enable = 1;
		} else {
			WL_IOVAR_SETINT(name, "wet_tunnel", 0);
		}
	}

	for (i = 0; i < bclist->count; i++) {
		char *subprefix;
		bsscfg = &bclist->bsscfgs[i];

		/* XXXMSSID: The note about setting preauth now does not seem right.
		 * NAS brings the BSS up if it runs, so setting the preauth value
		 * will make it in the bcn/prb. If that is right, we can move this
		 * chunk out of wlconf.
		 */
		/*
		 * Set The WPA2 Pre auth cap. only reason we are doing it here is the driver is down
		 * if we do it in the NAS we need to bring down the interface and up to make
		 * it affect in the  beacons
		 */
		if (ap || (apsta && bsscfg->idx != 0)) {
			set_preauth = 1;
			preauth = nvram_safe_get(strcat_r(bsscfg->prefix, "preauth", tmp));
			if (strlen (preauth) != 0) {
				set_preauth = atoi(preauth);
			}
			wlconf_set_preauth(name, bsscfg->idx, set_preauth);
		}

		/* Clear BRCM System level Capability IE */
		memset(&brcm_syscap_ie, 0, sizeof(brcm_prop_ie_t));
		brcm_syscap_ie.type = BRCM_SYSCAP_IE_TYPE;

		/* Add WET TUNNEL to IE */
		if (wet_tunnel_enable)
			brcm_syscap_ie.cap |= BRCM_SYSCAP_WET_TUNNEL;

		subprefix = apsta ? prefix : bsscfg->prefix;

		if (ap || (apsta && bsscfg->idx != 0)) {
			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "bss_maxassoc", tmp)));
			if (val > 0) {
				WL_BSSIOVAR_SETINT(name, "bss_maxassoc", bsscfg->idx, val);
			} else if (max_assoc > 0) { /* Set maxassoc same as global if not set */
				snprintf(var, sizeof(var), "%d", max_assoc);
				nvram_set(tmp, var);
			}
		}

		/* Set network type */
		val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "closed", tmp)));
		WL_BSSIOVAR_SETINT(name, "closednet", bsscfg->idx, val);

		/* Set the ap isolate mode */
		val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "ap_isolate", tmp)));
		WL_BSSIOVAR_SETINT(name, "ap_isolate", bsscfg->idx, val);

		/* Set the WMF enable mode */
		if (wmf) {
			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "wmf_bss_enable", tmp)));
			WL_BSSIOVAR_SETINT(name, "wmf_bss_enable", bsscfg->idx, val);

			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix,
				"wmf_psta_disable", tmp)));
			WL_BSSIOVAR_SETINT(name, "wmf_psta_disable", bsscfg->idx, val);
		}

		/* Set the Multicast Reverse Translation enable mode */
		if (wet || psta || psr) {
			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix,
							 "mcast_regen_bss_enable", tmp)));
			WL_BSSIOVAR_SETINT(name, "mcast_regen_bss_enable", bsscfg->idx, val);
		}

		if (rxchain_pwrsave) {
			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "rxchain_pwrsave_enable",
				tmp)));
			WL_BSSIOVAR_SETINT(name, "rxchain_pwrsave_enable", bsscfg->idx, val);

			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix,
				"rxchain_pwrsave_quiet_time", tmp)));
			WL_BSSIOVAR_SETINT(name, "rxchain_pwrsave_quiet_time", bsscfg->idx, val);

			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "rxchain_pwrsave_pps",
				tmp)));
			WL_BSSIOVAR_SETINT(name, "rxchain_pwrsave_pps", bsscfg->idx, val);

			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix,
				"rxchain_pwrsave_stas_assoc_check", tmp)));
			WL_BSSIOVAR_SETINT(name, "rxchain_pwrsave_stas_assoc_check", bsscfg->idx,
				val);
		}

		if (radio_pwrsave) {
			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "radio_pwrsave_enable",
				tmp)));
			WL_BSSIOVAR_SETINT(name, "radio_pwrsave_enable", bsscfg->idx, val);

			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix,
				"radio_pwrsave_quiet_time", tmp)));
			WL_BSSIOVAR_SETINT(name, "radio_pwrsave_quiet_time", bsscfg->idx, val);

			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "radio_pwrsave_pps",
				tmp)));
			WL_BSSIOVAR_SETINT(name, "radio_pwrsave_pps", bsscfg->idx, val);
			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "radio_pwrsave_level",
				tmp)));
			WL_BSSIOVAR_SETINT(name, "radio_pwrsave_level", bsscfg->idx, val);

			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix,
				"radio_pwrsave_stas_assoc_check", tmp)));
			WL_BSSIOVAR_SETINT(name, "radio_pwrsave_stas_assoc_check", bsscfg->idx,
				val);
		}
		val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "aspm", tmp)));
		WL_BSSIOVAR_SETINT(name, "aspm", bsscfg->idx, val);

		/* Configure SYSCAP IE to driver */
		if (brcm_syscap_ie.cap)  {
			wlconf_set_brcm_syscap_ie(name, bsscfg->idx,
				BRCM_PROP_OUI, (uchar *)&(brcm_syscap_ie.type),
				sizeof(brcm_syscap_ie.type) +
				sizeof(brcm_syscap_ie.cap));
		}
	}

	/* Set up the country code */
	(void) strcat_r(prefix, "country_code", tmp);
	country = nvram_get(tmp);
	(void) strcat_r(prefix, "country_rev", tmp2);
	country_rev = nvram_get(tmp2);
	if ((country && country[0] != '\0') && (country_rev && country_rev[0] != '\0')) {
		/* Initialize the wl country parameter */
		strncpy(country_spec.country_abbrev, country, WLC_CNTRY_BUF_SZ-1);
		country_spec.country_abbrev[WLC_CNTRY_BUF_SZ-1] = '\0';
		strncpy(country_spec.ccode, country, WLC_CNTRY_BUF_SZ-1);
		country_spec.ccode[WLC_CNTRY_BUF_SZ-1] = '\0';
		country_spec.rev = atoi(country_rev);

		WL_IOVAR_SET(name, "country", &country_spec, sizeof(country_spec));
	} else {
		/* Get the default country code if undefined */
		wl_iovar_get(name, "country", &country_spec, sizeof(country_spec));

		/* Add the new NVRAM variable */
		nvram_set("wl_country_code", country_spec.ccode);
		(void) strcat_r(prefix, "country_code", tmp);
		nvram_set(tmp, country_spec.ccode);
		snprintf(buf, sizeof(buf),  "%d", country_spec.rev);
		nvram_set("wl_country_rev", buf);
		(void) strcat_r(prefix, "country_rev", tmp);
		nvram_set(tmp, buf);
	}

	/* Change LED Duty Cycle */
	leddc = (uint32)strtoul(nvram_safe_get(strcat_r(prefix, "leddc", tmp)), NULL, 16);
	if (leddc)
		WL_IOVAR_SETINT(name, "leddc", leddc);

	/* Enable or disable the radio */
	val = nvram_match(strcat_r(prefix, "radio", tmp), "0");
	val += WL_RADIO_SW_DISABLE << 16;
	WL_IOCTL(name, WLC_SET_RADIO, &val, sizeof(val));

	/* Get supported phy types */
	WL_IOCTL(name, WLC_GET_PHYLIST, var, sizeof(var));
	nvram_set(strcat_r(prefix, "phytypes", tmp), var);

	/* Get radio IDs */
	*(next = buf) = '\0';
	for (i = 0; i < strlen(var); i++) {
		/* Switch to band */
		val = WLCONF_STR2PHYTYPE(var[i]);
		if (WLCONF_PHYTYPE_11N(val)) {
			WL_GETINT(name, WLC_GET_BAND, &val);
		} else
			val = WLCONF_PHYTYPE2BAND(val);
		WL_IOCTL(name, WLC_SET_BAND, &val, sizeof(val));
		/* Get radio ID on this band */
		WL_IOCTL(name, WLC_GET_REVINFO, &rev, sizeof(rev));
		next += sprintf(next, "%sBCM%X", i ? " " : "",
		                (rev.radiorev & IDCODE_ID_MASK) >> IDCODE_ID_SHIFT);
	}
	nvram_set(strcat_r(prefix, "radioids", tmp), buf);

	/* Set band */
	str = nvram_get(strcat_r(prefix, "phytype", tmp));
	val = str ? WLCONF_STR2PHYTYPE(str[0]) : PHY_TYPE_G;
	/* For NPHY use band value from NVRAM */
	if (WLCONF_PHYTYPE_11N(val)) {
		str = nvram_get(strcat_r(prefix, "nband", tmp));
		if (str)
			val = atoi(str);
		else {
			WL_GETINT(name, WLC_GET_BAND, &val);
		}
	} else
		val = WLCONF_PHYTYPE2BAND(val);

	WL_SETINT(name, WLC_SET_BAND, val);

	/* Check errors (card may have changed) */
	if (ret) {
		/* default band to the first band in band list */
		val = WLCONF_STR2PHYTYPE(var[0]);
		val = WLCONF_PHYTYPE2BAND(val);
		WL_SETINT(name, WLC_SET_BAND, val);
	}

	/* Store the resolved bandtype */
	bandtype = val;

	/* Check errors again (will cover 5Ghz-only cards) */
	if (ret) {
		int list[3];

		/* default band to the first band in band list */
		wl_ioctl(name, WLC_GET_BANDLIST, list, sizeof(list));
		WL_SETINT(name, WLC_SET_BAND, list[1]);

		/* Read it back, and set bandtype accordingly */
		WL_GETINT(name, WLC_GET_BAND, &bandtype);
	}

	/* Get current core revision */
	WL_IOCTL(name, WLC_GET_REVINFO, &rev, sizeof(rev));
	snprintf(buf, sizeof(buf), "%d", rev.corerev);
	nvram_set(strcat_r(prefix, "corerev", tmp), buf);

	if ((rev.chipnum == BCM4716_CHIP_ID) || (rev.chipnum == BCM47162_CHIP_ID) ||
		(rev.chipnum == BCM4748_CHIP_ID) || (rev.chipnum == BCM4331_CHIP_ID) ||
		(rev.chipnum == BCM43431_CHIP_ID) || (rev.chipnum == BCM5357_CHIP_ID) ||
	        (rev.chipnum == BCM53572_CHIP_ID) || (rev.chipnum == BCM43236_CHIP_ID)) {
		int pam_mode = WLC_N_PREAMBLE_GF_BRCM; /* default GF-BRCM */

		strcat_r(prefix, "mimo_preamble", tmp);
		if (nvram_match(tmp, "mm"))
			pam_mode = WLC_N_PREAMBLE_MIXEDMODE;
		else if (nvram_match(tmp, "gf"))
			pam_mode = WLC_N_PREAMBLE_GF;
		else if (nvram_match(tmp, "auto"))
			pam_mode = -1;
		WL_IOVAR_SETINT(name, "mimo_preamble", pam_mode);
	}

	if ((rev.chipnum == BCM5357_CHIP_ID) || (rev.chipnum == BCM53572_CHIP_ID)) {
		val = atoi(nvram_safe_get("coma_sleep"));
		if (val > 0) {
			struct {int sleep; int delay;} setbuf;
			nvram_unset("coma_sleep");
			nvram_commit();
			setbuf.sleep = val;
			setbuf.delay = 1;
			WL_IOVAR_SET(name, "coma", &setbuf, sizeof(setbuf));
		}
	}

	/* Get current phy type */
	WL_IOCTL(name, WLC_GET_PHYTYPE, &phytype, sizeof(phytype));
	snprintf(buf, sizeof(buf), "%s", WLCONF_PHYTYPE2STR(phytype));
	nvram_set(strcat_r(prefix, "phytype", tmp), buf);

	/* Setup regulatory mode */
	strcat_r(prefix, "reg_mode", tmp);
	if (nvram_match(tmp, "off")) {
		val = 0;
		WL_IOCTL(name, WLC_SET_REGULATORY, &val, sizeof(val));
		WL_IOCTL(name, WLC_SET_RADAR, &val, sizeof(val));
		WL_IOCTL(name, WLC_SET_SPECT_MANAGMENT, &val, sizeof(val));
	} else if (nvram_match(tmp, "h") || nvram_match(tmp, "strict_h")) {
		val = 0;
		WL_IOCTL(name, WLC_SET_REGULATORY, &val, sizeof(val));
		val = 1;
		WL_IOCTL(name, WLC_SET_RADAR, &val, sizeof(val));
		radar_enab = TRUE;
		if (nvram_match(tmp, "h"))
			val = 1;
		else
			val = 2;
		WL_IOCTL(name, WLC_SET_SPECT_MANAGMENT, &val, sizeof(val));

		/* Set the CAC parameters, if they exist in nvram. */
		if ((str = nvram_get(strcat_r(prefix, "dfs_preism", tmp)))) {
			val = atoi(str);
			wl_iovar_setint(name, "dfs_preism", val);
		}
		if ((str = nvram_get(strcat_r(prefix, "dfs_postism", tmp)))) {
			val = atoi(str);
			wl_iovar_setint(name, "dfs_postism", val);
		}
		val = atoi(nvram_safe_get(strcat_r(prefix, "tpc_db", tmp)));
		WL_IOCTL(name, WLC_SEND_PWR_CONSTRAINT, &val, sizeof(val));

	} else if (nvram_match(tmp, "d")) {
		val = 0;
		WL_IOCTL(name, WLC_SET_RADAR, &val, sizeof(val));
		WL_IOCTL(name, WLC_SET_SPECT_MANAGMENT, &val, sizeof(val));
		val = 1;
		WL_IOCTL(name, WLC_SET_REGULATORY, &val, sizeof(val));
	}

	/* set bandwidth capability for nphy and calculate nbw */
	if (WLCONF_PHYTYPE_11N(phytype)) {
		struct {
			int bandtype;
			uint8 bw_cap;
		} param;

		/* Get the user nmode setting now */
		nmode = AUTO;	/* enable by default for NPHY */
		/* Set n mode */
		strcat_r(prefix, "nmode", tmp);
		if (nvram_match(tmp, "0"))
			nmode = OFF;

		WL_IOVAR_SETINT(name, "nmode", (uint32)nmode);

		if (nmode == OFF)
			val = WLC_BW_CAP_20MHZ;
		else
			val = wlconf_bw_cap(prefix, bandtype);

		/* record the bw here */
		if (val == WLC_BW_CAP_80MHZ)
			nbw = WL_CHANSPEC_BW_80;
		else if (val == WLC_BW_CAP_40MHZ)
			nbw = WL_CHANSPEC_BW_40;

		param.bandtype = bandtype;
		param.bw_cap = (uint8) val;

		WL_IOVAR_SET(name, "bw_cap", &param, sizeof(param));
	} else {
		/* Save n mode to OFF */
		nvram_set(strcat_r(prefix, "nmode", tmp), "0");
	}

	/* Use chanspec to set the channel */
	if ((str = nvram_get(strcat_r(prefix, "chanspec", tmp))) != NULL) {
		chanspec = wf_chspec_aton(str);

		if (chanspec) {
			WL_IOVAR_SETINT(name, "chanspec", (uint32)chanspec);
		}
	}

	/* Legacy method of setting channels (for compatibility) */
	/* Set channel before setting gmode or rateset */
	/* Manual Channel Selection - when channel # is not 0 */
	val = atoi(nvram_safe_get(strcat_r(prefix, "channel", tmp)));
	if ((chanspec == 0) && val && !WLCONF_PHYTYPE_11N(phytype)) {
		WL_SETINT(name, WLC_SET_CHANNEL, val);
		if (ret) {
			/* Use current channel (card may have changed) */
			WL_IOCTL(name, WLC_GET_CHANNEL, &ci, sizeof(ci));
			snprintf(buf, sizeof(buf), "%d", ci.target_channel);
			nvram_set(strcat_r(prefix, "channel", tmp), buf);
		}
	} else if ((chanspec == 0) && val && WLCONF_PHYTYPE_11N(phytype)) {
		uint channel;
		uint nctrlsb = 0;
		uint cspecbw = (bandtype == WLC_BAND_2G) ?
			WL_CHANSPEC_BAND_2G:WL_CHANSPEC_BAND_5G;

		channel = val;

		if (nbw == WL_CHANSPEC_BW_80) {
			/* Get Ctrl SB for 80MHz channel */
			str = nvram_safe_get(strcat_r(prefix, "nctrlsb", tmp));

			/* Adjust the channel to be center channel */
			channel = channel + CH_40MHZ_APART - CH_10MHZ_APART;

			if (!strcmp(str, "ll")) {
				nctrlsb = WL_CHANSPEC_CTL_SB_LL;
			} else if (!strcmp(str, "lu")) {
				nctrlsb = WL_CHANSPEC_CTL_SB_LU;
				channel -= CH_20MHZ_APART;
			} else if (!strcmp(str, "ul")) {
				nctrlsb = WL_CHANSPEC_CTL_SB_UL;
				channel -= 2 * CH_20MHZ_APART;
			} else if (!strcmp(str, "uu")) {
				nctrlsb = WL_CHANSPEC_CTL_SB_UU;
				channel -= 3 * CH_20MHZ_APART;
			}

		} else if (nbw == WL_CHANSPEC_BW_40) {
			/* Get Ctrl SB for 40MHz channel */
			str = nvram_safe_get(strcat_r(prefix, "nctrlsb", tmp));

			/* Adjust the channel to be center channel */
			if (!strcmp(str, "lower")) {
				nctrlsb = WL_CHANSPEC_CTL_SB_LOWER;
				channel = channel + 2;
			} else if (!strcmp(str, "upper")) {
				nctrlsb = WL_CHANSPEC_CTL_SB_UPPER;
				channel = channel - 2;
			}
		}

		/* band | BW | CTRL SB | Channel */
		chanspec |= (cspecbw | nbw | nctrlsb | channel);
		WL_IOVAR_SETINT(name, "chanspec", (uint32)chanspec);
	}

	/* Set up number of Tx and Rx streams */
	if (WLCONF_PHYTYPE_11N(phytype)) {
		int count;
		int streams;

		/* Get the number of tx chains supported by the hardware */
		wl_iovar_getint(name, "hw_txchain", &count);
		/* update NVRAM with capabilities */
		snprintf(var, sizeof(var), "%d", count);
		nvram_set(strcat_r(prefix, "hw_txchain", tmp), var);

		/* Verify that there is an NVRAM param for txstreams, if not create it and
		 * set it to hw_txchain
		 */
		streams = atoi(nvram_safe_get(strcat_r(prefix, "txchain", tmp)));
		if (streams == 0) {
			/* invalid - NVRAM needs to be fixed/initialized */
			nvram_set(strcat_r(prefix, "txchain", tmp), var);
			streams = count;
		}
		/* Apply user configured txstreams, use 1 if user disabled nmode */
		WL_IOVAR_SETINT(name, "txchain", streams);

		wl_iovar_getint(name, "hw_rxchain", &count);
		/* update NVRAM with capabilities */
		snprintf(var, sizeof(var), "%d", count);
		nvram_set(strcat_r(prefix, "hw_rxchain", tmp), var);

		/* Verify that there is an NVRAM param for rxstreams, if not create it and
		 * set it to hw_txchain
		 */
		streams = atoi(nvram_safe_get(strcat_r(prefix, "rxchain", tmp)));
		if (streams == 0) {
			/* invalid - NVRAM needs to be fixed/initialized */
			nvram_set(strcat_r(prefix, "rxchain", tmp), var);
			streams = count;
		}

		/* Apply user configured rxstreams, use 1 if user disabled nmode */
		WL_IOVAR_SETINT(name, "rxchain", streams);
	}

	/* Reset to hardware rateset (band may have changed) */
	WL_IOCTL(name, WLC_GET_RATESET, &rs, sizeof(wl_rateset_t));
	WL_IOCTL(name, WLC_SET_RATESET, &rs, sizeof(wl_rateset_t));

	/* Set gmode */
	if (bandtype == WLC_BAND_2G) {
		int override = WLC_PROTECTION_OFF;
		int control = WLC_PROTECTION_CTL_OFF;

		/* Set gmode */
		gmode = atoi(nvram_safe_get(strcat_r(prefix, "gmode", tmp)));
		WL_IOCTL(name, WLC_SET_GMODE, &gmode, sizeof(gmode));

		/* Set gmode protection override and control algorithm */
		strcat_r(prefix, "gmode_protection", tmp);
		if (nvram_match(tmp, "auto")) {
			override = WLC_PROTECTION_AUTO;
			control = WLC_PROTECTION_CTL_OVERLAP;
		}
		WL_IOCTL(name, WLC_SET_GMODE_PROTECTION_OVERRIDE, &override, sizeof(override));
		WL_IOCTL(name, WLC_SET_PROTECTION_CONTROL, &control, sizeof(control));
	}

	/* Set nmode_protection */
	if (WLCONF_PHYTYPE_11N(phytype)) {
		int override = WLC_PROTECTION_OFF;
		int control = WLC_PROTECTION_CTL_OFF;

		/* Set n protection override and control algorithm */
		str = nvram_get(strcat_r(prefix, "nmode_protection", tmp));
		if (!str || !strcmp(str, "auto")) {
			override = WLC_PROTECTION_AUTO;
			control = WLC_PROTECTION_CTL_OVERLAP;
		}

		WL_IOVAR_SETINT(name, "nmode_protection_override",
		                (uint32)override);
		WL_IOCTL(name, WLC_SET_PROTECTION_CONTROL, &control, sizeof(control));
	}

	/* Set vlan_prio_mode */
	{
		uint32 mode = OFF; /* default */

		strcat_r(prefix, "vlan_prio_mode", tmp);

		if (nvram_match(tmp, "on"))
			mode = ON;

		WL_IOVAR_SETINT(name, "vlan_mode", mode);
	}

	/* Get bluetooth coexistance(BTC) mode */
	btc_mode = atoi(nvram_safe_get(strcat_r(prefix, "btc_mode", tmp)));

	/* Set the AMPDU and AMSDU options based on the N-mode */
	wme_global = wlconf_ampdu_amsdu_set(name, prefix, nmode, btc_mode, ap);

	/* Now that wme_global is known, check per-BSS disable settings */
	for (i = 0; i < bclist->count; i++) {
		char *subprefix;
		bsscfg = &bclist->bsscfgs[i];

		subprefix = apsta ? prefix : bsscfg->prefix;

		/* For each BSS, check WME; make sure wme is set properly for this interface */
		strcat_r(subprefix, "wme", tmp);
		nvram_set(tmp, wme_global ? "on" : "off");

		str = nvram_safe_get(strcat_r(bsscfg->prefix, "wme_bss_disable", tmp));
		val = (str[0] == '1') ? 1 : 0;
		WL_BSSIOVAR_SETINT(name, "wme_bss_disable", bsscfg->idx, val);
	}

	/*
	* Set operational capabilities required for stations
	* to associate to the BSS. Per-BSS setting.
	*/
	for (i = 0; i < bclist->count; i++) {
		bsscfg = &bclist->bsscfgs[i];
		str = nvram_safe_get(strcat_r(bsscfg->prefix, "bss_opmode_cap_reqd", tmp));
		val = atoi(str);
		WL_BSSIOVAR_SETINT(name, "mode_reqd", bsscfg->idx, val);
	}


	/* Get current rateset (gmode may have changed) */
	WL_IOCTL(name, WLC_GET_CURR_RATESET, &rs, sizeof(wl_rateset_t));

	strcat_r(prefix, "rateset", tmp);
	if (nvram_match(tmp, "all")) {
		/* Make all rates basic */
		for (i = 0; i < rs.count; i++)
			rs.rates[i] |= 0x80;
	} else if (nvram_match(tmp, "12")) {
		/* Make 1 and 2 basic */
		for (i = 0; i < rs.count; i++) {
			if ((rs.rates[i] & 0x7f) == 2 || (rs.rates[i] & 0x7f) == 4)
				rs.rates[i] |= 0x80;
			else
				rs.rates[i] &= ~0x80;
		}
	}

	if (phytype != PHY_TYPE_SSN && phytype != PHY_TYPE_LCN) {
		/* Set BTC mode */
		if (!wl_iovar_setint(name, "btc_mode", btc_mode)) {
			if (btc_mode == WL_BTC_PREMPT) {
				wl_rateset_t rs_tmp = rs;
				/* remove 1Mbps and 2 Mbps from rateset */
				for (i = 0, rs.count = 0; i < rs_tmp.count; i++) {
					if ((rs_tmp.rates[i] & 0x7f) == 2 ||
					    (rs_tmp.rates[i] & 0x7f) == 4)
						continue;
					rs.rates[rs.count++] = rs_tmp.rates[i];
				}
			}
		}
	}

	/* Set rateset */
	WL_IOCTL(name, WLC_SET_RATESET, &rs, sizeof(wl_rateset_t));

	/* Allow short preamble settings for the following:
	 * 11b - short/long
	 * 11g - short /long in GMODE_LEGACY_B and GMODE_AUTO gmodes
	 *	 GMODE_PERFORMANCE and GMODE_LRS will use short and long
	 *	 preambles respectively, by default
	 * 11n - short/long applicable in 2.4G band only
	 */
	if (phytype == PHY_TYPE_B ||
	    (WLCONF_PHYTYPE_11N(phytype) && (bandtype == WLC_BAND_2G)) ||
	    ((phytype == PHY_TYPE_G || phytype == PHY_TYPE_LP) &&
	     (gmode == GMODE_LEGACY_B || gmode == GMODE_AUTO))) {
		strcat_r(prefix, "plcphdr", tmp);
		if (nvram_match(tmp, "long"))
			val = WLC_PLCP_AUTO;
		else
			val = WLC_PLCP_SHORT;
		WL_IOCTL(name, WLC_SET_PLCPHDR, &val, sizeof(val));
	}

	/* Set rate in 500 Kbps units */
	val = atoi(nvram_safe_get(strcat_r(prefix, "rate", tmp))) / 500000;

	/* Convert Auto mcsidx to Auto rate */
	if (WLCONF_PHYTYPE_11N(phytype)) {
		int mcsidx = atoi(nvram_safe_get(strcat_r(prefix, "nmcsidx", tmp)));

		/* -1 mcsidx used to designate AUTO rate */
		if (mcsidx == -1)
			val = 0;
	}

	/* 1Mbps and 2 Mbps are not allowed in BTC pre-emptive mode */
	if (btc_mode == WL_BTC_PREMPT && (val == 2 || val == 4))
		/* Must b/g band.  Set to 5.5Mbps */
		val = 11;

	/* it is band-blind. try both band */
	error_bg = wl_iovar_setint(name, "bg_rate", val);
	error_a = wl_iovar_setint(name, "a_rate", val);

	if (error_bg && error_a) {
		/* both failed. Try default rate (card may have changed) */
		val = 0;

		error_bg = wl_iovar_setint(name, "bg_rate", val);
		error_a = wl_iovar_setint(name, "a_rate", val);

		snprintf(buf, sizeof(buf), "%d", val);
		nvram_set(strcat_r(prefix, "rate", tmp), buf);
	}

	/* check if nrate needs to be applied */
	if (nmode != OFF) {
		uint32 nrate = 0;
		int mcsidx = atoi(nvram_safe_get(strcat_r(prefix, "nmcsidx", tmp)));
		bool ismcs = (mcsidx >= 0);

		/* mcsidx of 32 is valid only for 40 Mhz */
		if (mcsidx == 32 && nbw == WL_CHANSPEC_BW_20) {
			mcsidx = -1;
			ismcs = FALSE;
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-1");
		}

		/* Use nrate iovar only for MCS rate. */
		if (ismcs) {
			nrate |= WL_RSPEC_ENCODE_HT;
			nrate |= mcsidx & WL_RSPEC_RATE_MASK;

			WL_IOVAR_SETINT(name, "nrate", nrate);
		}
	}

	/* Set multicast rate in 500 Kbps units */
	val = atoi(nvram_safe_get(strcat_r(prefix, "mrate", tmp))) / 500000;
	/* 1Mbps and 2 Mbps are not allowed in BTC pre-emptive mode */
	if (btc_mode == WL_BTC_PREMPT && (val == 2 || val == 4))
		/* Must b/g band.  Set to 5.5Mbps */
		val = 11;

	/* it is band-blind. try both band */
	error_bg = wl_iovar_setint(name, "bg_mrate", val);
	error_a = wl_iovar_setint(name, "a_mrate", val);

	if (error_bg && error_a) {
		/* Try default rate (card may have changed) */
		val = 0;

		wl_iovar_setint(name, "bg_mrate", val);
		wl_iovar_setint(name, "a_mrate", val);

		snprintf(buf, sizeof(buf), "%d", val);
		nvram_set(strcat_r(prefix, "mrate", tmp), buf);
	}

	/* Set fragmentation threshold */
	val = atoi(nvram_safe_get(strcat_r(prefix, "frag", tmp)));
	wl_iovar_setint(name, "fragthresh", val);

	/* Set RTS threshold */
	val = atoi(nvram_safe_get(strcat_r(prefix, "rts", tmp)));
	wl_iovar_setint(name, "rtsthresh", val);

	/* Set DTIM period */
	val = atoi(nvram_safe_get(strcat_r(prefix, "dtim", tmp)));
	WL_IOCTL(name, WLC_SET_DTIMPRD, &val, sizeof(val));

	/* Set beacon period */
	val = atoi(nvram_safe_get(strcat_r(prefix, "bcn", tmp)));
	WL_IOCTL(name, WLC_SET_BCNPRD, &val, sizeof(val));

	/* Set beacon rotation */
	str = nvram_get(strcat_r(prefix, "bcn_rotate", tmp));
	if (!str) {
		/* No nvram variable found, use the default */
		str = nvram_default_get(strcat_r(prefix, "bcn_rotate", tmp));
	}
	val = atoi(str);
	wl_iovar_setint(name, "bcn_rotate", val);

	/* Set framebursting mode */
	if (btc_mode == WL_BTC_PREMPT)
		val = FALSE;
	else
		val = nvram_match(strcat_r(prefix, "frameburst", tmp), "on");
	WL_IOCTL(name, WLC_SET_FAKEFRAG, &val, sizeof(val));

	/* Set dynamic frameburst max station limit */
	str = nvram_get(strcat_r(prefix, "frameburst_dyn_max_stations", tmp));
	if (!str) { /* Fall back to previous name, frameburst_dyn */
		str = nvram_safe_get("frameburst_dyn");
	}
	wl_iovar_setint(name, "frameburst_dyn_max_stations", atoi(str));

	/* Set STBC tx and rx mode */
	if (phytype == PHY_TYPE_N ||
		phytype == PHY_TYPE_HT ||
		phytype == PHY_TYPE_AC) {
		char *nvram_str = nvram_safe_get(strcat_r(prefix, "stbc_tx", tmp));

		if (!strcmp(nvram_str, "auto")) {
			WL_IOVAR_SETINT(name, "stbc_tx", AUTO);
		} else if (!strcmp(nvram_str, "on")) {
			WL_IOVAR_SETINT(name, "stbc_tx", ON);
		} else if (!strcmp(nvram_str, "off")) {
			WL_IOVAR_SETINT(name, "stbc_tx", OFF);
		}
		val = atoi(nvram_safe_get(strcat_r(prefix, "stbc_rx", tmp)));
		WL_IOVAR_SETINT(name, "stbc_rx", val);
	}

	/* Set RIFS mode based on framebursting */
	if (WLCONF_PHYTYPE_11N(phytype)) {
		char *nvram_str = nvram_safe_get(strcat_r(prefix, "rifs", tmp));
		if (!strcmp(nvram_str, "on"))
			wl_iovar_setint(name, "rifs", ON);
		else if (!strcmp(nvram_str, "off"))
			wl_iovar_setint(name, "rifs", OFF);

		/* RIFS mode advertisement */
		nvram_str = nvram_safe_get(strcat_r(prefix, "rifs_advert", tmp));
		if (!strcmp(nvram_str, "auto"))
			wl_iovar_setint(name, "rifs_advert", AUTO);
		else if (!strcmp(nvram_str, "off"))
			wl_iovar_setint(name, "rifs_advert", OFF);
	}

	/* Override BA mode only if set to on/off */
	ba = nvram_safe_get(strcat_r(prefix, "ba", tmp));
	if (!strcmp(ba, "on"))
		wl_iovar_setint(name, "ba", ON);
	else if (!strcmp(ba, "off"))
		wl_iovar_setint(name, "ba", OFF);

	if (WLCONF_PHYTYPE_11N(phytype)) {
		val = AVG_DMA_XFER_RATE;
		wl_iovar_set(name, "avg_dma_xfer_rate", &val, sizeof(val));
	}

	/*
	 * If no nvram variable exists to force non-aggregated mpdu regulation on/off,
	 * limit to 2G interfaces.
	 */
	str = nvram_get(strcat_r(prefix, "nar", tmp));
	if (str) {
		val = atoi(str);
	} else {
		val = (bandtype == WLC_BAND_2G) ? 1 : 0;
	}
	WLCONF_DBG("%sabling non-aggregated regulation on band %d\n", (val) ? "En":"Dis", bandtype);
	WL_IOVAR_SETINT(name, "nar", val);
	if (val) {
		/* nar is enabled on this interface, add tuneable parameters */
		str = nvram_get(strcat_r(prefix, "nar_handle_ampdu", tmp));
		if (str) {
			WL_IOVAR_SETINT(name, "nar_handle_ampdu", atoi(str));
		}
		str = nvram_get(strcat_r(prefix, "nar_transit_limit", tmp));
		if (str) {
			WL_IOVAR_SETINT(name, "nar_transit_limit", atoi(str));
		}
	}

	/* Set up TxBF */
	wlconf_set_txbf(name, prefix);

	/* set airtime fairness */
	val = 0;
	str = nvram_get(strcat_r(prefix, "atf", tmp));
	if (str) {
		val = atoi(str);
	}
	WL_IOVAR_SETINT(name, "atf", val);

	str = nvram_get(strcat_r(prefix, "ampdu_atf_us", tmp));
	if (str) {
		val = atoi(str);
		if (val) {
			WL_IOVAR_SETINT(name, "ampdu_atf_us", val);
			WL_IOVAR_SETINT(name, "nar_atf_us", val);
		}
	}

	/* set TAF */
	val = 0;
	str = nvram_get(strcat_r(prefix, "taf_enable", tmp));
	if (str && (bandtype == WLC_BAND_5G)) {
		val = atoi(str);
	}
	WL_IOVAR_SETINT(name, "taf", val);

	val = 0;
	str = nvram_get(strcat_r(prefix, "taf_rule", tmp));
	if (str) {
		val = strtoul(str, NULL, 0);
		WL_IOVAR_SETINT(name, "taf_rule", val);
	}

	/* Bring the interface back up */
	WL_IOCTL(name, WLC_UP, NULL, 0);

	/* set ebos, interface must be up */
	val = 0;
	str = nvram_get(strcat_r(prefix, "ebos_enable", tmp));
	if (str) {
		val = atoi(str);
	}
	WL_IOVAR_SETINT(name, "ebos_enable", val);

	val = 0;
	str = nvram_get(strcat_r(prefix, "ebos_flags", tmp));
	if (str) {
		val = strtoul(str, NULL, 0);
		WL_IOVAR_SETINT(name, "ebos_flags", val);
	}

	val = 0;
	str = nvram_get(strcat_r(prefix, "ebos_prr_threshold", tmp));
	if (str) {
		val = strtoul(str, NULL, 0);
		WL_IOVAR_SETINT(name, "ebos_prr_threshold", val);
	}

	val = 0;
	str = nvram_get(strcat_r(prefix, "ebos_prr_flags", tmp));
	if (str) {
		val = strtoul(str, NULL, 0);
		WL_IOVAR_SETINT(name, "ebos_prr_flags", val);
	}

	val = 0;
	str = nvram_get(strcat_r(prefix, "ebos_prr_transit", tmp));
	if (str) {
		val = strtol(str, NULL, 0);
		WL_IOVAR_SETINT(name, "ebos_prr_transit", val);
	}
	val = 0;

	str = nvram_get(strcat_r(prefix, "ebos_transit", tmp));
	if (str) {
		val = strtol(str, NULL, 0);
		WL_IOVAR_SETINT(name, "ebos_transit", val);
	}

	/* set phy_percal_delay */
	val = atoi(nvram_safe_get(strcat_r(prefix, "percal_delay", tmp)));
	if (val) {
		wl_iovar_set(name, "phy_percal_delay", &val, sizeof(val));
	}

	/* Set phy periodic cal if nvram present. Otherwise, use driver defaults. */
	str = nvram_get(strcat_r(prefix, "cal_period", tmp));
	if (str) {
		/* user specified phy cal period. */
		val = atoi(str);
		WL_IOVAR_SET(name, "cal_period", &val, sizeof(val));
	}

	/* Set antenna */
	val = atoi(nvram_safe_get(strcat_r(prefix, "antdiv", tmp)));
	WL_IOCTL(name, WLC_SET_ANTDIV, &val, sizeof(val));

	/* Set antenna selection */
	wlconf_set_antsel(name, prefix);

	/* Set radar parameters if it is enabled */
	if (radar_enab) {
		wlconf_set_radarthrs(name, prefix);
	}

	/* set pspretend */
	val = 0;
	if (ap) {
		/* Set pspretend for multi-ssid bss */
		for (i = 0; i < bclist->count; i++) {
			bsscfg = &bclist->bsscfgs[i];
			str = nvram_safe_get(strcat_r(bsscfg->prefix,
				"pspretend_retry_limit", tmp));
			if (str) {
				val = atoi(str);
				WL_BSSIOVAR_SETINT(name, "pspretend_retry_limit", bsscfg->idx, val);
			}
		}

		/* now set it for primary bss */
		val = 0;
		str = nvram_get(strcat_r(prefix, "pspretend_retry_limit", tmp));
		if (str) {
			val = atoi(str);
		}
	}
	WL_IOVAR_SETINT(name, "pspretend_retry_limit", val);

	val = 0;
	if (ap) {
		str = nvram_get(strcat_r(prefix, "pspretend_threshold", tmp));
		if (str) {
			val = atoi(str);
		}
	}
	WL_IOVAR_SETINT(name, "pspretend_threshold", val);

	/* Set channel interference threshold value if it is enabled */

	str = nvram_get(strcat_r(prefix, "glitchthres", tmp));

	if (str) {
		int glitch_thres = atoi(str);
		if (glitch_thres > 0)
			WL_IOVAR_SETINT(name, "chanim_glitchthres", glitch_thres);
	}

	str = nvram_get(strcat_r(prefix, "ccathres", tmp));

	if (str) {
		int cca_thres = atoi(str);
		if (cca_thres > 0)
			WL_IOVAR_SETINT(name, "chanim_ccathres", cca_thres);
	}

	str = nvram_get(strcat_r(prefix, "chanimmode", tmp));

	if (str) {
		int chanim_mode = atoi(str);
		if (chanim_mode >= 0)
			WL_IOVAR_SETINT(name, "chanim_mode", chanim_mode);
	}

	/* bcm_dcs (dynamic channel selection) settings */
	str = nvram_safe_get(strcat_r(prefix, "bcmdcs", tmp));
	if (!strcmp(str, "on"))
		wl_iovar_setint(name, "bcm_dcs", ON);
	else if (!strcmp(str, "off"))
		wl_iovar_setint(name, "bcm_dcs", OFF);

	/* Overlapping BSS Coexistence aka 20/40 Coex. aka OBSS Coex.
	 * For an AP - Only use if 2G band AND user wants a 40Mhz chanspec.
	 * For a STA - Always
	 */
	if (WLCONF_PHYTYPE_11N(phytype)) {
		if (sta ||
		    ((ap || apsta) && (nbw == WL_CHANSPEC_BW_40) && (bandtype == WLC_BAND_2G))) {
			str = nvram_safe_get(strcat_r(prefix, "obss_coex", tmp));
			if (!str) {
				/* No nvram variable found, use the default */
				str = nvram_default_get(strcat_r(prefix, "obss_coex", tmp));
			}
			obss_coex = atoi(str);
		} else {
			/* Need to disable obss coex in case of 20MHz and/or
			 * in case of 5G.
			 */
			obss_coex = 0;
		}
#ifdef WLTEST
		/* force coex off for msgtest build */
		obss_coex = 0;
#endif
		WL_IOVAR_SETINT(name, "obss_coex", obss_coex);
	}

	/* Set up TxBF timer */
	wlconf_set_txbf_timer(name, prefix);

	/* Auto Channel Selection:
	 * 1. When channel # is 0 in AP mode, this determines our channel and 20Mhz vs. 40Mhz
	 * 2. If we're running OBSS Coex and the user specified a channel, Autochannel runs to
	 *    do an initial scan to help us make decisions about whether we can create a 40Mhz AP
	 */
	/* The following condition(s) must be met in order for Auto Channel Selection to work.
	 *  - the I/F must be up for the channel scan
	 *  - the AP must not be supporting a BSS (all BSS Configs must be disabled)
	 */
	if (ap || apsta) {
		int channel = chanspec ? wf_chspec_ctlchan(chanspec) : 0;
#ifdef EXT_ACS
		char tmp[100];
		char *ptr;
		char * str_val;

		str_val = nvram_safe_get("acs_mode");
		if (!strcmp(str_val, "legacy"))
			goto legacy_mode;

		snprintf(tmp, sizeof(tmp), "acs_ifnames");
		ptr = nvram_get(tmp);
		if (ptr)
			snprintf(buf, sizeof(buf), "%s %s", ptr, name);
		else
			strncpy(buf, name, sizeof(buf));
		nvram_set(tmp, buf);
		WL_IOVAR_SETINT(name, "chanim_mode", CHANIM_EXT);
		goto legacy_end;

legacy_mode:
#endif /* EXT_ACS */
		if (obss_coex || channel == 0) {
			if (WLCONF_PHYTYPE_11N(phytype)) {
				chanspec_t chanspec;
				int pref_chspec;

				if (channel != 0) {
					/* assumes that initial chanspec has been set earlier */
					/* Maybe we expand scope of chanspec from above so
					 * that we don't have to do the iovar_get here?
					 */

					/* We're not doing auto-channel, give the driver
					 * the preferred chanspec.
					 */
					WL_IOVAR_GETINT(name, "chanspec", &pref_chspec);
					WL_IOVAR_SETINT(name, "pref_chanspec", pref_chspec);
				} else {
					WL_IOVAR_SETINT(name, "pref_chanspec", 0);
				}

				chanspec = wlconf_auto_chanspec(name);
				if (chanspec != 0)
					WL_IOVAR_SETINT(name, "chanspec", chanspec);
			} else {
				/* select a channel */
				val = wlconf_auto_channel(name);
				/* switch to the selected channel */
				if (val != 0)
					WL_IOCTL(name, WLC_SET_CHANNEL, &val, sizeof(val));
			}
			/* set the auto channel scan timer in the driver when in auto mode */
			if (channel == 0) {
				val = 15;	/* 15 minutes for now */
			} else {
				val = 0;
			}
		} else {
			/* reset the channel scan timer in the driver when not in auto mode */
			val = 0;
		}

		WL_IOCTL(name, WLC_SET_CS_SCAN_TIMER, &val, sizeof(val));
		WL_IOVAR_SETINT(name, "chanim_mode", CHANIM_ACT);
#ifdef EXT_ACS
legacy_end:
		;
#endif /* EXT_ACS */
		/* Apply sta_config configuration settings for this interface */
		foreach(var, nvram_safe_get("sta_config"), next) {
			wlconf_process_sta_config_entry(name, var);
		}

	} /* AP or APSTA */

	/* Security settings for each BSS Configuration */
	for (i = 0; i < bclist->count; i++) {
		bsscfg = &bclist->bsscfgs[i];
		wlconf_security_options(name, bsscfg->prefix, bsscfg->idx,
		                        mac_spoof, wet || sta || apsta || psta || psr);
	}

	/*
	 * Finally enable BSS Configs or Join BSS
	 *
	 * AP: Enable BSS Config to bring AP up only when nas will not run
	 * STA: Join the BSS regardless.
	 */
	for (i = 0; i < bclist->count; i++) {
		struct {int bsscfg_idx; int enable;} setbuf;
		char vifname[VIFNAME_LEN];
		char *name_ptr = name;

		setbuf.bsscfg_idx = bclist->bsscfgs[i].idx;
		setbuf.enable = 0;

		bsscfg = &bclist->bsscfgs[i];
		if (nvram_match(strcat_r(bsscfg->prefix, "bss_enabled", tmp), "1")) {
			setbuf.enable = 1;
		}

		/* Set the MAC list */
		maclist = (struct maclist *)buf;
		maclist->count = 0;
		if (!nvram_match(strcat_r(bsscfg->prefix, "macmode", tmp), "disabled")) {
			ea = maclist->ea;
			foreach(var, nvram_safe_get(strcat_r(bsscfg->prefix, "maclist", tmp)),
				next) {
				if (((char *)((&ea[1])->octet)) > ((char *)(&buf[sizeof(buf)])))
					break;
				if (ether_atoe(var, ea->octet)) {
					maclist->count++;
					ea++;
				}
			}
		}

		if (setbuf.bsscfg_idx == 0) {
			name_ptr = name;
		} else { /* Non-primary BSS; changes name syntax */
			char tmp[VIFNAME_LEN];
			int len;

			/* Remove trailing _ if present */
			memset(tmp, 0, sizeof(tmp));
			strncpy(tmp, bsscfg->prefix, VIFNAME_LEN - 1);
			if (((len = strlen(tmp)) > 0) && (tmp[len - 1] == '_')) {
				tmp[len - 1] = 0;
			}
			nvifname_to_osifname(tmp, vifname, VIFNAME_LEN);
			name_ptr = vifname;
		}

		WL_IOCTL(name_ptr, WLC_SET_MACLIST, buf, sizeof(buf));

		/* Set macmode for each VIF */
		(void) strcat_r(bsscfg->prefix, "macmode", tmp);

		if (nvram_match(tmp, "deny"))
			val = WLC_MACMODE_DENY;
		else if (nvram_match(tmp, "allow"))
			val = WLC_MACMODE_ALLOW;
		else
			val = WLC_MACMODE_DISABLED;

		WL_IOCTL(name_ptr, WLC_SET_MACMODE, &val, sizeof(val));
	}

	ret = 0;
exit:
	if (bclist != NULL)
		free(bclist);

	return ret;
}

int
wlconf_down(char *name)
{
	int val, ret = 0;
	int i;
	int wlsubunit;
	int bcmerr;
	struct {int bsscfg_idx; int enable;} setbuf;
	int wl_ap_build = 0; /* 1 = wl compiled with AP capabilities */
	char cap[WLC_IOCTL_SMLEN];
	char caps[WLC_IOCTL_MEDLEN];
	char *next;
	wlc_ssid_t ssid;

	/* wlconf doesn't work for virtual i/f */
	if (get_ifname_unit(name, NULL, &wlsubunit) == 0 && wlsubunit >= 0) {
		WLCONF_DBG("wlconf: skipping virtual interface \"%s\"\n", name);
		return 0;
	}

	/* Check interface (fail silently for non-wl interfaces) */
	if ((ret = wl_probe(name)))
		return ret;

	/* because of ifdefs in wl driver,  when we don't have AP capabilities we
	 * can't use the same iovars to configure the wl.
	 * so we use "wl_ap_build" to help us know how to configure the driver
	 */
	if (wl_iovar_get(name, "cap", (void *)caps, sizeof(caps)))
		return -1;

	foreach(cap, caps, next) {
		if (!strcmp(cap, "ap")) {
			wl_ap_build = 1;
		}
	}

	if (wl_ap_build) {
		/* Bring down the interface */
		WL_IOCTL(name, WLC_DOWN, NULL, sizeof(val));

		/* Disable all BSS Configs */
		for (i = 0; i < WL_MAXBSSCFG; i++) {
			setbuf.bsscfg_idx = i;
			setbuf.enable = 0;

			ret = wl_iovar_set(name, "bss", &setbuf, sizeof(setbuf));
			if (ret) {
				wl_iovar_getint(name, "bcmerror", &bcmerr);
				/* fail quietly on a range error since the driver may
				 * support fewer bsscfgs than we are prepared to configure
				 */
				if (bcmerr == BCME_RANGE)
					break;
			}
		}
	} else {
		WL_IOCTL(name, WLC_GET_UP, &val, sizeof(val));
		if (val) {
			/* Nuke SSID */
			ssid.SSID_len = 0;
			ssid.SSID[0] = '\0';
			WL_IOCTL(name, WLC_SET_SSID, &ssid, sizeof(ssid));

			/* Bring down the interface */
			WL_IOCTL(name, WLC_DOWN, NULL, sizeof(val));
		}
	}

	/* Nuke the WDS list */
	wlconf_wds_clear(name);

	return 0;
}

int
wlconf_start(char *name)
{
	int i, ii, unit, val, ret = 0;
	int wlunit = -1;
	int wlsubunit = -1;
	int ap, apsta, wds, sta = 0, wet = 0;
	int wl_ap_build = 0; /* wl compiled with AP capabilities */
	char buf[WLC_IOCTL_SMLEN];
	struct maclist *maclist;
	struct ether_addr *ea;
	struct bsscfg_list *bclist = NULL;
	struct bsscfg_info *bsscfg = NULL;
	wlc_ssid_t ssid;
	char cap[WLC_IOCTL_SMLEN], caps[WLC_IOCTL_MEDLEN];
	char var[80], tmp[100], prefix[PREFIX_LEN], *str, *next;
	int trf_mgmt_cap = 0, trf_mgmt_dwm_cap = 0;
	bool dwm_supported = FALSE;

	/* Check interface (fail silently for non-wl interfaces) */
	if ((ret = wl_probe(name)))
		return ret;

	/* wlconf doesn't work for virtual i/f, so if we are given a
	 * virtual i/f return 0 if that interface is in it's parent's "vifs"
	 * list otherwise return -1
	 */
	memset(tmp, 0, sizeof(tmp));
	if (get_ifname_unit(name, &wlunit, &wlsubunit) == 0) {
		if (wlsubunit >= 0) {
			/* we have been given a virtual i/f,
			 * is it in it's parent i/f's virtual i/f list?
			 */
			sprintf(tmp, "wl%d_vifs", wlunit);

			if (strstr(nvram_safe_get(tmp), name) == NULL)
				return -1; /* config error */
			else
				return 0; /* okay */
		}
	}
	else {
		return -1;
	}

	/* because of ifdefs in wl driver,  when we don't have AP capabilities we
	 * can't use the same iovars to configure the wl.
	 * so we use "wl_ap_build" to help us know how to configure the driver
	 */
	if (wl_iovar_get(name, "cap", (void *)caps, sizeof(caps)))
		return -1;

	foreach(cap, caps, next) {
		if (!strcmp(cap, "ap"))
			wl_ap_build = 1;

		if (!strcmp(cap, "traffic-mgmt"))
			trf_mgmt_cap = 1;

		if (!strcmp(cap, "traffic-mgmt-dwm"))
			trf_mgmt_dwm_cap = 1;
	}

	/* Get instance */
	WL_IOCTL(name, WLC_GET_INSTANCE, &unit, sizeof(unit));
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);


	/* Get the list of BSS Configs */
	if (!(bclist = wlconf_get_bsscfgs(name, prefix)))
		return -1;

	/* wlX_mode settings: AP, STA, WET, BSS/IBSS, APSTA */
	str = nvram_safe_get(strcat_r(prefix, "mode", tmp));
	ap = (!strcmp(str, "") || !strcmp(str, "ap"));
	apsta = (!strcmp(str, "apsta") ||
	         ((!strcmp(str, "sta") || !strcmp(str, "psr") || !strcmp(str, "wet")) &&
	          bclist->count > 1));
	sta = (!strcmp(str, "sta") && bclist->count == 1);
	wds = !strcmp(str, "wds");
	wet = !strcmp(str, "wet");
	if (!strcmp(str, "mac_spoof") || !strcmp(str, "psta") || !strcmp(str, "psr"))
		sta = 1;

	/* Retain remaining WET effects only if not APSTA */
	wet &= !apsta;

	/* AP only config, code copied as-is from wlconf function */
	if (ap || apsta || wds) {
		/* Set lazy WDS mode */
		val = atoi(nvram_safe_get(strcat_r(prefix, "lazywds", tmp)));
		WL_IOCTL(name, WLC_SET_LAZYWDS, &val, sizeof(val));

		/* Set the WDS list */
		maclist = (struct maclist *) buf;
		maclist->count = 0;
		ea = maclist->ea;
		foreach(var, nvram_safe_get(strcat_r(prefix, "wds", tmp)), next) {
			if (((char *)(ea->octet)) > ((char *)(&buf[sizeof(buf)])))
				break;
			ether_atoe(var, ea->octet);
			maclist->count++;
			ea++;
		}
		WL_IOCTL(name, WLC_SET_WDSLIST, buf, sizeof(buf));

		/* Set WDS link detection timeout */
		val = atoi(nvram_safe_get(strcat_r(prefix, "wds_timeout", tmp)));
		wl_iovar_setint(name, "wdstimeout", val);
	}

	/*
	 * Finally enable BSS Configs or Join BSS
	 * code copied as-is from wlconf function
	 */
	for (i = 0; i < bclist->count; i++) {
		struct {int bsscfg_idx; int enable;} setbuf;

		setbuf.bsscfg_idx = bclist->bsscfgs[i].idx;
		setbuf.enable = 0;

		bsscfg = &bclist->bsscfgs[i];
		if (nvram_match(strcat_r(bsscfg->prefix, "bss_enabled", tmp), "1")) {
			setbuf.enable = 1;
		}

		/*  bring up BSS  */
		if (ap || apsta || sta || wet) {
			for (ii = 0; ii < MAX_BSS_UP_RETRIES; ii++) {
				if (wl_ap_build) {
					WL_IOVAR_SET(name, "bss", &setbuf, sizeof(setbuf));
				}
				else {
					strcat_r(prefix, "ssid", tmp);
					ssid.SSID_len = strlen(nvram_safe_get(tmp));
					if (ssid.SSID_len > sizeof(ssid.SSID))
						ssid.SSID_len = sizeof(ssid.SSID);
					strncpy((char *)ssid.SSID, nvram_safe_get(tmp),
						ssid.SSID_len);
					WL_IOCTL(name, WLC_SET_SSID, &ssid, sizeof(ssid));
				}
				if (apsta && (ret != 0))
					sleep_ms(1000);
				else
					break;
			}
		}

	}
	if ((ap || apsta || sta) && (trf_mgmt_cap)) {
		if (trf_mgmt_dwm_cap && ap)
			dwm_supported = TRUE;
		trf_mgmt_settings(prefix, dwm_supported);
	}

#ifdef TRAFFIC_MGMT_RSSI_POLICY
	if ((ap || apsta) && (trf_mgmt_cap)) {
		trf_mgmt_rssi_policy(prefix);
	}
#endif /* TRAFFIC_MGMT_RSSI_POLICY */

#ifdef __CONFIG_EMF__
	if (nvram_match(strcat_r(bsscfg->prefix, "wmf_bss_enable", tmp), "1")) {
		val = atoi(nvram_safe_get(strcat_r(prefix, "wmf_ucigmp_query", tmp)));
		wl_iovar_setint(name, "wmf_ucast_igmp_query", val);
		val = atoi(nvram_safe_get(strcat_r(prefix, "wmf_mdata_sendup", tmp)));
		wl_iovar_setint(name, "wmf_mcast_data_sendup", val);
		val = atoi(nvram_safe_get(strcat_r(prefix, "wmf_ucast_upnp", tmp)));
		wl_iovar_setint(name, "wmf_ucast_upnp", val);
		val = atoi(nvram_safe_get(strcat_r(prefix, "wmf_igmpq_filter", tmp)));
		wl_iovar_setint(name, "wmf_igmpq_filter", val);
	}
#endif /* __CONFIG_EMF__ */

	if (bclist != NULL)
		free(bclist);

	return ret;
}

#if defined(linux)
static int
wlconf_security(char *name)
{
	int unit, bsscfg_idx;
	char prefix[PREFIX_LEN];
	char tmp[100], *str;

	/* Get the interface subunit */
	if (get_ifname_unit(name, &unit, &bsscfg_idx) != 0) {
		WLCONF_DBG("wlconfig(%s): unable to parse unit.subunit in interface "
		           "name \"%s\"\n", name, name);
		return -1;
	}

	/* Configure security parameters for the newly created interface */
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	str = nvram_safe_get(strcat_r(prefix, "mode", tmp));
	wlconf_security_options(name, prefix, bsscfg_idx, FALSE,
	                        !strcmp(str, "psta") || !strcmp(str, "psr"));

	return 0;
}

int
main(int argc, char *argv[])
{
	/* Check parameters and branch based on action */
	if (argc == 3 && !strcmp(argv[2], "up"))
		return wlconf(argv[1]);
	else if (argc == 3 && !strcmp(argv[2], "down"))
		return wlconf_down(argv[1]);
	else if (argc == 3 && !strcmp(argv[2], "start"))
	  return wlconf_start(argv[1]);
	else if (argc == 3 && !strcmp(argv[2], "security"))
	  return wlconf_security(argv[1]);
	else {
		fprintf(stderr, "Usage: wlconf <ifname> up|down\n");
		return -1;
	}
}
#endif /*  defined(linux) */
