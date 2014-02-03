/* nvram table for asuswrt, version 0.1 */

#include <epivers.h>
#include <typedefs.h>
#include <string.h>
#include <ctype.h>
#include <bcmnvram.h>
#include <wlioctl.h>
#include <stdio.h>
#include <shared.h>
#include <version.h>
#include <shutils.h>

#define XSTR(s) STR(s)
#define STR(s) #s

// stub for wlconf, etc.
struct nvram_tuple router_defaults[] = {
	// NVRAM for init_syspara: system paraamters getting from MFG/NVRAM Area
 	//{ "boardflags", "" }, 
	//{ "productid", "" },
	//{ "firmver", "" },
	//{ "hardware_version", "" }, // bootloader and hardware version
	//{ "et0macaddr", "" }, 
	//{ "wl0macaddr", "" }, 
	//{ "wl1macaddr", "" },

	// NVRAM for restore_defaults: system wide parameters
	{ "nvramver", RTCONFIG_NVRAM_VER},
	{ "restore_defaults",	"0"	},	// Set to 0 to not restore defaults on boot
	{ "sw_mode", "1" 		}, 	// big switch for different mode 
	{ "preferred_lang", "EN"	},
	// NVRAM from init_nvram: system wide parameters accodring to model and mode 
	//{ "wan_ifnames", "" },
	//{ "lan_ifnames", "" },
	//{ "lan1_ifnames", "" },
	//{ "vlan_enable", "" },
	//{ "vlan0ports", "" },
	//{ "vlan1ports", "" },
	//{ "vlan2ports", "" },
	/* Guest H/W parameters */

	// NVRAM for switch	
	{ "switch_stb_x", "0"}, 		// oleg patch
	{ "switch_wantag", "none"},		//for IPTV/VoIP case
	{ "switch_wan0tagid", "" },		//Wan Port
	{ "switch_wan0prio", "0" },
	{ "switch_wan1tagid", "" },		//IPTV Port
	{ "switch_wan1prio", "0" },
	{ "switch_wan2tagid", "" },		//VoIP Port
	{ "switch_wan2prio", "0" },
	{ "wl_unit",		"0"	},
	{ "wl_subunit", 	"-1"	},
	{ "wl_vifnames", 	""	},	/* Virtual Interface Names */
	/* Wireless parameters */
#ifndef RTCONFIG_RALINK
	{ "wl_version", EPI_VERSION_STR },	/* OS revision */
#endif
#ifdef RTCONFIG_DSL
	{ "wl_HW_switch", "0" },		/* siwtch WiFi slash*/
#endif
	{ "wl_ifname", "", 0 },			/* Interface name */
	{ "wl_hwaddr", "", 0 },			/* MAC address */
	{ "wl_phytype", "b", 0 },		/* Current wireless band ("a" (5 GHz),
 						 * "b" (2.4 GHz), or "g" (2.4 GHz))
						 */
	{ "wl_corerev", "", 0 },		/* Current core revision */
	{ "wl_phytypes", "", 0 },		/* List of supported wireless bands (e.g. "ga") */
	{ "wl_radioids", "", 0 },		/* List of radio IDs */
	{ "wl_ssid", "ASUS", 0 },		/* Service set ID (network name) */
	{ "wl1_ssid", "ASUS_5G" },
	{ "wl_bss_enabled", "1", 0 },		/* Service set Enable (1) or disable (0) radio */
						/* See "default_get" below. */
#ifdef __CONFIG_HSPOT__
	{ "wl_bss_hs2_enabled", "1", 0 },	/* Service set Hotspot Enable (1), disable (0) radio */
						/* See "default_get" below. */
#endif  /* __CONFIG_HSPOT__ */
//	{ "wl_country_code", "", 0 },		/* Country Code (default obtained from driver) */
#ifndef RTCONFIG_RALINK
	{ "wl_country_rev", "", 0 },		/* Regrev Code (default obtained from driver) */
#endif
	{ "wl_radio", "1", 0 },			/* Enable (1) or disable (0) radio */
	{ "wl_closed", "0", 0 },		/* Closed (hidden) network */
	{ "wl_ap_isolate", "0", 0 },		/* AP isolate mode */
	{ "wl_igs", "0" },			/* BCM: wl_wmf_bss_enable
						 * Ralink: IGMPSnEnable */

#ifndef RTCONFIG_RALINK
	{ "wl_wmf_bss_enable", "0", 0 },	/* WMF Enable/Disable */
	{ "wl_mcast_regen_bss_enable", "1", 0 },/* MCAST REGEN Enable/Disable */

	/* operational capabilities required for stations to associate to the BSS */
#ifdef RTCONFIG_BCMWL6
	{ "wl_bss_opmode_cap_reqd", "0", 0 },
#endif
#ifndef RTCONFIG_BCMARM
	{ "wl_rxchain_pwrsave_enable", "1", 0 },/* Rxchain powersave enable */
	{ "wl_rxchain_pwrsave_quiet_time", "1800", 0 },	/* Quiet time for power save */
	{ "wl_rxchain_pwrsave_pps", "10", 0 },	/* Packets per second threshold for power save */
	{ "wl_rxchain_pwrsave_stas_assoc_check", "0", 0 }, /* STAs associated before powersave */
#endif
	{ "wl_radio_pwrsave_enable", "0", 0 },	/* Radio powersave enable */
	{ "wl_radio_pwrsave_quiet_time", "1800", 0 },	/* Quiet time for power save */
	{ "wl_radio_pwrsave_pps", "10", 0 },	/* Packets per second threshold for power save */
	{ "wl_radio_pwrsave_level", "0", 0 },	/* Radio power save level */
	{ "wl_radio_pwrsave_stas_assoc_check", "0", 0 }, /* STAs associated before powersave */
#endif
	{ "wl_mode", "ap", 0 },			/* AP mode (ap|sta|wds) */
#ifndef RTCONFIG_RALINK
	{ "wl_lazywds", "0", 0 },		/* Enable "lazy" WDS mode (0|1) */
	{ "wl_wds", "", 0 },			/* xx:xx:xx:xx:xx:xx ... */
	{ "wl_wds_timeout", "1", 0 },		/* WDS link detection interval defualt 1 sec */
	{ "wl_wep", "disabled", 0 },		/* WEP data encryption (enabled|disabled) */
	{ "wl_auth", "0", 0 },			/* Shared key authentication optional (0) or
						 * required (1)
						 */
#endif
	{ "wl_key", "1", 0 },			/* Current WEP key */
	{ "wl_key1", "", 0 },			/* 5/13 char ASCII or 10/26 char hex */
	{ "wl_key2", "", 0 },			/* 5/13 char ASCII or 10/26 char hex */
	{ "wl_key3", "", 0 },			/* 5/13 char ASCII or 10/26 char hex */
	{ "wl_key4", "", 0 },			/* 5/13 char ASCII or 10/26 char hex */
#ifndef RTCONFIG_RALINK
	{ "wl_maclist", "", 0 },		/* xx:xx:xx:xx:xx:xx ... */
#endif
	{ "wl_macmode", "disabled", 0 },	/* "allow" only, "deny" only, or "disabled"
						 * (allow all)
						 */
	{ "wl_assoc_retry_max", "3", 0 },	/* Non-zero limit for association retries */
#ifndef RTCONFIG_BCMWL6
	{ "wl_channel", "0"},			/* Channel number */
#else
	{ "wl_chanspec", "0", 0 },		/* Channel specification */
#endif
#ifndef RTCONFIG_RALINK
	{ "wl_noisemitigation", "0"},
	{ "wl_reg_mode", "off", 0 },		/* Regulatory: 802.11H(h)/802.11D(d)/off(off) */
	{ "wl_rate", "0", 0 },			/* Rate (bps, 0 for auto) */
#if 0
	{ "wl_mrate", "0", 0 },			/* Mcast Rate (bps, 0 for auto) */
#endif
#endif
	{ "wl_mrate_x", "0" },			/* ralink auto rate */
	{ "wl_frameburst", "on", 0 },		/* BRCM Frambursting mode (off|on) */
	{ "wl_rateset", "default", 0 },		/* "default" or "all" or "12" */
	{ "wl_frag", "2346", 0 },		/* Fragmentation threshold */
	{ "wl_rts", "2347", 0 },		/* RTS threshold */
#ifndef RTCONFIG_RALINK
	{ "wl_dtim", "3", 0 },			/* DTIM period */
#else
	{ "wl_dtim", "1"},
#endif
	{ "wl_bcn", "100", 0 },			/* Beacon interval */
#ifndef RTCONFIG_RALINK
	{ "wl_bcn_rotate", "1", 0 },		/* Beacon rotation */
#endif
	{ "wl_plcphdr", "long", 0 },		/* 802.11b PLCP preamble type */
#ifndef RTCONFIG_RALINK
 	{ "wl_gmode", XSTR(GMODE_AUTO), 0 },	/* 54g mode */
#else
	{ "wl_nmode_protection", "auto", 0},	/* 802.11n protection */
#endif
	{ "wl_gmode_protection", "auto", 0 },	/* 802.11g RTS/CTS protection (off|auto) */
#ifndef RTCONFIG_RALINK
	{ "wl_optimizexbox", "0"},		/* Optimize WiFi packet for Xbox */
#endif
	{ "wl_frameburst", "on"},		/* BRCM Frambursting mode (off|on) */
	{ "wl_wme", "on", 0 },			/* WME mode (off|on|auto) */
#ifndef RTCONFIG_RALINK
	{ "wl_wme_bss_disable", "0", 0 },	/* WME BSS disable advertising (off|on) */
	{ "wl_antdiv", "-1", 0 },		/* Antenna Diversity (-1|0|1|3) */
	{ "wl_infra", "1", 0 },			/* Network Type (BSS/IBSS) */
//	{ "wl_interfmode", "2"},		/* Interference mitigation mode (2/3/4) */
#ifndef RTCONFIG_BCMWL6
	{ "wl_nbw_cap", "0"},			/* BW Cap; def 20inB and 40inA */
#else
	{ "wl_bw_cap", "1", 0},			/* BW Cap; 20 MHz */
#endif
#endif
#ifndef RTCONFIG_BCMWL6
	{ "wl_nctrlsb", "lower"},		/* N-CTRL SB */
#endif
#ifdef RTCONFIG_WL_AUTO_CHANNEL
	{"AUTO_CHANNEL", "1"},			/*0: real channel; 1: Auto; for RT-N12HP & RT-N14UHP*/
#endif
#ifndef RTCONFIG_RALINK
	{ "wl_nband", "2", 0},			/* N-BAND */
	{ "wl0_nband", "2"},			/* 2.4 GHz */
	{ "wl1_nband", "1"},			/* 5 GHz */
	{ "wl_nmcsidx", "-1", 0},		/* MCS Index for N - rate */
	{ "wl_nmode", "-1", 0},			/* N-mode */
	{ "wl_rifs_advert", "auto", 0},		/* RIFS mode advertisement */
	{ "wl_vlan_prio_mode", "off", 0},	/* VLAN Priority support */
	{ "wl_leddc", "0x640000", 0},		/* 100% duty cycle for LED on router */
	{ "wl_rxstreams", "0", 0},		/* 802.11n Rx Streams, 0 is invalid, WLCONF will
						 * change it to a radio appropriate default
						 */
	{ "wl_txstreams", "0", 0},		/* 802.11n Tx Streams 0, 0 is invalid, WLCONF will
						 * change it to a radio appropriate default
						 */
	{ "wl_stbc_tx", "auto", 0 },		/* Default STBC TX setting */
	{ "wl_stbc_rx", "1", 0 },		/* Default STBC RX setting */
	{ "wl_ampdu", "auto", 0 },		/* Default AMPDU setting */
#ifndef RTCONFIG_BCMWL6
	{ "wl_nreqd", "0"},			/* Require 802.11n support */
#endif
#endif
#ifndef RTCONFIG_RALINK
//	{ "wl_mimo_preamble", "gfbcm"},		/* Mimo PrEamble: mm/gf/auto/gfbcm */
#else
	{ "wl_mimo_preamble", "mm"},
#endif
#ifndef RTCONFIG_RALINK
	/* Default AMPDU retry limit per-tid setting */
	{ "wl_ampdu_rtylimit_tid", "7 7 7 7 7 7 7 7", 0 },
	/* Default AMPDU regular rate retry limit per-tid setting */
	{ "wl_ampdu_rr_rtylimit_tid", "3 3 3 3 3 3 3 3", 0 },
	{ "wl_amsdu", "auto", 0 },		/* Default AMSDU setting */
	{ "wl_rx_amsdu_in_ampdu", "auto", 0 },	/* Default RX AMSDU In AMPDU setting */
	{ "wl_obss_coex", "1", 0 },		/* Default OBSS Coexistence setting - OFF */
#ifdef RTCONFIG_BCMWL6
	{ "wl_ack_ratio", "0"},
	{ "wl_ampdu_mpdu", "0"},
	{ "wl_ampdu_rts", "1"},
#ifdef RTCONFIG_BCMARM
	{ "wl_turbo_qam", "1"},
#endif
#endif
#endif
	/* WPA parameters */
	{ "wl_auth_mode", "none", 0 },		/* Network authentication mode (radius|none) */
	{ "wl_wpa_psk", "", 0 },		/* WPA pre-shared key */
	{ "wl_wpa_gtk_rekey", "3600", 0 },		/* GTK rotation interval */
	{ "wl_radius_ipaddr", "", 0 },		/* RADIUS server IP address */
	{ "wl_radius_key", "", 0 },		/* RADIUS shared secret */
	{ "wl_radius_port", "1812", 0 },	/* RADIUS server UDP port */
	{ "wl_crypto", "tkip+aes", 0 },		/* WPA data encryption */
#if 0
	{ "wl_net_reauth", "3600", 0 },		/* Network Re-auth/PMK caching duration */
#else
	{ "wl_pmk_cache", "60"},		/* Network Re-auth/PMK caching duration in minutes */
#endif
	{ "wl_akm", "", 0 },			/* WPA akm list */
#ifdef RTCONFIG_BCMWL6
#ifdef MFP
	{ "wl_mfp", "0", 0 },			/* Protected Management Frame */
#endif
	{ "wl_psr_mrpt", "0", 0 },		/* Default to one level repeating mode */
#endif
#ifdef RTCONFIG_WPS
	/* WSC parameters */
#ifndef RTCONFIG_RALINK
	{ "wps_version2", "enabled", 0 },	/* Must specified before other wps variables */
	{ "wl_wps_mode", "enabled", 0 },	/* enabled wps */
	{ "wl_wps_config_state", "0", 0 },	/* config state unconfiged */
#endif
//	{ "wps_device_pin", "12345670", 0 },	// it is mapped to secret_code
#if 0
	{ "wps_modelname", RT_BUILD_NAME},
#else
	{ "wps_modelname", "Wi-Fi Protected Setup Router"},
#endif
	{ "wps_mfstring", "ASUSTeK Computer Inc."},
//	{ "wps_device_name", RT_BUILD_NAME},
#ifndef RTCONFIG_RALINK
	{ "wl_wps_reg", "enabled", 0 },
#endif
	{ "wps_sta_pin", "00000000", 0 },
//	{ "wps_modelnum", RT_BUILD_NAME},
	/* Allow or Deny Wireless External Registrar get or configure AP security settings */
	{ "wps_wer_mode", "allow", 0 },

	{ "lan_wps_oob", "enabled", 0 },	/* OOB state */
	{ "lan_wps_reg", "enabled", 0 },	/* For DTM 1.4 test */

	{ "lan1_wps_oob", "enabled", 0 },
	{ "lan1_wps_reg", "enabled", 0 },
#endif /* __CONFIG_WPS__ */
#ifdef __CONFIG_WFI__
	{ "wl_wfi_enable", "0", 0 },	/* 0: disable, 1: enable WifiInvite */
	{ "wl_wfi_pinmode", "0", 0 },	/* 0: auto pin, 1: manual pin */
#endif /* __CONFIG_WFI__ */
#ifdef __CONFIG_WAPI_IAS__
	/* WAPI parameters */
	{ "wl_wai_cert_name", "", 0 },		/* AP certificate name */
	{ "wl_wai_cert_index", "1", 0 },	/* AP certificate index. 1:X.509, 2:GBW */
	{ "wl_wai_cert_status", "0", 0 },	/* AP certificate status */
	{ "wl_wai_as_ip", "", 0 },		/* ASU server IP address */
	{ "wl_wai_as_port", "3810", 0 },	/* ASU server UDP port */
#endif /* __CONFIG_WAPI_IAS__ */
#ifndef RTCONFIG_RALINK
	/* WME parameters (cwmin cwmax aifsn txop_b txop_ag adm_control oldest_first) */
	/* EDCA parameters for STA */
	{ "wl_wme_sta_be", "15 1023 3 0 0 off off", 0 },	/* WME STA AC_BE parameters */
	{ "wl_wme_sta_bk", "15 1023 7 0 0 off off", 0 },	/* WME STA AC_BK parameters */
	{ "wl_wme_sta_vi", "7 15 2 6016 3008 off off", 0 },	/* WME STA AC_VI parameters */
	{ "wl_wme_sta_vo", "3 7 2 3264 1504 off off", 0 },	/* WME STA AC_VO parameters */

	/* EDCA parameters for AP */
	{ "wl_wme_ap_be", "15 63 3 0 0 off off", 0 },		/* WME AP AC_BE parameters */
	{ "wl_wme_ap_bk", "15 1023 7 0 0 off off", 0 },		/* WME AP AC_BK parameters */
	{ "wl_wme_ap_vi", "7 15 1 6016 3008 off off", 0 },	/* WME AP AC_VI parameters */
	{ "wl_wme_ap_vo", "3 7 1 3264 1504 off off", 0 },	/* WME AP AC_VO parameters */
#endif
	{ "wl_wme_no_ack", "off", 0},		/* WME No-Acknowledgment mode */
	{ "wl_wme_apsd", "on", 0},		/* WME APSD mode */
#ifndef RTCONFIG_RALINK
	{ "wl_maxassoc", "128", 0},		/* Max associations driver could support */
	{ "wl_bss_maxassoc", "128", 0},		/* Max associations driver could support */

	{ "wl_sta_retry_time", "5", 0 },	/* Seconds between association attempts */
#endif
#ifdef BCMDBG
	{ "wl_nas_dbg", "0", 0 },		/* Enable/Disable NAS Debugging messages */
#endif

	// ASUS used only?	
	{ "wl_nmode_x", 		"0"	},	/* 0/1/2, auto/nonly,bgmixed */
#ifdef RTCONFIG_BCMWL6
	{ "wl_bw",			"0"	},	/* 0/1/2/3 auto/20/40/80MHz */
#else
	{ "wl_bw",			"1"	},	/* 0/1/2 20, 20/40, 40MHz */
#endif
	{ "wl_auth_mode_x", 		"open"	},
// open/shared/psk/wpa/radius
	{ "wl_wep_x", 			"0"	},
// WEP data encryption 0, 1, 2 : disabled/5/13
	{ "wl_timesched", 		"1"	},
	{ "wl_radio_date_x",	"1111111"	},
	{ "wl_radio_time_x",	"00002359"	},
	{ "wl_radio_time2_x",	"00002359"	},
	{ "wl_phrase_x",		""	},	// Passphrase	// Add
	{ "wl_lanaccess", 		"off"	},
	{ "wl_expire", 			"0"	},
	{ "wl_mbss", 		""	},
#ifdef RTCONFIG_RALINK
	{ "wl_TxPower",			"100" 	},
#else
//	{ "wl_TxPower", 		"0"	},
	{ "wl_TxPower",			"80"	},
#endif

#if defined (RTCONFIG_RALINK) || defined (RTCONFIG_BCMWL6)
#if defined(RTAC53U)
	/* RT-AC53U disable txbf by default */
	{ "wl_txbf", "0" },
#else
	{ "wl_txbf", "1" },
#endif
#endif
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_BCMARM
	{ "wl_itxbf", "1" },
	{ "wl0_itxbf", "0" },
#endif
#endif

#if defined (RTCONFIG_WIRELESSREPEATER) || defined (RTCONFIG_PROXYSTA)
	{ "wlc_list",			""	},
	{ "wlc_band",			""	},
	{ "wlc_ssid", 			"8f3610e3c9feabed953a6"	},
	{ "wlc_wep",			""	},
	{ "wlc_key",			""	},
	{ "wlc_wep_key",		""	},
	{ "wlc_auth_mode", 		""	},
	{ "wlc_crypto", 		""	},
	{ "wlc_wpa_psk",		""	},
#ifndef RTCONFIG_BCMWL6
	{ "wlc_nbw_cap", 		""	},
#else
	{ "wlc_bw_cap",			""	},
#endif
	{ "wlc_ure_ssid",		""	},
#endif

#ifdef RTCONFIG_PROXYSTA
	{ "wlc_psta",			"0"	},	/* 0: disabled, 1: Proxy STA, 2: Proxy STA Repeater */
#endif

	// WPA parameters
#ifdef RTCONFIG_RALINK
	{ "wl_PktAggregate", "1" },	// UI configurable
 	{ "wl_HT_OpMode", "0" }, 	// UI configurable
	{ "wl_DLSCapable", "0" },	// UI configurable
	{ "wl_GreenAP",	"0" },		// UI configurable
	{ "wl_HT_AutoBA", "1" },
	{ "wl_HT_HTC", "1"},
	{ "wl_HT_RDG", "1"},
	{ "wl_HT_LinkAdapt", "0"},
	{ "wl_HT_MpduDensity", "5" },
	{ "wl_HT_AMSDU", "0"},
	{ "wl_HT_GI", "1"},
	{ "wl_HT_BAWinSize", "64" },
	{ "wl_HT_MCS", "33"},
	{ "wl_HT_BADecline", "0" },
//	{ "wl_HT_TxStream", "2" },
//	{ "wl_HT_RxStream", "3" },
//	{ "wl0_HT_TxStream", "2" }, // move to init_nvram for model dep.
//	{ "wl0_HT_RxStream", "2" }, // move to init_nvram for model dep.
//	{ "wl1_HT_TxStream", "2" }, // move to init_nvram for model dep.
//	{ "wl1_HT_RxStream", "3" }, // move to init_nvram for model dep.
	{ "wl_HT_STBC", "1" },
	{ "wl_McastPhyMode", "0" },
	{ "wl_McastMcs", "0" },
	// the following for ralink 5g only
	{ "wl_IEEE80211H", "0" },
	{ "wl_CSPeriod", "10" },
	{ "wl_RDRegion", "FCC" },
	{ "wl_CarrierDetect", "0" },
	{ "wl_ChannelGeography", "2" },
	{ "wl_txbf_en", "0" },
#endif

#ifdef RTCONFIG_EMF
	{ "emf_enable",		"0"	},	/* it is common entry for all platform now
						   broadcom: enable mrate, wmf_bss_enable
						   ralink: enable mrate, IgmpSnEnable */
#ifndef RTCONFIG_RALINK
	{ "emf_entry", "", 0 },			/* Static MFDB entry (mgrp:if) */
	{ "emf_uffp_entry", "", 0 },		/* Unreg frames forwarding ports */
	{ "emf_rtport_entry", "", 0 },		/* IGMP frames forwarding ports */
#ifdef RTCONFIG_BCMWL6
	{ "wl_wmf_ucigmp_query", "0", 0 },	/* Disable Converting IGMP Query to ucast (default) */
	{ "wl_wmf_mdata_sendup", "0", 0 },	/* Disable Sending Multicast Data to host  (default) */
	{ "wl_wmf_ucast_upnp", "0", 0 },	/* Disable Converting upnp to ucast (default) */
	{ "wl_wmf_igmpq_filter", "0", 0 },	/* Disable igmp query filter */
#endif
#endif
#endif

// WPS 
//	#if defined (W7_LOGO) || defined (WIFI_LOGO)
	{ "wps_enable", "1"},
//	#else
//	{ "wps_enable", "0"},					// win7 logo
//	#endif
#ifdef RTCONFIG_RALINK
	{ "wl_wsc_config_state", "0"},				/* config state unconfiged */
#endif
	{ "wps_band", "0"},					/* "0": 2.4G, "1": 5G */
#if defined(RTCONFIG_WPSMULTIBAND)
	{ "wps_multiband", "1"},				/* Enable WPS on 2.4G and 5G both */
#else
	{ "wps_multiband", "0"},
#endif

// Wireless WDS Mode
	{ "wl_mode_x", "0"},					// 0/1/2(ap/wds/hybrid)
	{ "wl_wdsapply_x", "0"},
	{ "wl_wdslist", ""}, 					// xxxxxxxxxxxx ...

// Wireless Mac Filter
	{ "wl_maclist_x", ""},					// xxxxxxxxxxxx ... xxxxxxxxxxx

#ifdef RTCONFIG_BCMWL6
	{ "acs_ifnames", "", 0 },

	{ "wl_wet_tunnel", "0", 0  },		/* Disable wet tunnel */

	{ "dpsta_ifnames", "", 0  },
	{ "dpsta_policy", "1", 0  },
	{ "dpsta_lan_uif", "1", 0  },
#ifdef TRAFFIC_MGMT_RSSI_POLICY
	{ "wl_trf_mgmt_rssi_policy", "0", 0 }, /* Disable RSSI (default) */
#endif /* TRAFFIC_MGMT */

	/* Tx Beamforming */
	{ "wl_txbf_bfr_cap", "1", 0 },
	{ "wl_txbf_bfe_cap", "1", 0 },
#ifndef RTCONFIG_BCMARM
	{ "wl_txbf_timer", "25", 0 },
#endif

	/* acsd setting */
	{ "wl_acs_fcs_mode", "0", 0 },		/* acsd disable FCS mode */
	{ "wl_dcs_csa_unicast", "0", 0 },	/* disable unicast csa */
	{ "wl_acs_excl_chans", "", 0 },		/* acsd exclude chanspec list */
	{ "wl_acs_dfs", "0", 0 },		/* acsd fcs disable init DFS chan */
	{ "wl_acs_dfsr_immediate", "300 3", 0 },   /* immediate if > 3 switches last 5 minutes */
	{ "wl_acs_dfsr_deferred", "604800 5", 0 }, /* deferred if > 5 switches in last 7 days */
	{ "wl_acs_dfsr_activity", "30 10240", 0 }, /* active: >10k I/O in the last 30 seconds */
	{ "wl_acs_cs_scan_timer", "900", 0 },	/* acsd fcs cs scan timeout */
	{ "wl_acs_ci_scan_timer", "4", 0 },	/* acsd fcs CI scan period */
	{ "wl_acs_ci_scan_timeout", "300", 0 },	/* acsd fcs CI scan timeout */
	{ "wl_acs_scan_entry_expire", "3600", 0 },	/* acsd fcs scan expier time */
	{ "wl_acs_tx_idle_cnt", "5", 0 },		/* acsd fcs tx idle thld */
	{ "wl_acs_chan_dwell_time", "70", 0 },	/* acsd fcs chan dwell time */
	{ "wl_acs_chan_flop_period", "70", 0 },	/* acsd fcs chan flip-flop time */
	{ "wl_intfer_period", "1", 0 },		/* acsd fcs sample period */
	{ "wl_intfer_cnt", "3", 0 },		/* acsd fcs sample cnt */
	{ "wl_intfer_txfail", "16", 0 },	/* fcs non-TCP txfail threshold setting */
	{ "wl_intfer_tcptxfail", "5", 0 },	/* fcs TCP txfail threshold setting */

	{ "wl_pspretend_retry_limit", "0", 0 }, /* Disable PsPretend */
	{ "wl_pspretend_threshold", "0", 0 },	/* Disable PsPretend Threshold */

	{ "bsd_enable", "0", 0 },		/* Disable Band Steer Daemon */
#endif

	// make sure its purpose
	// advanced-ctnf
	{ "ct_tcp_timeout",		""},
	{ "ct_udp_timeout",		"30 180"},
	{ "ct_timeout",			""},
	{ "ct_max",			""}, //per model default value is assigned in init_nvram

#ifndef RTCONFIG_RALINK
	{ "ctf_disable",		"0"		},
	{ "ctf_disable_force", 		"0"		},
#ifdef RTCONFIG_BCMFA
	{ "ctf_fa_mode",                "0"             },
#endif
#ifdef RTCONFIG_USB_MODEM
	// TODO: for the bad CTF. After updating CTF, need to mark these codes.
	{ "ctf_disable_modem", 		"0"		},
#endif
#ifndef RTCONFIG_BCMARM
#if defined(RTAC53U)
	// RT-AC53U disable gro can get better usb performance
	{ "gro_disable", 		"0"		},
#else
	{ "gro_disable", 		"1"		},
#endif
#else
	{ "gro_disable",		"0"		},
#endif
#endif
//#ifdef RTCONFIG_BCMWL6
//	{ "pktc_disable", 		"0"		},
//#endif

	// NVRAM for start_lan: 
// LAN H/W parameters
	{ "lan_hwnames",		""		},	// LAN driver names (e.g. et0)
	{ "lan_hwaddr",			""		},	// LAN interface MAC address

	// LAN TCP/IP parameters
	{ "lan_unit",			"-1"		},
	{ "lan_proto",			"static"	},	// DHCP server [static|dhcp]	//Barry add 2004 09 16
	{ "lan_ipaddr",			"192.168.1.1"	},	// LAN IP address
	{ "lan_ipaddr_rt",		"192.168.1.1"	},
	{ "lan_netmask",		"255.255.255.0"	},	// LAN netmask
	{ "lan_netmask_rt",		"255.255.255.0" },
	{ "lan_gateway",		"0.0.0.0"	},	// LAN Gateway

	{ "lan_wins",			""		},	// x.x.x.x x.x.x.x ...
	{ "lan_domain",			""		},	// LAN domain name
	{ "lan_lease",			"86400"		},	// LAN lease time in seconds
	{ "lan_stp",			"1"		},	// LAN spanning tree protocol
	{ "lan_route",			""		},	// Static routes (ipaddr:netmask:gateway:metric:ifname ...)

	{ "lan_dnsenable_x", "0"},
	{ "lan_dns1_x", ""},					/* x.x.x.x x.x.x.x ... */
	{ "lan_dns2_x", ""},
	{ "lan_dns_fwd_local", "0"},		/* Forward queries for local domain to upstream DNS server */
	{ "lan_port", "80"},
	{ "jumbo_frame_enable", "0"},

	/* Guest TCP/IP parameters */

	/* Guest H/W parameters */
	{ "lan1_hwnames", "" },			/* LAN driver names (e.g. et0) */
	{ "lan1_hwaddr", "" },			/* LAN interface MAC address */

	{ "lan1_proto", "0", 0 },		/* DHCP client [static|dhcp] */
	{ "lan1_ipaddr", "192.168.2.1", 0 },	/* LAN IP address */
	{ "lan1_netmask", "255.255.255.0", 0 },	/* LAN netmask */
	{ "lan1_gateway", "192.168.2.1", 0 },	/* LAN gateway */
	{ "lan1_wins", "", 0 },			/* x.x.x.x x.x.x.x ... */
	{ "lan1_domain", "", 0 },		/* LAN domain name */
	{ "lan1_lease", "86400", 0 },		/* LAN lease time in seconds */
	{ "lan1_stp", "1", 0 },			/* LAN spanning tree protocol */
	{ "lan1_route", "", 0 },		/* Static routes
						 * (ipaddr:netmask:gateway:metric:ifname ...)
						 */

	// NVRAM for start_dhcpd
	// DHCP server parameters
	{ "dhcp_enable_x", "1" },
	{ "dhcp_start", "192.168.1.2"},
	{ "dhcp_end", "192.168.1.254"},
	{ "dhcp_lease", "86400" },
	{ "dhcp_gateway_x", "" },
	{ "dhcp_dns1_x", "" },
	{ "dhcp_wins_x", "" },
	{ "dhcp_static_x", "0"},
	{ "dhcp_staticlist", ""},
	{ "dhcpd_lmax", "253"},
	{ "dhcpc_mode", "1"},
	{ "dhcpd_querylog", "1"},

	// NVRAM for start_dhcpd
	// Guest DHCP server parameters
	{ "dhcp1_enable_x", "0" },
	{ "dhcp1_start", "192.168.2.2"},
	{ "dhcp1_end", "192.168.2.254"},
	{ "dhcp1_lease", "86400" },
	{ "dhcp1_gateway_x", "" },
	{ "dhcp1_dns1_x", "" },
	{ "dhcp1_wins_x", "" },
	{ "dhcp1_static_x", "0"},
	{ "dhcp1_staticlist", ""},

	{ "time_zone", "GMT0" },
	{ "time_zone_dst", "0" },
	{ "time_zone_dstoff", "" },
	{ "ntp_server1", "time.nist.gov" },
	{ "ntp_server0", "pool.ntp.org" },

	// NVRAM for do_startic_routes
	{ "sr_enable_x", "0"},
	{ "sr_rulelist", ""},
	{ "dr_enable_x", "1" }, // oleg patch
	{ "mr_enable_x", "0" }, // oleg patch
	
	// Domain Name
	{ "local_domain", "router.asus.com"},
	
	// NVRAM for switch control
//#ifdef RTCONFIG_SWITCH_CONTROL_8367
#ifdef RTCONFIG_RALINK
	{ "switch_ctrlrate_unknown_unicast", "20" },
	{ "switch_ctrlrate_unknown_multicast", "20" },
	{ "switch_ctrlrate_multicast", "20" },
	{ "switch_ctrlrate_broadcast", "20" },
#endif

#ifdef RTCONFIG_RALINK
#if defined (W7_LOGO) || defined (WIFI_LOGO) || defined(RTCONFIG_DSL)
	{ "hwnat", "0"},
#else
	{ "hwnat", "1"},
#endif
#endif

	// NVRAM for start_wan
	{ "wan_unit", "0",},		/* Last configured connection */
	{ "wan_enable", "1"},
	// NVRAM for start_wan_if
	/* WAN H/W parameters */
	{ "wan_hwname", ""},		/* WAN driver name (e.g. et1) */
	{ "wan_hwaddr", ""},		/* WAN interface MAC address */
	{ "wan_phytype", ""},

	/* WAN TCP/IP parameters */
	{ "wan_proto", "dhcp"},		/* [static|dhcp|pppoe|pptp/l2tp|disabled] */
	{ "wan_nat_x", "1"},
	{ "wan_dhcpenable_x", "1"}, 	// replace x_DHCPClient
	{ "wan_ipaddr_x", "0.0.0.0"},	/* WAN IP address */
	{ "wan_ipaddr", "0.0.0.0"},
	{ "wan_netmask_x", "0.0.0.0"},	/* WAN netmask */
	{ "wan_gateway_x", "0.0.0.0"},	/* WAN gateway */
	{ "wan_gateway", "0.0.0.0"},
	{ "wan_dnsenable_x", "1"},
	{ "wan_dns1_x", ""},		/* x.x.x.x x.x.x.x ... */
	{ "wan_dns2_x", ""},
	{ "wan_dns", ""},
	// { "wan_wins", ""},		/* x.x.x.x x.x.x.x ... */
	{ "wan_heartbeat_x", ""}, 	/* VPN Server */
	{ "wan_hostname", ""},		/* WAN hostname */
	{ "wan_hwaddr_x", ""},		/* Cloned mac */
#ifdef TODO
	{ "wan_domain", ""},		/* WAN domain name */
	{ "wan_lease", "86400"},	/* WAN lease time in seconds */
#endif

	/* Auth parameters */
	{ "wan_auth_x", "" },		/* WAN authentication type */

	/* PPPoE parameters */
	{ "wan_pppoe_username", ""},	/* PPP username */
	{ "wan_pppoe_passwd", ""},	/* PPP password */
	{ "wan_pppoe_idletime", "0"},	// oleg patch
	{ "wan_pppoe_mru", "1492"},	/* Negotiate MRU to this value */
	{ "wan_pppoe_mtu", "1492"},	/* Negotiate MTU to the smaller of this value or the peer MRU */
	{ "wan_pppoe_service", ""},	/* PPPoE service name */
	{ "wan_pppoe_ac", ""},		/* PPPoE access concentrator name */
	{ "wan_pppoe_options_x", ""},	// oleg patch
	{ "wan_pptp_options_x", "" },	// oleg patch

	/* Misc WAN parameters */

	{ "wan_desc", ""},		/* WAN connection description */
	{ "wan_upnp_enable", "1"}, 	// upnp igd
	{ "wan_pppoe_relay", "0" },
	{ "wan_dhcpc_options",""},  // Optional arguments for udhcpc
	{ "wan_mtu", "1500"},		// WAN MTU (non-PPP based)

	// VPN+DHCP, a sperated nvram to control this function
	{ "wan_vpndhcp", "1"},

	// For miniupnpd, so far default value only
	{ "upnp_enable", "1" },
	{ "upnp_secure", "1" },
	{ "upnp_port", "0" },
	{ "upnp_ssdp_interval", "60" },
	{ "upnp_mnp", "1" },
	{ "upnp_clean", "1" },
	{ "upnp_clean_interval", "600" },
	{ "upnp_clean_threshold", "20" },

#ifdef RTCONFIG_DUALWAN // RTCONFIG_DUALWAN
	{ "wans_mode", "fo" }, 		// off/failover/loadbance/routing(off/fo/lb/rt)
#ifdef RTCONFIG_DSL
	{ "wans_dualwan", "dsl none"},
#else
	{ "wans_dualwan", "wan none"},
#endif
	{ "wans_lanport", "1"},
	{ "wans_lb_ratio", "3:1" }, 	// only support two wan simultaneously
	{ "wans_routing_enable", "0" },
	{ "wans_routing_rulelist", "" },
	{ "wan0_routing_isp_enable", "0" },
	{ "wan0_routing_isp", "china_mobile" },
	{ "wan1_routing_isp_enable", "0" },
	{ "wan1_routing_isp", "china_mobile" },

	{ "wandog_enable", "0" },
	{ "wandog_target", "" },
	{ "wandog_interval", "5" },
	{ "wandog_maxfail", "12" },
	{ "wandog_delay", "0" },
#endif // RTCONFIG_DUALWAN

#ifdef RTCONFIG_DSL
	{ "dslx_modulation", "5" }, // multiple mode
	{ "dslx_snrm_offset", "0" }, /* Paul add 2012/9/24, for SNR Margin tweaking. */
	{ "dslx_sra", "1" }, /* Paul add 2012/10/15, for setting SRA. */
	{ "dslx_bitswap", "1" }, /* Paul add 2013/10/23, for Bitswap control. */
#ifdef RTCONFIG_DSL_ANNEX_B //Paul add 2012/8/21
	{ "dslx_annex", "0" }, // Annex B
#else
	{ "dslx_annex", "4" }, // Annex AIJLM
#endif

// the following variables suppose can be removed
	{ "dslx_nat", "1" },	
	{ "dslx_upnp_enable", "1" },	
	{ "dslx_link_enable", "1" },	
	{ "dslx_DHCPClient", "1" },
	{ "dslx_dhcp_clientid", "" },	//Required by some ISP using RFC 1483 MER.
	{ "dslx_ipaddr", "0.0.0.0"},	/* IP address */
	{ "dslx_netmask", "0.0.0.0"},	/* netmask */
	{ "dslx_gateway", "0.0.0.0"},	/* gateway */
	{ "dslx_dnsenable", "1"},
	{ "dslx_dns1", ""},
	{ "dslx_dns2", ""},	
// now use switch_stb_x	
	{ "dslx_pppoe_username", ""},
	{ "dslx_pppoe_passwd", ""},
	// this one is no longer to use
	//{ "dslx_pppoe_dial_on_demand", ""},
	{ "dslx_pppoe_idletime", "0"},
	{ "dslx_pppoe_mtu", "1492"},
	// this one is no longer to use	
//	{ "dslx_pppoe_mru", ""},
	{ "dslx_pppoe_service", ""},
	{ "dslx_pppoe_ac", ""},
	{ "dslx_pppoe_options", ""},	
	{ "dslx_hwaddr", ""},
//	
	{ "dsl_unit", "0"}, 
//
	{ "dsl_enable", ""},
	{ "dsl_vpi", ""},
	{ "dsl_vci", ""},
	{ "dsl_encap", ""},
	{ "dsl_proto", ""},
	
	/* Paul modify 2012/8/6, set default Service Category to UBR without PCR, with PCR, SCR and MBS set to 0. */
	{ "dsl_svc_cat", "0"},
	{ "dsl_pcr", "0"},
	{ "dsl_scr", "0"},
	{ "dsl_mbs", "0"},

// those PVC need to init first so that QIS internet/IPTV PVC setting could write to NVRAM
	{ "dsl0_enable", "0"},
	{ "dsl0_vpi", ""},
	{ "dsl0_vci", ""},
	{ "dsl0_encap", ""},
	{ "dsl0_proto", ""},
	{ "dsl0_svc_cat", ""},
	{ "dsl0_pcr", ""},
	{ "dsl0_scr", ""},
	{ "dsl0_mbs", ""},
//
	{ "dsl1_enable", "0"},
	{ "dsl1_vpi", ""},
	{ "dsl1_vci", ""},
	{ "dsl1_encap", ""},
	{ "dsl1_proto", ""},
	{ "dsl1_svc_cat", ""},
	{ "dsl1_pcr", ""},
	{ "dsl1_scr", ""},
	{ "dsl1_mbs", ""},	
//
	{ "dsl2_enable", "0"},
	{ "dsl2_vpi", ""},
	{ "dsl2_vci", ""},
	{ "dsl2_encap", ""},
	{ "dsl2_proto", ""},
	{ "dsl2_svc_cat", ""},
	{ "dsl2_pcr", ""},
	{ "dsl2_scr", ""},
	{ "dsl2_mbs", ""},	
//
	{ "dsl3_enable", "0"},
	{ "dsl3_vpi", ""},
	{ "dsl3_vci", ""},
	{ "dsl3_encap", ""},
	{ "dsl3_proto", ""},
	{ "dsl3_svc_cat", ""},
	{ "dsl3_pcr", ""},
	{ "dsl3_scr", ""},
	{ "dsl3_mbs", ""},
//
	{ "dsl4_enable", "0"},
	{ "dsl4_vpi", ""},
	{ "dsl4_vci", ""},
	{ "dsl4_encap", ""},
	{ "dsl4_proto", ""},
	{ "dsl4_svc_cat", ""},
	{ "dsl4_pcr", ""},
	{ "dsl4_scr", ""},
	{ "dsl4_mbs", ""},
//
	{ "dsl5_enable", "0"},
	{ "dsl5_vpi", ""},
	{ "dsl5_vci", ""},
	{ "dsl5_encap", ""},
	{ "dsl5_proto", ""},
	{ "dsl5_svc_cat", ""},
	{ "dsl5_pcr", ""},
	{ "dsl5_scr", ""},
	{ "dsl5_mbs", ""},
//
	{ "dsl6_enable", "0"},
	{ "dsl6_vpi", ""},
	{ "dsl6_vci", ""},
	{ "dsl6_encap", ""},
	{ "dsl6_proto", ""},
	{ "dsl6_svc_cat", ""},
	{ "dsl6_pcr", ""},
	{ "dsl6_scr", ""},
	{ "dsl6_mbs", ""},
//
	{ "dsl7_enable", "0"},
	{ "dsl7_vpi", ""},
	{ "dsl7_vci", ""},
	{ "dsl7_encap", ""},
	{ "dsl7_proto", ""},
	{ "dsl7_svc_cat", ""},
	{ "dsl7_pcr", ""},
	{ "dsl7_scr", ""},
	{ "dsl7_mbs", ""},	
// number of PVC , program generated
	{ "dslx_config_num", "0"},
// for debug , program generated
	{ "dslx_debug", "0"},	
#endif

	// NVRAM for start_firewall/start_qos
	// QOS
	// Maybe removed later
	{ "qos_rulelist", "<Web Surf>>80>tcp>0~512>0<HTTPS>>443>tcp>0~512>0<File Transfer>>80>tcp>512~>3<File Transfer>>443>tcp>512~>3"},

	{ "qos_orates",	"80-100,10-100,5-100,3-100,2-95,0-0,0-0,0-0,0-0,0-0"},
	{ "qos_irates",	"100,100,100,100,100,0,0,0,0,0"},
	{ "qos_enable",			"0"				},
	{ "qos_method",			"0"				},
	{ "qos_sticky",			"1"				},
	{ "qos_ack",			"on"				},
	{ "qos_syn",			"on"				},
	{ "qos_fin",			"off"				},
	{ "qos_rst",			"off"				},
	{ "qos_icmp",			"on"				},
	{ "qos_reset",			"0"				},
	{ "qos_obw",			""				},
	{ "qos_ibw",			""				},
	{ "qos_orules",			"" 				},
	{ "qos_burst0",			""				},
	{ "qos_burst1",			""				},
	{ "qos_default",		"3"				},


	// TriggerList
	{ "autofw_enable_x", "0" },
	{ "autofw_rulelist", "" },

	// VSList
	{ "vts_enable_x", "0"},
	{ "vts_rulelist", ""},
	{ "vts_upnplist", ""},
	{ "vts_ftpport", "2021"},

	// DMZ
	{ "dmz_ip", "" },
	{ "sp_battle_ips", ""},

	// NVRAM for start_ddns
	{ "ddns_enable_x", "0"},
	{ "ddns_server_x", ""},
	{ "ddns_username_x", ""},
	{ "ddns_passwd_x", ""},
	{ "ddns_hostname_x", ""},
	{ "ddns_wildcard_x", "0"},

	// NVRAM for start_firewall
	// Firewall
#if defined (RTCONFIG_PARENTALCTRL) || defined(RTCONFIG_OLD_PARENTALCTRL)
	{"MULTIFILTER_ALL", "0"},
	{"MULTIFILTER_ENABLE", "" },
	{"MULTIFILTER_MAC", "" },
	{"MULTIFILTER_DEVICENAME", "" },
	{"MULTIFILTER_MACFILTER_DAYTIME", "" },
	{"MULTIFILTER_LANTOWAN_ENABLE", "" },
	{"MULTIFILTER_LANTOWAN_DESC", "" },
	{"MULTIFILTER_LANTOWAN_PORT", "" },
	{"MULTIFILTER_LANTOWAN_PROTO", "" },
	{"MULTIFILTER_URL_ENABLE", "" },
	{"MULTIFILTER_URL", "" },
#endif	/* RTCONFIG_PARENTALCTRL */
#ifdef RTCONFIG_YANDEXDNS
	{ "yadns_enable_x", "0"},	/* 0: disable, 1: enable Yandex.DNS */
	{ "yadns_mode", "1"},		/* 0: Undefended, 1: Safe, 2: Family */
	{ "yadns_rulelist", ""},	/* List client modes <devname>hh:ww:aa:dd:dd:rr>mode... */
#endif  /* RTCONFIG_YANDEXDNS */
#ifdef RTCONFIG_DNSFILTER
	{ "dnsfilter_enable_x", "0"},
	{ "dnsfilter_mode", "0"},	/* Default to no global filtering (only per client rules) */
	{ "dnsfilter_rulelist", ""},	/* List client modes <devname>hh:ww:aa:dd:dd:rr>mode... */
	{ "dnsfilter_custom1", ""},	/* User-defined DNS filter */
#endif
	{ "fw_enable_x", "1" },
	{ "fw_dos_x", "0" },
	{ "fw_log_x", "none" },
	{ "fw_pt_pptp", "1" },
	{ "fw_pt_l2tp", "1" },
	{ "fw_pt_ipsec", "1" },
	{ "fw_pt_rtsp", "1" },
	{ "fw_pt_h323", "1" },
	{ "fw_pt_sip", "1" },
	{ "fw_pt_pppoerelay", "0"},
	{ "misc_http_x", "0" },
	{ "misc_httpport_x", "8080" },
#ifdef RTCONFIG_HTTPS
	{ "misc_httpsport_x", "8443" },
#endif
	{ "misc_ping_x", "0" },
	{ "misc_lpr_x", "0" },	
	
	// UrlList
	{ "url_enable_x", "0"},
	{ "url_date_x", "1111111"},
	{ "url_time_x", "00002359"},
	{ "url_enable_x_1", "0"},
	{ "url_time_x_1", "00002359"},
	{ "url_rulelist", "" },

	// KeywordList
	{ "keyword_enable_x", "0"},
	{ "keyword_date_x", "1111111"},
	{ "keyword_time_x", "00002359"},
	{ "keyword_rulelist", ""},

	// MFList
	{ "macfilter_enable_x", "0"},
	{ "macfilter_num_x", "0"},
	{ "macfilter_rulelist", ""},

	// LWFilterListi
	{ "fw_lw_enable_x", "0"},
	{ "filter_lw_date_x", "1111111"},
	{ "filter_lw_time_x", "00002359"},
	{ "filter_lw_time2_x", "00002359"},
	{ "filter_lw_default_x", "ACCEPT"},
	{ "filter_lw_icmp_x", ""},
	{ "filter_lwlist", ""},

	// NVRAM for start_usb
	{ "usb_enable", "1"},
#ifdef RTCONFIG_USB_XHCI
	{ "usb_usb3", "0"},
#ifdef RTCONFIG_XHCIMODE
	{ "xhcimode_waitsec", "1"},
#endif
#endif
	{ "usb_usb2", "1"},
	{ "usb_ftpenable_x", "1"},
	{ "usb_ftpanonymous_x", "1"},
	{ "usb_ftpsuper_x", "0"},
	{ "usb_ftpport_x", "21"},
	{ "usb_ftpmax_x", "12"},
	{ "usb_ftptimeout_x", "120"},
	{ "usb_ftpstaytimeout_x", "240"},
	{ "usb_ftpscript_x", ""},
	{ "usb_ftpnum_x", "0"},
	{ "usb_bannum_x", "0"},
	#ifndef MR
	{ "qos_rulenum_x", "0"},
	#endif
	{ "usb_ftpusername_x", ""},
	{ "usb_ftppasswd_x", ""},
	{ "usb_ftpmaxuser_x", ""},
	{ "usb_ftprights_x", ""},
	{ "usb_ftpbanip_x", ""},

	// remain default setting control as tomato
	{ "usb_enable", "1"},
	{ "usb_uhci", "0"},
	{ "usb_ohci", "1"},
	{ "usb_usb2", "1"},
	{ "usb_irq_thresh", "0"},
	{ "usb_storage", "1"},
	{ "usb_printer", "1"},
	{ "usb_ext_opt", ""},
	{ "usb_fat_opt", ""},
	{ "usb_ntfs_opt", ""},
	{ "usb_hfs_opt", ""},
	{ "usb_fs_ext3", "1"},
	{ "usb_fs_fat", "1"},
#ifdef RTCONFIG_NTFS
	{ "usb_fs_ntfs", "1"},
	{ "usb_fs_ntfs_sparse", "0"},
#endif
#ifdef RTCONFIG_HFS
	{ "usb_fs_hfs", "1"},
#endif
	{ "usb_automount", "1"},
#ifdef LINUX26
	{ "usb_idle_timeout", "0"},
	{ "usb_idle_exclude", ""},
#endif
	{ "script_usbhotplug", ""},
	{ "script_usbmount", ""},
	{ "script_usbumount", ""},

	{ "smbd_enable", "1"},
	{ "smbd_autoshare", "1"},
	{ "smbd_cpage", ""},
	{ "smbd_cset", "utf8"},
	{ "smbd_custom", ""},
	{ "smbd_master", "0"},
	{ "smbd_passwd", ""},
	{ "smbd_shares", "share</mnt<Default Share<1<0>root$</Hidden Root<0<1"},
	{ "smbd_user", "nas"},
	{ "smbd_wgroup", "WORKGROUP"},
	{ "smbd_wins", "0"},
	{ "smbd_simpler_naming", "0"},
	{ "smbd_bind_wan", "0"},

#ifdef RTCONFIG_NFS
	{ "nfsd_enable", "0"},
	{ "nfsd_enable_v2", "0"},
	{ "nfsd_exportlist", ""},
#endif

	{ "log_ipaddr", ""},
	{ "log_port", "514"},
	{ "log_size", "256"},
	{ "log_level", "7" },		/* <  LOG_DEBUG */
	{ "console_loglevel", "5"},	/* <  KERN_INFO */

#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2)
	{ "jffs2_on", "1" },
	{ "jffs2_exec", "" },
	{ "jffs2_format", "0" },
#endif

#ifdef RTCONFIG_UBIFS
	{ "ubifs_on", "1" },
	{ "ubifs_exec", "" },
#endif

#ifdef RTCONFIG_USB
	{ "acc_num", "1"},
	{ "acc_list", "admin>admin"},
	{ "st_samba_mode", "1"},
	{ "st_ftp_mode", "2"},
	{ "enable_ftp", "0"},
	{ "enable_samba", "1"},
	{ "st_max_user", "5"},
	{ "computer_name", ""},
	{ "st_samba_workgroup", "WORKGROUP"},
	{ "ftp_lang", "EN" },

//#ifdef RTCONFIG_WEBDAV
	{ "enable_webdav", "0"}, // 0: Disable, 1: enable
	{ "st_webdav_mode", "2"}, // 0: http, 1: https, 2: both
	{ "webdav_proxy", "0"},
	{ "webdav_aidisk", "0"},
	{ "webdav_http_port", "8082"},
	{ "webdav_https_port", "443"},
	{ "acc_webdavproxy", "admin>1"}, //0: Only show USBDisk, 1: show USBDisk and Smb pc
	{ "enable_webdav_captcha", "0"}, // 0: disable, 1: enable
	{ "enable_webdav_lock", "0"}, // 0: disable, 1: enable
	{ "webdav_acc_lock", "0"}, // 0: unlock account, 1: lock account
	{ "webdav_lock_interval", "2"},
	{ "webdav_lock_times", "3"},
	{ "webdav_last_login_info", ""},
//#endif

//#ifdef RTCONFIG_CLOUDSYNC
	{ "enable_cloudsync", "0" },
	{ "cloud_sync", ""},
	{ "share_link_param", ""},
	{ "share_link_result", ""},
	{ "share_link_host", ""},
//#endif

#ifdef RTCONFIG_DISK_MONITOR
	{ "diskmon_policy", "disk"}, // all, disk, part
	{ "diskmon_part", ""}, // sda1, sdb1
	{ "diskmon_force_stop", "0"}, // 0: disable, 1: stop if possible

	{ "diskmon_usbport", ""}, // 1, 2
	{ "diskmon_freq", "0"}, // 0: disable, 1: Month, 2: Week, 3: Hour
	{ "diskmon_freq_time", ""}, // DAY>WEEK>HOUR
	{ "usb_path1_diskmon_freq", "0"},
	{ "usb_path1_diskmon_freq_time", ""},
	{ "usb_path2_diskmon_freq", "0"},
	{ "usb_path2_diskmon_freq_time", ""},
	{ "usb_path3_diskmon_freq", "0"},
	{ "usb_path3_diskmon_freq_time", ""},
#ifndef RTCONFIG_BCMARM
	{ "diskremove_bad_device", "1"}, // 0: disable, 1: remove the bad device
#else
	{ "diskremove_bad_device", "0"},
#endif
#endif
#endif

#ifdef RTCONFIG_HTTPS
	{ "https_lanport", "8443"},
	{ "https_crt_file", ""},
	{ "https_crt_save", "0"},
	{ "https_crt_gen", "1"},
	{ "https_crt_cn", ""},
#endif

#ifdef RTCONFIG_MEDIA_SERVER
	{ "dms_enable", "1" 	},
	{ "dms_rescan", "1"	},
	{ "dms_port", "8200" 	},
	{ "dms_dbdir", "/var/cache/minidlna"	},
	{ "dms_dir", "/tmp/mnt" },
	{ "dms_tivo", "0"	},
	{ "dms_stdlna", "0"	},
	{ "dms_sas", 	"0"	},
	{ "daapd_enable", "0" 	},
#endif

#ifdef DM
	{ "apps_dl", "1"},
	{ "apps_dl_share", "0"},
	{ "apps_dl_seed", "24"},
	{ "apps_dms", "1"},
	{ "apps_caps", "0"},
	{ "apps_comp", "0"},
	{ "apps_pool", "harddisk/part0"},
	{ "apps_share", "share"},
	{ "apps_ver", ""},
	{ "apps_seed", "0"},
	{ "apps_upload_max", "0"},
	{ "machine_name", ""},

	{ "apps_dms_usb_port", "1"},
	{ "apps_running", "0"},
	{ "apps_dl_share_port_from", "10001"},
	{ "apps_dl_share_port_to", "10050"},

	{ "apps_installed", "0"},

	{ "sh_num", "0"},
#endif

#ifndef RTCONFIG_BCMARM
	{ "apps_ipkg_old", "1" },
#endif
	{ "apps_ipkg_server", "" },
	{ "apps_wget_timeout", "30" },
	{ "apps_local_space", "/rom" },
	{ "apps_swap_enable", "0" },
	{ "apps_swap_threshold", "" }, // 72M
	{ "apps_swap_file", ".swap" },
	{ "apps_swap_size", "33000" }, // 32M
	{ "apps_state_autofix", "1" },

	{ "http_username", "admin" },
	{ "http_passwd", "admin" },
	{ "http_enable", "0"}, // 0: http, 1: https, 2: both
	{ "http_client", "0"},
	{ "http_clientlist", "0"},

	{ "temp_lang", ""},
	{ "wollist", ""},
	{ "httpd_die_reboot", ""},

	{ "x_Setting", "0"},		// is any setting set
	{ "r_Setting", "0"},		// is repeater set
	{ "w_Setting", "0"},		// is wilreess set

	{ "asus_mfg", "0"},		// 2008.03 James.
	{ "asus_mfg_flash", ""},	// 2008.03 James.
	{ "btn_rst", "0"},		// 2008.03 James.
	{ "btn_ez", "0"},		// 2008.03 James.
	{ "btn_ez_radiotoggle", "0"},	// Turn WPS into radio toggle
#ifdef RTCONFIG_WIFI_TOG_BTN
	{ "btn_wifi_toggle", "0"},
#endif
#ifdef RTCONFIG_LED_BTN
	{ "btn_led", "0"},
	{ "btn_led_mode", "0"},
#endif
#ifdef RTCONFIG_TURBO
	{ "btn_turbo", "0"},
#endif
	 /* APCLI/STA parameters */
	#if 0
	{ "sta_ssid", "", 0 },
	{ "sta_bssid", "", 0 },
	{ "sta_auth_mode", "open", 0 },
	{ "sta_wep_x", "0", 0 },
	{ "sta_crypto", "tkip", 0 },
	{ "sta_wpa_psk", "12345678", 0 },
	{ "sta_key", "1", 0 },
	{ "sta_key_type", "0", 0 },
	{ "sta_key1", "", 0 },
	{ "sta_key2", "", 0 },
	{ "sta_key3", "", 0 },
	{ "sta_key4", "", 0 },
	{ "sta_check_ha", "0", 0 },
	{ "sta_authorized", "0", 0 },
	{ "sta_connected", "0", 0 },
	#endif

	{ "record_lanaddr", ""},
	{ "telnetd_enable", "0"},

	{ "Dev3G", "AUTO"},
	{ "modem_enable", "1"}, // 0: disabled, 1: WCDMA, 2: CDMA2000, 3: TD-SCDMA, 4: WiMAX.
	{ "modem_pincode", ""},
	{ "modem_apn", "internet"},
	{ "modem_country", ""},
	{ "modem_isp", ""},
	{ "modem_dialnum", "*99#"},
	{ "modem_user", ""},
	{ "modem_pass", ""},
	{ "modem_ttlsid", ""},
	{ "modem_running", "0"},
#ifdef RTCONFIG_USB_MODEM_PIN
	{ "modem_pincode_opt", "1"},
#endif

	{ "udpxy_enable_x", "0"},

	/* traffic monitor - added by jerry5 2009/07 */
	{"rstats_enable", "1"},
	{"rstats_path", ""},
	{"rstats_new", "0"},
	{"rstats_stime", "1"},
	{"rstats_offset", "1"},
	{"rstats_data", ""},
	{"rstats_colors", ""},
	{"rstats_exclude", ""},
	{"rstats_sshut", "1"},
	{"rstats_bak", "0"},

	/* IPTraffic traffic monitor */
	{ "cstats_enable", "0"},
	{ "cstats_exclude",""},
	{ "cstats_include", ""},
	{ "cstats_all", "1"},
	{ "cstats_sshut", "1"},
	{ "cstats_new", "0"},

	{"http_id", "TIDe855a6487043d70a"},

	{"asus_debug", "0"},
	{"di_debug", "0"},
	{"dw_debug", "0"},
	{"v2_debug", "0"},
	{"env_path", ""},
	{"debug_logeval", "0"},
	{"debug_cprintf", "0"},
	{"debug_cprintf_file", "0"},
	{"debug_logrc", "0"},
	{"debug_ovrc", "0"},
	{"debug_abrst", "0"},
	{"https_enable", "0"},
	{"upnp_min_port_int", "1024"},
	{"upnp_max_port_int", "65535"},
	{"upnp_min_port_ext", "1"},
	{"upnp_max_port_ext", "65535"},
	{"mfp_ip_monopoly", ""},
	#if (!defined(W7_LOGO) && !defined(WIFI_LOGO))
	{"telnetd", "0"},
	#else
	{"telnetd", "1"},
	#endif

#if defined(RTCONFIG_SSH)
	{"sshd_enable", "0"},
	{"sshd_port", "22"},
	{"sshd_pass","1"},
	{"sshd_authkeys",""},
	{"sshd_forwarding","0"},
	{"sshd_wan","0"},
	{"sshd_bfp","0"},
#endif

#if defined(RTCONFIG_PPTPD) || defined(RTCONFIG_ACCEL_PPTPD)
	{"pptpd_enable", 	"0"},
	{"pptpd_broadcast", 	"disable"},
	{"pptpd_chap", 		"0"},	 // 0/1/2(Auto/MS-CHAPv1/MS-CHAPv2)
	{"pptpd_mppe", 		"0"}, 	 // 1|2|4|8(MPPE-128|MPPE-56|MPPE-40|No Encryption)
	{"pptpd_dns1", 		""},
	{"pptpd_dns2", 		""},
	{"pptpd_wins1", 	""},
	{"pptpd_wins2", 	""},
	{"pptpd_server", 	""},
	{"pptpd_clients", 	"192.168.10.2-11"},
	{"pptpd_clientlist", 	""},
	{"pptpd_mru",		"1450"},
	{"pptpd_mtu",		"1450"},
#endif

#if defined(RTCONFIG_VPNC)
	{ "vpnc_proto", "disable"},
	{ "vpnc_pppoe_username", ""},
	{ "vpnc_pppoe_passwd", ""},
	{ "vpnc_heartbeat_x", ""},
	{ "vpnc_dnsenable_x", "1"},
	{ "vpnc_pppoe_options_x", ""},
	{ "vpnc_pptp_options_x", ""},
	{ "vpnc_state_t", "0"},
	{ "vpnc_sbstate_t", "0"},
	{ "vpnc_clientlist", ""},
	{ "vpnc_connect_row", ""},
#endif

#if defined(RTCONFIG_PPTPD) || defined(RTCONFIG_ACCEL_PPTPD) || defined(RTCONFIG_OPENVPN)
	{"VPNServer_enable",	"0"},
	{"VPNServer_mode",	"pptpd"},
	{"VPNClient_rule",	""},
#endif

#if defined(RTCONFIG_VPNC) || defined(RTCONFIG_OPENVPN)
	{"VPNClient_enable",	"0"},
	{"VPNClient_rule",	""},
#endif

#ifdef RTCONFIG_OPENVPN
	{ "vpn_upload_state",		""		},
	{ "vpn_upload_unit",		""		},
	{ "vpn_upload_type",		""		},
	// openvpn
	{ "vpn_debug",			"0"		},
	{ "vpn_loglevel",		"3"		},
	{ "vpn_server_unit",		"1"		},
	{ "vpn_serverx_eas",		""		},
	{ "vpn_serverx_dns",		""		},
	{ "vpn_server_poll",		"0"		},
	{ "vpn_server_if",		"tun"		},
	{ "vpn_server_proto",		"udp"		},
	{ "vpn_server_port",		"1194"		},
	{ "vpn_server_firewall",	"auto"		},
	{ "vpn_server_crypt",		"tls"		},
	{ "vpn_server_comp",		"adaptive"	},
	{ "vpn_server_cipher",		"default"	},
	{ "vpn_server_dhcp",		"1"		},
	{ "vpn_server_r1",		"192.168.1.50"	},
	{ "vpn_server_r2",		"192.168.1.55"	},
	{ "vpn_server_sn",		"10.8.0.0"	},
	{ "vpn_server_nm",		"255.255.255.0"	},
	{ "vpn_server_local",		"10.8.0.1"	},
	{ "vpn_server_remote",		"10.8.0.2"	},
	{ "vpn_server_reneg",		"-1"		},
	{ "vpn_server_hmac",		"-1"		},
	{ "vpn_server_plan",		"1"		},
	{ "vpn_server_ccd",		"0"		},
	{ "vpn_server_c2c",		"0"		},
	{ "vpn_server_ccd_excl",	"0"		},
	{ "vpn_server_ccd_val",		""		},
	{ "vpn_server_pdns",		"0"		},
	{ "vpn_server_rgw",		"0"		},
	{ "vpn_server_custom",		""		},
	{ "vpn_server_igncrt",		"1"		},
	{ "vpn_server_userpass_auth",	"0"		},
	{ "vpn_serverx_clientlist",	""		},
	{ "vpn_server1_poll",		"0"		},
	{ "vpn_server1_if",		"tun"		},
	{ "vpn_server1_proto",		"udp"		},
	{ "vpn_server1_port",		"1194"		},
	{ "vpn_server1_firewall",	"auto"		},
	{ "vpn_server1_crypt",		"tls"		},
	{ "vpn_server1_comp",		"adaptive"	},
	{ "vpn_server1_cipher",		"default"	},
	{ "vpn_server1_dhcp",		"1"		},
	{ "vpn_server1_r1",		"192.168.1.50"	},
	{ "vpn_server1_r2",		"192.168.1.55"	},
	{ "vpn_server1_sn",		"10.8.0.0"	},
	{ "vpn_server1_nm",		"255.255.255.0"	},
	{ "vpn_server1_local",		"10.8.0.1"	},
	{ "vpn_server1_remote",		"10.8.0.2"	},
	{ "vpn_server1_reneg",		"-1"		},
	{ "vpn_server1_hmac",		"-1"		},
	{ "vpn_server1_plan",		"1"		},
	{ "vpn_server1_ccd",		"0"		},
	{ "vpn_server1_c2c",		"0"		},
	{ "vpn_server1_ccd_excl",	"0"		},
	{ "vpn_server1_ccd_val",	""		},
	{ "vpn_server1_pdns",		"0"		},
	{ "vpn_server1_rgw",		"0"		},
	{ "vpn_server1_custom",		""		},
	{ "vpn_server1_userpass_auth",	"0"		},
	{ "vpn_server1_igncrt",		"1"		},
	{ "vpn_server1_state",		"0"		},
	{ "vpn_server1_errno",		"0"		},
	{ "vpn_crt_server1_static",	""		},
	{ "vpn_crt_server1_ca",		""		},
	{ "vpn_crt_server1_ca_key",	""		},
	{ "vpn_crt_server1_crt",	""		},
	{ "vpn_crt_server1_key",	""		},
	{ "vpn_crt_server1_client_crt",	""		},
	{ "vpn_crt_server1_client_key",	""		},
	{ "vpn_crt_server1_dh",		""		},
	{ "vpn_server2_poll",		"0"		},
	{ "vpn_server2_if",		"tun"		},
	{ "vpn_server2_proto",		"udp"		},
	{ "vpn_server2_port",		"1194"		},
	{ "vpn_server2_firewall",	"auto"		},
	{ "vpn_server2_crypt",		"tls"		},
	{ "vpn_server2_comp",		"adaptive"	},
	{ "vpn_server2_cipher",		"default"	},
	{ "vpn_server2_dhcp",		"1"		},
	{ "vpn_server2_r1",		"192.168.1.50"	},
	{ "vpn_server2_r2",		"192.168.1.55"	},
	{ "vpn_server2_sn",		"10.16.0.0"	},
	{ "vpn_server2_nm",		"255.255.255.0"	},
	{ "vpn_server2_local",		"10.16.0.1"	},
	{ "vpn_server2_remote",		"10.16.0.2"	},
	{ "vpn_server2_reneg",		"-1"		},
	{ "vpn_server2_hmac",		"-1"		},
	{ "vpn_server2_plan",		"1"		},
	{ "vpn_server2_ccd",		"0"		},
	{ "vpn_server2_c2c",		"0"		},
	{ "vpn_server2_ccd_excl",	"0"		},
	{ "vpn_server2_ccd_val",	""		},
	{ "vpn_server2_pdns",		"0"		},
	{ "vpn_server2_rgw",		"0"		},
	{ "vpn_server2_custom",		""		},
	{ "vpn_server2_userpass_auth",	"0"		},
	{ "vpn_server2_igncrt",		"1"		},
	{ "vpn_server2_state",		"0"		},
	{ "vpn_server2_errno",		"0"		},
	{ "vpn_crt_server2_static",	""		},
	{ "vpn_crt_server2_ca",		""		},
	{ "vpn_crt_server2_ca_key",	""		},
	{ "vpn_crt_server2_crt",	""		},
	{ "vpn_crt_server2_key",	""		},
	{ "vpn_crt_server2_client_crt",	""		},
	{ "vpn_crt_server2_client_key",	""		},
	{ "vpn_crt_server2_dh",		""		},
	{ "vpn_client_unit",		"1"		},
	{ "vpn_clientx_eas",		""		},
	{ "vpn_client1_poll",		"0"		},
	{ "vpn_client1_if",		"tun"		},
	{ "vpn_client1_bridge",		"1"		},
	{ "vpn_client1_nat",		"1"		},
	{ "vpn_client1_proto",		"udp"		},
	{ "vpn_client1_addr",		""		},
	{ "vpn_client1_port",		"1194"		},
	{ "vpn_client1_retry",		"30"		},
	{ "vpn_client1_rg",		"0"		},
	{ "vpn_client1_firewall",	"auto"		},
	{ "vpn_client1_crypt",		"tls"		},
	{ "vpn_client1_comp",		"adaptive"	},
	{ "vpn_client1_cipher",		"default"	},
	{ "vpn_client1_local",		"10.8.0.2"	},
	{ "vpn_client1_remote",		"10.8.0.1"	},
	{ "vpn_client1_nm",		"255.255.255.0"	},
	{ "vpn_client1_reneg",		"-1"		},
	{ "vpn_client1_hmac",		"-1"		},
	{ "vpn_client1_adns",		"0"		},
	{ "vpn_client1_rgw",		"0"		},
	{ "vpn_client1_gw",		""		},
	{ "vpn_client1_custom",		""		},
	{ "vpn_client1_cn",		""		},
	{ "vpn_client1_tlsremote",	"0"		},
	{ "vpn_crt_client1_static",	""		},
	{ "vpn_crt_client1_ca",		""		},
	{ "vpn_crt_client1_crt",	""		},
	{ "vpn_crt_client1_key",	""		},
	{ "vpn_client1_userauth",	"0"		},
	{ "vpn_client1_username",	""		},
	{ "vpn_client1_password",	""		},
	{ "vpn_client1_useronly",	"0"		},
	{ "vpn_client1_state",		"0"		},
	{ "vpn_client1_errno",		"0"		},
	{ "vpn_client2_poll",		"0"		},
	{ "vpn_client2_if",		"tun"		},
	{ "vpn_client2_bridge",		"1"		},
	{ "vpn_client2_nat",		"1"		},
	{ "vpn_client2_proto",		"udp"		},
	{ "vpn_client2_addr",		""		},
	{ "vpn_client2_port",		"1194"		},
	{ "vpn_client2_retry",		"30"		},
	{ "vpn_client2_rg",		"0"		},
	{ "vpn_client2_firewall",	"auto"		},
	{ "vpn_client2_crypt",		"tls"		},
	{ "vpn_client2_comp",		"adaptive"	},
	{ "vpn_client2_cipher",		"default"	},
	{ "vpn_client2_local",		"10.16.0.2"	},
	{ "vpn_client2_remote",		"10.16.0.1"	},
	{ "vpn_client2_nm",		"255.255.255.0"	},
	{ "vpn_client2_reneg",		"-1"		},
	{ "vpn_client2_hmac",		"-1"		},
	{ "vpn_client2_adns",		"0"		},
	{ "vpn_client2_rgw",		"0"		},
	{ "vpn_client2_gw",		""		},
	{ "vpn_client2_custom",		""		},
	{ "vpn_client2_cn",		""		},
	{ "vpn_client2_tlsremote",	"0"		},
	{ "vpn_client2_useronly",	"0"		},
	{ "vpn_crt_client2_static",	""		},
	{ "vpn_crt_client2_ca",		""		},
	{ "vpn_crt_client2_crt",	""		},
	{ "vpn_crt_client2_key",	""		},
	{ "vpn_client2_userauth",	"0"		},
	{ "vpn_client2_username",	""		},
	{ "vpn_client2_password",	""		},
	{ "vpn_client2_state",		"0"		},
	{ "vpn_client2_errno",		"0"		},
	{ "vpn_client_poll",		"0"		},
	{ "vpn_client_if",		"tun"		},
	{ "vpn_client_bridge",		"1"		},
	{ "vpn_client_nat",		"1"		},
	{ "vpn_client_proto",		"udp"		},
	{ "vpn_client_addr",		""		},
	{ "vpn_client_port",		"1194"		},
	{ "vpn_client_retry",		"30"		},
	{ "vpn_client_rg",		"0"		},
	{ "vpn_client_firewall",	"auto"		},
	{ "vpn_client_crypt",		"tls"		},
	{ "vpn_client_comp",		"adaptive"	},
	{ "vpn_client_cipher",		"default"	},
	{ "vpn_client_local",		"10.8.0.2"	},
	{ "vpn_client_remote",		"10.8.0.1"	},
	{ "vpn_client_nm",		"255.255.255.0"	},
	{ "vpn_client_reneg",		"-1"		},
	{ "vpn_client_hmac",		"-1"		},
	{ "vpn_client_adns",		"0"		},
	{ "vpn_client_rgw",		"0"		},
	{ "vpn_client_gw",		""		},
	{ "vpn_client_cn",		""		},
	{ "vpn_client_tlsremote",	"0"		},
	{ "vpn_client_custom",		""		},
	{ "vpn_client_userauth",	"0"		},
	{ "vpn_client_username",	""		},
	{ "vpn_client_password",	""		},
	{ "vpn_client_useronly",	"0"		},
#endif

#ifdef RTCONFIG_PUSH_EMAIL
	{ "PM_enable", "0"},                            /* Enable Push Mail feature. */
	{ "PM_type", "0"},                              /* Send the tracking to the of xDSL team's official gmail.  */
	{ "PM_SMTP_SERVER", ""},
	{ "PM_SMTP_PORT", ""},
	{ "PM_MY_NAME", ""},
	{ "PM_MY_EMAIL", ""},
	{ "PM_USE_TLS", "true"},
	{ "PM_SMTP_AUTH", "LOGIN"},
	{ "PM_SMTP_AUTH_USER", ""},
	{ "PM_SMTP_AUTH_PASS", ""},
	{ "PM_MAIL_SUBJECT", ""},
	{ "PM_LETTER_CONTENT", ""},
	{ "PM_LETTER_PATH", ""},
	{ "PM_MAIL_FILE", ""},
	{ "PM_MAIL_TARGET", ""},
	{ "PM_title", ""},                              /* The title of mail. */
	{ "PM_target", ""},                             /* The address of Mail Server. */
	{ "PM_restart", "0"},                           /* reset the Push Mail Service. */
	{ "PM_freq", "0"},                              /* 0:daily, 1:weekly, 2:monthly. */
	{ "PM_mon", "0"},                               /* months since January (0 to 11). */
	{ "PM_day", "0"},                               /* days since Sunday (0 to 6 Sunday=0). */
	{ "PM_hour", "0"},                              /* hours since midnight (0 to 23). */
	{ "pushnotify_httplogin", "1"},
	{ "pushnotify_diskmonitor", "1"},
#endif

#ifdef  __CONFIG_NORTON__
	{ "nga_lickey",			"0"		},
	{ "nga_user",			"0"		},
	{ "nga_pass",			"0"		},
	{ "nga_devname",		"0"		},
	{ "nga_ncw",			"0"		},
	{ "nga_enable",			"0"		},
	{ "nga_gact",			"0"		},
	{ "NGA_MID",			""		},
	{ "ngap_0",			""		},
	{ "ngap_1",			""		},
#endif /* __CONFIG_NORTON__ */

#ifdef RTCONFIG_IPV6
	// IPv6 parameters
	{ "ipv6_service",	"disabled"	},	// disabled/staic6/dhcp6/6to4/6in4/6rd/other
	{ "ipv6_ifdev",		"ppp"		},	
	{ "ipv6_prefix",	""		},	// The global-scope IPv6 prefix to route/advertise
	{ "ipv6_prefix_length",	"64"		},	// The bit length of the prefix. Used by dhcp6c. For radvd, /64 is always assumed.
	{ "ipv6_dhcp_pd",	"1"		},	// Enable DHCP-PD (DHCP Prefix Delegation) by default
	{ "ipv6_rtr_addr",	""		},	// defaults to $ipv6_prefix::1
	{ "ipv6_prefix_len_wan","64"		},	// used in ipv6_service other
	{ "ipv6_ipaddr",	""		},	// used in ipv6_service other
	{ "ipv6_gateway",	""		},	// used in ipv6_service other
	{ "ipv6_radvd",		"1"		},	// Enable Router Advertisement (radvd)
	{ "ipv6_accept_ra",	"1"		},	// Accept RA from WAN (0x1) / LAN (0x2)
	{ "ipv6_ifname",	"six0"		},	// The interface facing the rest of the IPv6 world
	{ "ipv6_relay",		"192.88.99.1"	},	// IPv6 Anycast Address
	{ "ipv6_tun_v4end",	"0.0.0.0"	},	// Foreign IPv4 endpoint of SIT tunnel
	{ "ipv6_tun_addr",	""		},	// IPv6 address to assign to local tunnel endpoint
	{ "ipv6_tun_addrlen",	"64"		},	// CIDR prefix length for tunnel's IPv6 address	
	{ "ipv6_tun_mtu",	"0"		},	// Tunnel MTU, 0 for default
	{ "ipv6_tun_ttl",	"255"		},	// Tunnel TTL
	{ "ipv6_6rd_dhcp",	"1"		},
	{ "ipv6_6rd_router", 	"0.0.0.0"	},
	{ "ipv6_6rd_ip4size", 	"0"		},
	{ "ipv6_6rd_prefix",	""		},
	{ "ipv6_6rd_prefixlen", "32"		},
#if 0
	{ "ipv6_dns",		""		},	// DNS server(s) IPs
#else
	{ "ipv6_dns1",		""		},
	{ "ipv6_dns2",		""		},
	{ "ipv6_dns3",		""		},
#endif
	{ "ipv6_get_dns",	""		},	// DNS IP address which got by dhcp6c
	{ "ipv6_dnsenable",	"1"		},
	{ "ipv6_get_domain",	""		},	// domain name which got by dhcp6c
	{ "ipv6_debug",		"0"		},
#endif
	{ "ipv6_autoconf_type", "0"		},	// IPv6 LAN auconfiguration type, 0: stateless, 1: stateful
	{ "ipv6_dhcp_lifetime", "86400"		},	// valid lifetimes of IPv6 prefix in seconds
	{ "ipv6_dhcp_start",	""		},	// start of address pool
	{ "ipv6_dhcp_end",	""		},	// end of address pool
	{ "ipv6_fw_enable",	"1"		},	// Default FORWARD table to DROP packets
	{ "ipv6_fw_rulelist",	""		},	// IPv6 allowed forward rules
	{ "ipv6_ra_conf",	"noneset"	},	// address configuration from WAN router advertisement
	{ "ipv6_dhcp6s_enable",	"1"		},	// DHCP6 Server for LAN

	{ "web_redirect", 	"1"		},	// Only NOLINK is redirected in default, it is overwrited in init_nvram
	{ "disiosdet",		"1"		},

#ifdef RTCONFIG_FANCTRL
	{ "fanctrl_dutycycle",		"0"},
#endif
#ifdef RTCONFIG_SHP
	{ "lfp_disable", 		"0"},
#endif
#ifdef RTCONFIG_ISP_METER
	{ "isp_meter",			"disable"},
	{ "isp_limit",			"0"},
	{ "isp_day_rx",			"0"},
	{ "isp_day_tx",			"0"},
	{ "isp_month_rx",		"0"},
	{ "isp_month_tx",		"0"},
	{ "isp_limit_time",		"0"},
	{ "isp_connect_time",		"0"},
#endif
	{ "Ate_version",		"1.0"},
	{ "Ate_power_on_off_ver",	"2.4"},
	{ "Ate_power_on_off_enable",	"0"},
	{ "Ate_reboot_count",		"100"},
	{ "Ate_rc_check",		"0"},
	{ "Ate_dev_check",		"0"},
	{ "Ate_boot_check",		"0"},
	{ "Ate_total_fail_check",	"0"},
	{ "Ate_dev_fail",		"0"},
	{ "Ate_boot_fail",		"0"},
	{ "Ate_total_fail",		"10"},
	{ "Ate_continue_fail",		"3"},
	{ "dev_fail_reboot",		"3"},
	// Wireless parameters
#ifdef RTCONFIG_TIMEMACHINE
	{"timemachine_enable", 		"0"},
	{"tm_device_name", 		""},
	{"tm_vol_size", 		"0"},
	{"tm_partition_num", 		""},
	{"tm_ui_setting", 		"0"},
	{"tm_usb_path_vid", 		""},
	{"tm_usb_path_pid", 		""},
	{"tm_usb_path_serial", 		""},
	{"tm_debug", 			"0"},
#endif
	{ "Ate_fw_fail",		"10"},
	{ "Ate_reboot_delay",		"1"},
#ifdef RTCONFIG_USER_LOW_RSSI
	{ "wl_user_rssi",		"0"},		/* disabled by default, setting range: -70 ~ -90 */
	{ "wl_lrc",			"2"},
#endif
	{ "webui_resolve_conn", "0"},
	{ "led_disable", "0"},

	{ NULL, NULL }
};

// nvram for system control state
struct nvram_tuple router_state_defaults[] = {
	{ "rc_service", ""},

	{ "asus_mfg", "0"},
	{ "btn_rst", "0"},
	{ "btn_ez", "0"},

	{ "wan_primary", "0" }, // Always first run in WAN port.
	{ "wan0_primary", "1" },
	{ "wan1_primary", "0" },
	{ "wan_pppoe_ifname", "" },
	{ "wan_state_t", "0" },
	{ "wan_sbstate_t", "0" },	// record substate for each wan state
	{ "wan_auxstate_t", "0" },	// which is maintained by wanduck
	{ "wan_proto_t", "" },
	{ "nat_state", "0"},
	{ "link_wan", ""},
	{ "link_wan1", ""},
#ifdef RTCONFIG_IPV6
	{ "wan_6rd_router", ""	},
	{ "wan_6rd_ip4size", ""	},
	{ "wan_6rd_prefix", ""	},
	{ "wan_6rd_prefixlen", "" },
	{ "ipv6_state_t", "0" },
	{ "ipv6_sbstate_t", "0" },
#endif
	{ "lan_state_t", "0" },
	{ "lan_sbstate_t", "0" },	// record substate for each wan state
	{ "lan_auxstate_t", "0" },	// whcih is mtainted by wanduck

	{ "autodet_state", "0"},
	{ "autodet_auxstate", "0"},
	{ "invoke_later", "0"},

#if defined (RTCONFIG_WIRELESSREPEATER) || defined (RTCONFIG_PROXYSTA)
	// Wireless Client State
	{ "wlc_state", "0"},
	{ "wlc_sbstate", "0"},
	{ "wlc_mode", "0"},
#endif
	{ "wlc_scan_state", "0"},
	{ "wlc_scan_mode", "0"},	// 0=active 1=passive

#ifdef RTCONFIG_MEDIA_SERVER
	{ "dms_state", ""},
 	{ "dms_dbcwd", ""},
#endif

#ifdef RTCONFIG_USBRESET
	{ "usbreset_active", "0"},
	{ "usbreset_num", "0"},
#endif

	// USB state
#if 0
	{ "usb_path1", "" },
	{ "usb_path1_act", "" },
	{ "usb_path1_vid", "" },
	{ "usb_path1_pid", "" },
	{ "usb_path1_manufacturer", "" },
	{ "usb_path1_product", "" },
	{ "usb_path1_serial", "" },
	{ "usb_path1_speed", "" },
	{ "usb_path1_removed", "0" },
#ifdef RTCONFIG_DISK_MONITOR
	{ "usb_path1_pool_error", "0" },
#endif
	{ "usb_path1_fs_path0", "" },
#if 0
	{ "usb_path1_fs_path1", "" },
	{ "usb_path1_fs_path2", "" },
	{ "usb_path1_fs_path3", "" },
	{ "usb_path1_fs_path4", "" },
	{ "usb_path1_fs_path5", "" },
	{ "usb_path1_fs_path6", "" },
	{ "usb_path1_fs_path7", "" },
	{ "usb_path1_fs_path8", "" },
	{ "usb_path1_fs_path9", "" },
	{ "usb_path1_fs_path10", "" },
	{ "usb_path1_fs_path11", "" },
	{ "usb_path1_fs_path12", "" },
	{ "usb_path1_fs_path13", "" },
	{ "usb_path1_fs_path14", "" },
	{ "usb_path1_fs_path15", "" },
	{ "usb_path1_label0", "" },
	{ "usb_path1_label1", "" },
	{ "usb_path1_label2", "" },
	{ "usb_path1_label3", "" },
	{ "usb_path1_label4", "" },
	{ "usb_path1_label5", "" },
	{ "usb_path1_label6", "" },
	{ "usb_path1_label7", "" },
	{ "usb_path1_label8", "" },
	{ "usb_path1_label9", "" },
	{ "usb_path1_label10", "" },
	{ "usb_path1_label11", "" },
	{ "usb_path1_label12", "" },
	{ "usb_path1_label13", "" },
	{ "usb_path1_label14", "" },
	{ "usb_path1_label15", "" },
#endif

	{ "usb_path2", "" },
	{ "usb_path2_act", "" },
	{ "usb_path2_vid", "" },
	{ "usb_path2_pid", "" },
	{ "usb_path2_manufacturer", "" },
	{ "usb_path2_product", "" },
	{ "usb_path2_serial", "" },
	{ "usb_path2_speed", "" },
	{ "usb_path2_removed", "0" },
#ifdef RTCONFIG_DISK_MONITOR
	{ "usb_path2_pool_error", "0" },
#endif
	{ "usb_path2_fs_path0", "" },
#if 0
	{ "usb_path2_fs_path1", "" },
	{ "usb_path2_fs_path2", "" },
	{ "usb_path2_fs_path3", "" },
	{ "usb_path2_fs_path4", "" },
	{ "usb_path2_fs_path5", "" },
	{ "usb_path2_fs_path6", "" },
	{ "usb_path2_fs_path7", "" },
	{ "usb_path2_fs_path8", "" },
	{ "usb_path2_fs_path9", "" },
	{ "usb_path2_fs_path10", "" },
	{ "usb_path2_fs_path11", "" },
	{ "usb_path2_fs_path12", "" },
	{ "usb_path2_fs_path13", "" },
	{ "usb_path2_fs_path14", "" },
	{ "usb_path2_fs_path15", "" },
	{ "usb_path2_label0", "" },
	{ "usb_path2_label1", "" },
	{ "usb_path2_label2", "" },
	{ "usb_path2_label3", "" },
	{ "usb_path2_label4", "" },
	{ "usb_path2_label5", "" },
	{ "usb_path2_label6", "" },
	{ "usb_path2_label7", "" },
	{ "usb_path2_label8", "" },
	{ "usb_path2_label9", "" },
	{ "usb_path2_label10", "" },
	{ "usb_path2_label11", "" },
	{ "usb_path2_label12", "" },
	{ "usb_path2_label13", "" },
	{ "usb_path2_label14", "" },
	{ "usb_path2_label15", "" },
#endif

	{ "usb_path3", "" },
	{ "usb_path3_act", "" },
	{ "usb_path3_vid", "" },
	{ "usb_path3_pid", "" },
	{ "usb_path3_manufacturer", "" },
	{ "usb_path3_product", "" },
	{ "usb_path3_serial", "" },
	{ "usb_path3_speed", "" },
	{ "usb_path3_removed", "0" },
#ifdef RTCONFIG_DISK_MONITOR
	{ "usb_path3_pool_error", "0" },
#endif
	{ "usb_path3_fs_path0", "" },
#if 0
	{ "usb_path3_fs_path1", "" },
	{ "usb_path3_fs_path2", "" },
	{ "usb_path3_fs_path3", "" },
	{ "usb_path3_fs_path4", "" },
	{ "usb_path3_fs_path5", "" },
	{ "usb_path3_fs_path6", "" },
	{ "usb_path3_fs_path7", "" },
	{ "usb_path3_fs_path8", "" },
	{ "usb_path3_fs_path9", "" },
	{ "usb_path3_fs_path10", "" },
	{ "usb_path3_fs_path11", "" },
	{ "usb_path3_fs_path12", "" },
	{ "usb_path3_fs_path13", "" },
	{ "usb_path3_fs_path14", "" },
	{ "usb_path3_fs_path15", "" },
	{ "usb_path3_label0", "" },
	{ "usb_path3_label1", "" },
	{ "usb_path3_label2", "" },
	{ "usb_path3_label3", "" },
	{ "usb_path3_label4", "" },
	{ "usb_path3_label5", "" },
	{ "usb_path3_label6", "" },
	{ "usb_path3_label7", "" },
	{ "usb_path3_label8", "" },
	{ "usb_path3_label9", "" },
	{ "usb_path3_label10", "" },
	{ "usb_path3_label11", "" },
	{ "usb_path3_label12", "" },
	{ "usb_path3_label13", "" },
	{ "usb_path3_label14", "" },
	{ "usb_path3_label15", "" },
#endif
#endif

	{ "apps_dev", "" },
	{ "apps_mounted_path", "" },
	{ "apps_state_action", "" },
	{ "apps_state_autorun", "" },
	{ "apps_state_install", "" },
	{ "apps_state_remove", "" },
	{ "apps_state_switch", "" },
	{ "apps_state_stop", "" },
	{ "apps_state_enable", "" },
	{ "apps_state_update", "" },
	{ "apps_state_upgrade", "" },
	{ "apps_state_error", "" },
	{ "apps_download_file", "" },
	{ "apps_download_percent", "" },
	{ "apps_depend_do", "" },
	{ "apps_depend_action", "" },
	{ "apps_depend_action_target", "" },

#ifdef RTCONFIG_DISK_MONITOR
	{ "diskmon_status", "" },
#endif

	{ "webs_state_update", "" },
	{ "webs_state_upgrade", "" },
	{ "webs_state_error", "" },
	{ "webs_state_info", "" },

	{ "ftp_ports", ""},

#ifdef RTCONFIG_DSL
// name starting with 'dsl' are reserved for dsl unit
// for temp variable please use dsltmp_xxx
	{ "dsltmp_autodet_state", ""},
	{ "dsltmp_autodet_vpi", "0"},	
	{ "dsltmp_autodet_vci", "35"},
	{ "dsltmp_autodet_encap", "0"},			
// manually config	
	{ "dsltmp_cfg_prctl", "0"},		
	{ "dsltmp_cfg_vpi", "0"},	
	{ "dsltmp_cfg_vci", "35"},		
	{ "dsltmp_cfg_encap", "0"},			
	{ "dsltmp_cfg_iptv_idx", ""},
	{ "dsltmp_cfg_iptv_num_pvc", "0"},	
	{ "dsltmp_cfg_ispname", ""},
	{ "dsltmp_cfg_country", ""},
// tmp variable for QIS , it will write to dsl0_xxx after finish page	
	{ "dsltmp_qis_vpi", ""},
	{ "dsltmp_qis_vci", ""},
	{ "dsltmp_qis_proto", ""},
	{ "dsltmp_qis_encap", ""},
	{ "dsltmp_qis_pppoe_username", ""},
	{ "dsltmp_qis_pppoe_passwd", ""},			
	{ "dsltmp_qis_pppoe_dial_on_demand", ""},
	{ "dsltmp_qis_pppoe_idletime", ""},
	{ "dsltmp_qis_pppoe_mtu", ""},
	{ "dsltmp_qis_pppoe_mru", ""},
	{ "dsltmp_qis_pppoe_service", ""},
	{ "dsltmp_qis_pppoe_options", ""},
	{ "dsltmp_qis_DHCPClient", ""},
	{ "dsltmp_qis_ipaddr", ""},
	{ "dsltmp_qis_netmask", ""},
	{ "dsltmp_qis_gateway", ""},	
	{ "dsltmp_qis_dnsenable", ""},
	{ "dsltmp_qis_dns1", ""},
	{ "dsltmp_qis_dns2", ""},	
	{ "dsltmp_qis_svc_cat", ""},
	{ "dsltmp_qis_pcr", ""},	
	{ "dsltmp_qis_scr", ""},		
	{ "dsltmp_qis_mbs", ""},			
	{ "dsltmp_qis_pppoe_relay", ""},			
	{ "dsltmp_qis_hwaddr", ""},				
	{ "dsltmp_qis_admin_passwd", ""},					
	{ "dsltmp_qis_admin_passwd_set", "0"},
	{ "dsltmp_qis_dsl_pvc_set", "0"},	
// for DSL driver and tool
	{ "dsltmp_tc_resp_to_d", ""},			
	{ "dsltmp_adslatequit", "0"},
	{ "dsltmp_tcbootup", "0"},	
	{ "dsltmp_adslsyncsts", ""},	
	{ "dsltmp_adslsyncsts_detail", ""},	
// for web ui identify , 1=old ui, 2=asuswrt
	{ "dsltmp_web_ui_ver", "2"},	
#endif
	{ "ddns_cache", ""},
	{ "ddns_ipaddr", ""},
	{ "ddns_status", ""},
	{ "ddns_server_x_old", ""},
	{ "ddns_hostname_old", ""},
	{ "ddns_return_code", ""},
	{ "ddns_return_code_chk", ""},
	{ "ddns_update_by_wdog", ""},
	{ "reboot_time", "70"},	
	{ NULL, NULL }
};

#ifdef REMOVE
const defaults_t if_generic[] = {
	{ "lan_ifname",		"br0"			},
	{ "lan_ifnames",	"eth0 eth2 eth3 eth4"	},
	{ "wan_ifname",		"eth1"			},
	{ "wan_ifnames",	"eth1"			},
	{ NULL, NULL }
};

const defaults_t if_vlan[] = {
	{ "lan_ifname",		"br0"			},
	{ "lan_ifnames",	"vlan0 eth1 eth2 eth3"	},
	{ "wan_ifname",		"vlan1"			},
	{ "wan_ifnames",	"vlan1"			},
	{ NULL, NULL }
};
#endif

#ifdef RTCONFIG_BCMWL6
#ifndef RTCONFIG_BCMARM
struct nvram_tuple bcm4360ac_defaults[] = {
	{ "pci/2/1/aa2g", "0", 0 },
	{ "pci/2/1/aa5g", "7", 0 },
	{ "pci/2/1/aga0", "0", 0 },
	{ "pci/2/1/aga1", "0", 0 },
	{ "pci/2/1/aga2", "0", 0 },
	{ "pci/2/1/agbg0", "133", 0 },
	{ "pci/2/1/agbg1", "133", 0 },
	{ "pci/2/1/agbg2", "133", 0 },
	{ "pci/2/1/antswitch", "0", 0 },
	{ "pci/2/1/cckbw202gpo", "0", 0 },
	{ "pci/2/1/cckbw20ul2gpo", "0", 0 },
	{ "pci/2/1/dot11agofdmhrbw202gpo", "0", 0 },
	{ "pci/2/1/femctrl", "3", 0 },
	{ "pci/2/1/papdcap2g", "0", 0 },
	{ "pci/2/1/tworangetssi2g", "0", 0 },
	{ "pci/2/1/pdgain2g", "4", 0 },
	{ "pci/2/1/epagain2g", "0", 0 },
	{ "pci/2/1/tssiposslope2g", "1", 0 },
	{ "pci/2/1/gainctrlsph", "0", 0 },
	{ "pci/2/1/papdcap5g", "0", 0 },
	{ "pci/2/1/tworangetssi5g", "0", 0 },
	{ "pci/2/1/pdgain5g", "4", 0 },
	{ "pci/2/1/epagain5g", "0", 0 },
	{ "pci/2/1/tssiposslope5g", "1", 0 },
	{ "pci/2/1/maxp2ga0", "76", 0 },
	{ "pci/2/1/maxp2ga1", "76", 0 },
	{ "pci/2/1/maxp2ga2", "76", 0 },
	{ "pci/2/1/mcsbw202gpo", "0", 0 },
	{ "pci/2/1/mcsbw402gpo", "0", 0 },
	{ "pci/2/1/measpower", "0x7f", 0 },
	{ "pci/2/1/measpower1", "0x7f", 0 },
	{ "pci/2/1/measpower2", "0x7f", 0 },
	{ "pci/2/1/noiselvl2ga0", "31", 0 },
	{ "pci/2/1/noiselvl2ga1", "31", 0 },
	{ "pci/2/1/noiselvl2ga2", "31", 0 },
	{ "pci/2/1/noiselvl5gha0", "31", 0 },
	{ "pci/2/1/noiselvl5gha1", "31", 0 },
	{ "pci/2/1/noiselvl5gha2", "31", 0 },
	{ "pci/2/1/noiselvl5gla0", "31", 0 },
	{ "pci/2/1/noiselvl5gla1", "31", 0 },
	{ "pci/2/1/noiselvl5gla2", "31", 0 },
	{ "pci/2/1/noiselvl5gma0", "31", 0 },
	{ "pci/2/1/noiselvl5gma1", "31", 0 },
	{ "pci/2/1/noiselvl5gma2", "31", 0 },
	{ "pci/2/1/noiselvl5gua0", "31", 0 },
	{ "pci/2/1/noiselvl5gua1", "31", 0 },
	{ "pci/2/1/noiselvl5gua2", "31", 0 },
	{ "pci/2/1/ofdmlrbw202gpo", "0", 0 },
	{ "pci/2/1/pa2ga0", "0xfe72,0x14c0,0xfac7", 0 },
	{ "pci/2/1/pa2ga1", "0xfe80,0x1472,0xfabc", 0 },
	{ "pci/2/1/pa2ga2", "0xfe82,0x14bf,0xfad9", 0 },
	{ "pci/2/1/pcieingress_war", "15", 0 },
	{ "pci/2/1/phycal_tempdelta", "255", 0 },
	{ "pci/2/1/rawtempsense", "0x1ff", 0 },
	{ "pci/2/1/rxchain", "7", 0 },
	{ "pci/2/1/rxgainerr2g", "0xffff", 0 },
	{ "pci/2/1/rxgainerr5g", "0xffff,0xffff,0xffff,0xffff", 0 },
	{ "pci/2/1/rxgains2gelnagaina0", "0", 0 },
	{ "pci/2/1/rxgains2gelnagaina1", "0", 0 },
	{ "pci/2/1/rxgains2gelnagaina2", "0", 0 },
	{ "pci/2/1/rxgains2gtrelnabypa0", "0", 0 },
	{ "pci/2/1/rxgains2gtrelnabypa1", "0", 0 },
	{ "pci/2/1/rxgains2gtrelnabypa2", "0", 0 },
	{ "pci/2/1/rxgains2gtrisoa0", "0", 0 },
	{ "pci/2/1/rxgains2gtrisoa1", "0", 0 },
	{ "pci/2/1/rxgains2gtrisoa2", "0", 0 },
	{ "pci/2/1/sar2g", "18", 0 },
	{ "pci/2/1/sar5g", "15", 0 },
	{ "pci/2/1/sromrev", "11", 0 },
	{ "pci/2/1/subband5gver", "0x4", 0 },
	{ "pci/2/1/tempcorrx", "0x3f", 0 },
	{ "pci/2/1/tempoffset", "255", 0 },
	{ "pci/2/1/temps_hysteresis", "15", 0 },
	{ "pci/2/1/temps_period", "15", 0 },
	{ "pci/2/1/tempsense_option", "0x3", 0 },
	{ "pci/2/1/tempsense_slope", "0xff", 0 },
	{ "pci/2/1/tempthresh", "255", 0 },
	{ "pci/2/1/txchain", "7", 0 },
	{ "pci/2/1/ledbh0", "2", 0 },
	{ "pci/2/1/ledbh1", "5", 0 },
	{ "pci/2/1/ledbh2", "4", 0 },
	{ "pci/2/1/ledbh3", "11", 0 },
	{ "pci/2/1/ledbh10", "7", 0 },

	{ 0, 0, 0 }
};
#else
struct nvram_tuple bcm4360ac_defaults[] = {
	{ "0:ledbh10", "7", 0 },
	{ "1:ledbh10", "7", 0 },
	{ 0, 0, 0 }
};
#endif
#endif

/* nvram override default setting for Media Router */
struct nvram_tuple router_defaults_override_type1[] = {
	{ "router_disable", "1", 0 },		/* lan_proto=static lan_stp=0 wan_proto=disabled */
	{ "lan_stp", "0", 0 },			/* LAN spanning tree protocol */
	{ "wl_wmf_bss_enable", "1", 0 },	/* WMF Enable for IPTV Media or WiFi+PLC */
	{ "wl_reg_mode", "h", 0 },		/* Regulatory: 802.11H(h) */
	{ "wl_wet_tunnel", "1", 0  },		/* Enable wet tunnel */
	{ "wl_taf_enable", "1", 0  },		/* Enable TAF */

	/* EBOS feature Media router default */
	{ "wl_ebos_enable", "0", 0 },		/* EBOS feature on */
	{ "wl_ebos_flags", "104", 0 },		/* 104(0x68) pseudo-round robin */
	{ "wl_ebos_prr_threshold", "0x0f000000", 0 },	/* pseudo-round robin threshold */

#ifdef __CONFIG_EMF__
	{ "emf_enable", "1", 0 },		/* Enable EMF by default */
	{ "wl_wmf_ucigmp_query", "1", 0 },	/* Enable Converting IGMP Query to ucast */
	{ "wl_wmf_ucast_upnp", "1", 0 },	/* Enable upnp to ucast conversion */
	{ "wl_wmf_igmpq_filter", "1", 0 },	/* Enable igmp query filter */
#endif
	{ "wl_acs_fcs_mode", "1", 0 },		/* Enable acsd fcs mode */
	{ "wl_acs_dfs", "1", 0 },		/* Enable first DFS chan Selection */
	{ "wl_dcs_csa_unicast", "1", 0 },	/* Enable unicast CSA */
	/* Exclude ACSD to select 140l, 144u, 140/80, 144/80 to compatible with Ducati 11N */
	{ "wl_acs_excl_chans", "0xd98e,0xd88e,0xe28a,0xe38a", 0 },
	{ "wl_pspretend_retry_limit", "5", 0 }, /* Enable PsPretend */
	{ "wl_pspretend_threshold", "0", 0 },	/* Disable PsPretend Threshold */
	{ "wl_acs_chan_dwell_time", "70", 0 },	/* WAR for AP to stay on DFS chan */
	{ "wl_frameburst", "on", 0 },		/* BRCM Frambursting mode (off|on) */
	{ "wl_amsdu", "off", 0 },		/* Default IPTV AMSDU setting */
	{ "wl_rx_amsdu_in_ampdu", "off", 0 },	/* Media RX AMSDU In AMPDU setting */
	{ 0, 0, 0 }
};

/* Translates from, for example, wl0_ (or wl0.1_) to wl_. */
/* Only single digits are currently supported */

static void
fix_name(const char *name, char *fixed_name)
{
	char *pSuffix = NULL;

	/* Translate prefix wlx_ and wlx.y_ to wl_ */
	/* Expected inputs are: wld_root, wld.d_root, wld.dd_root
	 * We accept: wld + '_' anywhere
	 */
	pSuffix = strchr(name, '_');

	if ((strncmp(name, "wl", 2) == 0) && isdigit(name[2]) && (pSuffix != NULL)) {
		strcpy(fixed_name, "wl");
		strcpy(&fixed_name[2], pSuffix);
		return;
	}

	/* No match with above rules: default to input name */
	strcpy(fixed_name, name);
}


/*
 * Find nvram param name; return pointer which should be treated as const
 * return NULL if not found.
 *
 * NOTE:  This routine special-cases the variable wl_bss_enabled.  It will
 * return the normal default value if asked for wl_ or wl0_.  But it will
 * return 0 if asked for a virtual BSS reference like wl0.1_.
 */
char *
nvram_default_get(const char *name)
{
	int idx;
	char fixed_name[NVRAM_MAX_VALUE_LEN];

	fix_name(name, fixed_name);
	if (strcmp(fixed_name, "wl_bss_enabled") == 0) {
		if (name[3] == '.' || name[4] == '.') { /* Virtual interface */
			return "0";
		}
	}

#ifdef __CONFIG_HSPOT__
	if (strcmp(fixed_name, "wl_bss_hs2_enabled") == 0) {
		if (name[3] == '.' || name[4] == '.') { /* Virtual interface */
			return "0";
		}
	}
#endif  /* __CONFIG_HSPOT__ */

	if (!strcmp(nvram_safe_get("devicemode"), "1")) {
		for (idx = 0; router_defaults_override_type1[idx].name != NULL; idx++) {
			if (strcmp(router_defaults_override_type1[idx].name, fixed_name) == 0) {
				return router_defaults_override_type1[idx].value;
			}
		}
	}

	for (idx = 0; router_defaults[idx].name != NULL; idx++) {
		if (strcmp(router_defaults[idx].name, fixed_name) == 0) {
			return router_defaults[idx].value;
		}
	}

	return NULL;
}
/* validate/restore all per-interface related variables */
void
nvram_validate_all(char *prefix, bool restore)
{
	struct nvram_tuple *t;
	char tmp[100];
	char *v;

	for (t = router_defaults; t->name; t++) {
		if (!strncmp(t->name, "wl_", 3)) {
			strcat_r(prefix, &t->name[3], tmp);
			if (!restore && nvram_get(tmp))
				continue;
			v = nvram_get(t->name);
			nvram_set(tmp, v ? v : t->value);
		}
	}

	/* override router type1 nvram setting */
	if (!strcmp(nvram_safe_get("devicemode"), "1")) {
		for (t = router_defaults_override_type1; t->name; t++) {
			if (!strncmp(t->name, "wl_", 3)) {
				strcat_r(prefix, &t->name[3], tmp);
				if (!restore && nvram_get(tmp))
					continue;
				v = nvram_get(t->name);
				nvram_set(tmp, v ? v : t->value);
			}
		}
	}
}

/* restore specific per-interface variable */
void
nvram_restore_var(char *prefix, char *name)
{
	struct nvram_tuple *t;
	char tmp[100];

	for (t = router_defaults; t->name; t++) {
		if (!strncmp(t->name, "wl_", 3) && !strcmp(&t->name[3], name)) {
			nvram_set(strcat_r(prefix, name, tmp), t->value);
			break;
		}
	}

	/* override router type1 setting */
	if (!strcmp(nvram_safe_get("devicemode"), "1")) {
		for (t = router_defaults_override_type1; t->name; t++) {
			if (!strncmp(t->name, "wl_", 3) && !strcmp(&t->name[3], name)) {
				nvram_set(strcat_r(prefix, name, tmp), t->value);
				break;
			}
		}
	}
}
