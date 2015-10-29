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
#ifdef RPAC68U
	{ "sw_mode", "2" 		}, 	// big switch for different mode
#elif defined(RTCONFIG_DEFAULT_AP_MODE)
	{ "sw_mode", "3"		},	// big switch for different mode
#else
	{ "sw_mode", "1" 		}, 	// big switch for different mode
#endif
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

	//Use to do force submit form request
	{ "force_change", "0"},

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
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
	{ "wl_version", EPI_VERSION_STR },	/* OS revision */
#endif
#ifdef RTCONFIG_WIRELESS_SWITCH
	{ "wl_HW_switch", "0" },		/* siwtch WiFi slash*/
#endif
	{ "wl_ifname", "", 0 },			/* Interface name */
	{ "wl_hwaddr", "", 0 },			/* MAC address */
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
	{ "wl_phytype", "b", 0 },		/* Current wireless band ("a" (5 GHz),
 						 * "b" (2.4 GHz), or "g" (2.4 GHz))
						 */
	{ "wl_corerev", "", 0 },		/* Current core revision */
	{ "wl_phytypes", "", 0 },		/* List of supported wireless bands (e.g. "ga") */
	{ "wl_radioids", "", 0 },		/* List of radio IDs */
#endif
	{ "wl_ssid", "ASUS", 0 },		/* Service set ID (network name) */
#if !defined (RTAC3200) && !defined (RTAC5300)
	{ "wl1_ssid", "ASUS_5G" },
#else
	{ "wl1_ssid", "ASUS_5G-1" },
	{ "wl2_ssid", "ASUS_5G-2" },
#endif
	{ "wl_bss_enabled", "1", 0 },		/* Service set Enable (1) or disable (0) radio */
						/* See "default_get" below. */
#ifdef __CONFIG_HSPOT__
	{ "wl_hsflag",		"3159", 0 },	/* Passpoint Flags */
	{ "wl_hs2en",		"0", 0 },	/* Passpoint Enable (1), disable (0) radio */
	{ "wl_hs2cap",		"1", 0 },	/* Passpoint Realese 2 (1), Realese 1 (0) radio */
	{ "wl_opercls",		"3", 0 },	/* Operating Class */
	{ "wl_anonai",		"anonymous.com", 0 },	/* Anonymous NAI */

	{ "wl_oplist",		"T-Mobile!eng", 0 },	/* Operator Friendly Name List */

	{ "wl_homeqlist",	"t-mobile.com:rfc4282", 0 },/* NAIHomeRealmQueryList */
	{ "wl_wanmetrics",	"1:0:0=2500>384=0>0=0", 0 },/* WAN Metrics */
	{ "wl_osu_ssid",	"CellSpot_AutoConnect", 0}, /* OSU SSID */

	{ "wl_osu_frndname",	"T-Mobile!eng", 0},	/* OSU Friendly Name */

	{ "wl_osu_uri",
	"https://osu-server.t-mobile.com/", 0},	/* OSU Server URI */

	{ "wl_osu_nai",		"", 0},		/* OSU NAI */
	{ "wl_osu_method",	"1", 0},	/* OSU Method */

	{ "wl_osu_icons",
	"icon_red_zxx.png+icon_red_eng.png", 0},/* OSU Icons */

	{ "wl_osu_servdesc", "T-Mobile Passpoint service!eng", 0}, /* OSU Serv Desc */

	/* ---- Passpoint Flag ----------------------------------- */
	{ "wl_gascbdel",	"0", 0 },	/* GAS CB Delay */
	{ "wl_4framegas",	"0", 0 },	/* 4 Frame GAS */

	/* ---- temporary ----------------------------------- */
	{ "wl_osuicon_id",	"1", 0 },	/* OSU Provider's Icon ID */
	{ "wl_conn_id",		"1", 0 },	/* Connection Capability ID */

//#ifdef __CONFIG_802_11U__
	{ "wl_u11en",		"0", 0 },	/* 802.11u IW Enable (1), disable (0) radio */
	{ "wl_iwint",		"0", 0 },	/* Internet Enable (1), disable (0) radio */
	{ "wl_iwnettype",	"2", 0 },	/* Select Access Network Type */
	{ "wl_hessid",		"50:6F:9A:00:11:22", 0 },	/* Interworking HESSID */
	{ "wl_ipv4addr",	"3", 0 },	/* Select IPV4 Address Type Availability */
	{ "wl_ipv6addr",	"0", 0 },	/* Select IPV6 Address Type Availability */

	{ "wl_netauthlist", "accepttc=+"
	"httpred=https://t-mobile.com", 0 },	/* Network Authentication Type List */

	{ "wl_venuegrp",	"2", 0 },	/* Venue Group */
	{ "wl_venuetype",	"8", 0 },	/* Venue Type */

	{ "wl_venuelist",
	"542D4D6F62696C6521656E67", 0 },	/* Venue Name List */

	{ "wl_ouilist",		"506F9A;001BC504BD", 0 },	/* Roaming Consortium List */
	{ "wl_3gpplist",	"310:260;310:310", 0 },		/* 3GPP Cellular Network Information List */
	{ "wl_domainlist",	"t-mobile.com", 0 },		/* Domain Name List */

	{ "wl_realmlist",
	"wlan.mnc260.mcc310.3gppnetwork.org+0+21=2,4#5,7?"
	"wlan.mnc260.mcc310.3gppnetwork.org+0+13=5,6?"
	"wlan.mnc310.mcc310.3gppnetwork.org+0+21=2,4#5,7?"
	"wlan.mnc310.mcc310.3gppnetwork.org+0+13=5,6" , 0 },	/* NAI Realm List */
//#endif /* __CONFIG_802_11U__ */

#endif /* __CONFIG_HSPOT__ */

//	{ "wl_country_code", "", 0 },		/* Country Code (default obtained from driver) */
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
	{ "wl_country_rev", "", 0 },		/* Regrev Code (default obtained from driver) */
#endif
	{ "wl_radio", "1", 0 },			/* Enable (1) or disable (0) radio */
	{ "wl_closed", "0", 0 },		/* Closed (hidden) network */
	{ "wl_ap_isolate", "0", 0 },		/* AP isolate mode */
	{ "wl_igs", "0" },			/* BCM: wl_wmf_bss_enable
						 * Ralink: IGMPSnEnable */

#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
	{ "wl_wmf_bss_enable", "0", 0 },	/* WMF Enable/Disable */
	{ "wl_mcast_regen_bss_enable", "1", 0 },/* MCAST REGEN Enable/Disable */

	/* operational capabilities required for stations to associate to the BSS */
#ifdef RTCONFIG_BCMWL6
	{ "wl_bss_opmode_cap_reqd", "0", 0 },
#endif
	{ "wl_rxchain_pwrsave_enable", "1", 0 },/* Rxchain powersave enable */
	{ "wl_rxchain_pwrsave_quiet_time", "1800", 0 },	/* Quiet time for power save */
	{ "wl_rxchain_pwrsave_pps", "10", 0 },	/* Packets per second threshold for power save */
	{ "wl_rxchain_pwrsave_stas_assoc_check", "0", 0 }, /* STAs associated before powersave */
	{ "wl_radio_pwrsave_enable", "0", 0 },	/* Radio powersave enable */
	{ "wl_radio_pwrsave_quiet_time", "1800", 0 },	/* Quiet time for power save */
	{ "wl_radio_pwrsave_pps", "10", 0 },	/* Packets per second threshold for power save */
	{ "wl_radio_pwrsave_level", "0", 0 },	/* Radio power save level */
	{ "wl_radio_pwrsave_stas_assoc_check", "0", 0 }, /* STAs associated before powersave */
#endif
	{ "wl_mode", "ap", 0 },			/* AP mode (ap|sta|wds) */
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
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
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
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
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
	{ "wl_noisemitigation", "0"},
	{ "wl_reg_mode", "off", 0 },		/* Regulatory: 802.11H(h)/802.11D(d)/off(off) */
	{ "wl_rate", "0", 0 },			/* Rate (bps, 0 for auto) */
#if 0
	{ "wl_mrate", "0", 0 },			/* Mcast Rate (bps, 0 for auto) */
#endif
#endif
	{ "wl_mrate_x", "0" },			/* ralink auto rate */
#ifndef RTCONFIG_WIFILOGO
	{ "wl_frameburst", "on", 0 },		/* BRCM Frambursting mode (off|on) */
#else
	{ "wl_frameburst", "off", 0 },
#endif
#if defined (RTCONFIG_BCMARM) && !defined (RTCONFIG_BCM7)
	{ "frameburst_dyn", "0", 0 },		/* Frameburst controlled dynamically if on */
#endif
	{ "wl_rateset", "default", 0 },		/* "default" or "all" or "12" */
	{ "wl_frag", "2346", 0 },		/* Fragmentation threshold */
	{ "wl_rts", "2347", 0 },		/* RTS threshold */
#ifdef RTCONFIG_RALINK
	{ "wl_dtim", "1"},
#elif defined(RTCONFIG_QCA)
	{ "wl_dtim", "1"},
#else
	{ "wl_dtim", "3", 0 },			/* DTIM period */
#endif
	{ "wl_bcn", "100", 0 },			/* Beacon interval */
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
	{ "wl_bcn_rotate", "1", 0 },		/* Beacon rotation */
#endif
	{ "wl_plcphdr", "long", 0 },		/* 802.11b PLCP preamble type */
#ifdef RTCONFIG_RALINK
	{ "wl_nmode_protection", "auto", 0},	/* 802.11n protection */
#elif defined(RTCONFIG_QCA) || defined(RTCONFIG_BCM_7114)
	{ "wl_nmode_protection", "auto", 0},	/* 802.11n protection */
#else
	{ "wl_gmode", XSTR(GMODE_AUTO), 0 },	/* 54g mode */
#endif
	{ "wl_gmode_protection", "auto", 0 },	/* 802.11g RTS/CTS protection (off|auto) */
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
	{ "wl_optimizexbox", "0"},		/* Optimize WiFi packet for Xbox */
#endif
	{ "wl_wme", "auto", 0 },		/* WME mode (off|on|auto) */
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
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
#ifdef RTCONFIG_TCODE
	{"location_code", ""},			/*0: real channel; 1: Auto; for RT-N12HP & RT-N14UHP*/
#endif
#ifdef RTCONFIG_RALINK
	{ "wl_nband", "2", 0},			/* N-BAND */
	{ "wl0_nband", "2"},			/* 2.4 GHz */
	{ "wl1_nband", "1"},			/* 5 GHz */
#elif defined(RTCONFIG_QCA)
	{ "wl_nband", "2", 0},			/* N-BAND */
	{ "wl0_nband", "2"},			/* 2.4 GHz */
	{ "wl1_nband", "1"},			/* 5 GHz */
#else
	{ "wl_nband", "2", 0},			/* N-BAND */
#if defined (RTAC3200) || defined (RTAC5300)
	{ "wl1_nband", "1"},
	{ "wl2_nband", "1"},
#else
	{ "wl1_nband", "1"},			/* 5 GHz */
#endif

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
#endif	/* RTCONFIG_RALINK */
#ifdef RTCONFIG_RALINK
	{ "wl_mimo_preamble", "mm"},
#elif defined(RTCONFIG_QCA)
	{ "wl_mimo_preamble", "mm"},
#else
//	{ "wl_mimo_preamble", "gfbcm"},		/* Mimo PrEamble: mm/gf/auto/gfbcm */
#endif
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
#if defined(RTCONFIG_BCM_7114)
	{ "wl_ampdu_rtylimit_tid", "5 5 5 5 5 5 5 5", 0 },
	{ "wl_ampdu_rr_rtylimit_tid", "2 2 2 2 2 2 2 2", 0 },
#else
	/* Default AMPDU retry limit per-tid setting */
	{ "wl_ampdu_rtylimit_tid", "7 7 7 7 7 7 7 7", 0 },
	/* Default AMPDU regular rate retry limit per-tid setting */
	{ "wl_ampdu_rr_rtylimit_tid", "3 3 3 3 3 3 3 3", 0 },
#endif
	{ "wl_amsdu", "auto", 0 },		/* Default AMSDU setting */
#if defined (RTCONFIG_BCMARM) && !defined (RTCONFIG_BCM7) && !defined (RTCONFIG_BCM_7114)
	{ "wl_rx_amsdu_in_ampdu", "auto", 0 },	/* Default RX AMSDU In AMPDU setting */
#endif
	{ "wl_obss_coex", "1", 0 },		/* Default OBSS Coexistence setting - OFF */
#ifdef RTCONFIG_BCMWL6
#if !defined (RTCONFIG_BCM7) && !defined (RTCONFIG_BCM_7114)
	{ "wl_ack_ratio", "0"},
#endif
	{ "wl_ampdu_mpdu", "0"},
	{ "wl_ampdu_rts", "1"},
#ifdef RTCONFIG_BCMARM
#ifdef RTCONFIG_BCM_7114
	{ "wl_turbo_qam", "2"},
#else
	{ "wl_turbo_qam", "1"},
#endif
#ifdef RTCONFIG_BCM_7114
	{ "wl0_turbo_qam", "2"},
#else
	{ "wl0_turbo_qam", "1"},
#endif
#ifdef RTCONFIG_BCM_7114
	{ "wl1_turbo_qam", "2"},
	{ "wl2_turbo_qam", "2"},
#endif	// BCM_7114
#endif	// BCMARM
#endif	// BCMWL6
#endif	// RALINK
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
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
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
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
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
#ifdef RTCONFIG_WPS_LED
	{ "wps_success", "0", 0 },	/* after 3 seconds under WPS success, watchdog clear to 0 */
#endif
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
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
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
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
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
#ifdef RTCONFIG_QTN
	{ "wl1_bw",			"3"	},	/* 0/1/2/3 auto/20/40/80MHz */
#endif
#else
	{ "wl_bw",			"1"	},	/* 0/1/2 20, 20/40, 40MHz */
#endif
	{ "wl_auth_mode_x", 		"open"	},
// open/shared/psk/wpa/radius
	{ "wl_wep_x", 			"0"	},
// WEP data encryption 0, 1, 2 : disabled/5/13
	{ "wl_timesched", 		"0"	},
	{ "wl_radio_date_x",	"1111111"	},
	{ "wl_radio_time_x",	"00002359"	},
	{ "wl_radio_time2_x",	"00002359"	},
	{ "wl_sched",		"000000"	},
	{ "wl_phrase_x",	""		},	// Passphrase	// Add
	{ "wl_lanaccess", 	"off"		},
	{ "wl_expire", 		"0"		},
	{ "wl_mbss",		""		},
	{ "wl_txpower",		"100"		},

#if defined (RTCONFIG_RALINK) || defined (RTCONFIG_BCMWL6)
#if defined(RTAC53U) || defined(RTCONFIG_QTN)
	/* RT-AC53U disable txbf by default */
	{ "wl_txbf", "0" },
#else
	{ "wl_txbf", "1" },
#ifdef RTCONFIG_QTN
	{ "wl1_80211h", "0" },
#endif
#endif
#endif
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_BCMARM
#ifdef RTCONFIG_QTN
	{ "wl_itxbf", "0" },
#else
	{ "wl_itxbf", "1" },
#endif
#endif
#endif

#if defined (RTCONFIG_WIRELESSREPEATER) || defined (RTCONFIG_PROXYSTA)
#if defined (RTCONFIG_CONCURRENTREPEATER)
	/* 2.4GHz */
	{ "wlc0_list",			""	},
	{ "wlc0_ssid",			""	},
	{ "wlc0_wep",			""	},
	{ "wlc0_key",			""	},
	{ "wlc0_wep_key",		""	},
	{ "wlc0_auth_mode",		""	},
	{ "wlc0_crypto",		""	},
	{ "wlc0_wpa_psk",		""	},
	{ "wlc0_state",			"0"	},
	{ "wlc0_sbstate",		"0"	},
	{ "wlc0_scan_state",		"0"	},
	{ "wlc0_mode",			"0"	},
	{ "wlc0_ure_ssid",		""	},
	/* 5GHz */
	{ "wlc1_list",			""	},
	{ "wlc1_ssid",			""	},
	{ "wlc1_wep",			""	},
	{ "wlc1_key",			""	},
	{ "wlc1_wep_key",		""	},
	{ "wlc1_auth_mode",		""	},
	{ "wlc1_crypto",		""	},
	{ "wlc1_wpa_psk",		""	},
	{ "wlc1_state", 		"0"	},
	{ "wlc1_sbstate", 		"0" 	},
	{ "wlc1_scan_state", "0"},
	{ "wlc1_mode",			"0"	},
	{ "wlc1_ure_ssid",		""	},
#ifndef RTCONFIG_BCMWL6
	{ "wlc0_nbw_cap",		""	},
	{ "wlc1_nbw_cap",		""	},
#else
	{ "wlc0_bw_cap",		""	},
	{ "wlc1_bw_cap",		""	},
#endif
#else
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
	{ "wlc_express",		"0"	},	/* 0: disabled, 1: 2.4GHz, 2: 5GHz */
#endif
#ifdef RTCONFIG_PROXYSTA
	{ "wlc_psta",			"0"	},	/* 0: disabled, 1: Proxy STA, 2: Proxy STA Repeater */
	{ "wlc_band_ex",		""	},	/* another psta band */
#ifdef PXYSTA_DUALBAND
	{ "exband",			"0"	},	/* 0:disabled, 1:double bands */
#endif
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
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
	{ "emf_entry", "", 0 },			/* Static MFDB entry (mgrp:if) */
	{ "emf_uffp_entry", "", 0 },		/* Unreg frames forwarding ports */
	{ "emf_rtport_entry", "", 0 },		/* IGMP frames forwarding ports */
#ifdef RTCONFIG_BCMWL6
	{ "wl_wmf_ucigmp_query", "0", 0 },	/* Disable Converting IGMP Query to ucast (default) */
	{ "wl_wmf_mdata_sendup", "0", 0 },	/* Disable Sending Multicast Data to host (default) */
#ifdef RTCONFIG_BCMARM
	{ "wl_wmf_ucast_upnp", "0", 0 },	/* Disable Converting upnp to ucast (default) */
	{ "wl_wmf_igmpq_filter", "0", 0 },	/* Disable igmp query filter */
#endif
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
#if defined (RTAC68U) || defined (RTAC66U) || defined (RTN66U) || defined (RTCONFIG_QTN) || defined (DSL_AC68U)
	{ "acs_dfs", "0", 0},			/* disable DFS channels for acsd by default */
#endif
	{ "acs_band1", "0", 0},

	{ "wl_wet_tunnel", "0", 0 },		/* Disable wet tunnel */

	{ "dpsta_ifnames", "", 0 },
	{ "dpsta_policy", "1", 0 },
	{ "dpsta_lan_uif", "1", 0 },
#ifdef TRAFFIC_MGMT_RSSI_POLICY
	{ "wl_trf_mgmt_rssi_policy", "0", 0 },	/* Disable RSSI (default) */
#endif /* TRAFFIC_MGMT */
#ifdef RTCONFIG_BCMARM
#ifndef RTCONFIG_WIFILOGO
	{ "wl_atf", "1", 0 }, 			/* Airtime Fairness */
#else
	{ "wl_atf", "0", 0 },
#endif
#endif

	/* Tx Beamforming */
	{ "wl_txbf_bfr_cap", "1", 0 },
	{ "wl_txbf_bfe_cap", "1", 0 },
#ifdef RTCONFIG_BCMARM
#ifndef RTCONFIG_WIFILOGO
	{ "wl_txbf_imp", "1", 0 },
#else
	{ "wl_txbf_imp", "0", 0 },

#endif
#else
	{ "wl_txbf_timer", "25", 0 },
#endif

	/* acsd setting */
	{ "wl_acs_fcs_mode", "0", 0 },		/* acsd disable FCS mode */
	{ "wl_dcs_csa_unicast", "0", 0 },	/* disable unicast csa */
	{ "wl_acs_excl_chans", "", 0 },		/* acsd exclude chanspec list */
#ifdef RTCONFIG_BCM7
	{ "wl_acs_dfs", "2", 0 },		/* acsd fcs disable init DFS chan */
#else
	{ "wl_acs_dfs", "0", 0 },		/* acsd fcs disable init DFS chan */
#endif
	{ "wl_acs_dfsr_immediate", "300 3", 0 },/* immediate if > 3 switches last 5 minutes */
	{ "wl_acs_dfsr_deferred", "604800 5", 0 }, /* deferred if > 5 switches in last 7 days */
	{ "wl_acs_dfsr_activity", "30 10240", 0 }, /* active: >10k I/O in the last 30 seconds */
	{ "wl_acs_cs_scan_timer", "900", 0 },	/* acsd fcs cs scan timeout */
	{ "wl_acs_ci_scan_timer", "4", 0 },	/* acsd fcs CI scan period */
	{ "wl_acs_ci_scan_timeout", "300", 0 },	/* acsd fcs CI scan timeout */
	{ "wl_acs_scan_entry_expire", "3600", 0 },	/* acsd fcs scan expier time */
	{ "wl_acs_tx_idle_cnt", "0", 0 },		/* acsd fcs tx idle thld */
	{ "wl_acs_chan_dwell_time", "70", 0 },	/* acsd fcs chan dwell time */
	{ "wl_acs_chan_flop_period", "70", 0 },	/* acsd fcs chan flip-flop time */
	{ "wl_intfer_period", "1", 0 },		/* acsd fcs sample period */
	{ "wl_intfer_cnt", "3", 0 },		/* acsd fcs sample cnt */
	{ "wl_intfer_txfail", "5", 0 },		/* fcs non-TCP txfail threshold setting */
	{ "wl_intfer_tcptxfail", "5", 0 },	/* fcs TCP txfail threshold setting */

#ifdef RTCONFIG_BCM10
	{ "wl_pspretend_retry_limit", "5", 0 }, /* Enable PsPretend */
#else
	{ "wl_pspretend_retry_limit", "0", 0 }, /* Disable PsPretend */
#endif
#ifndef RTCONFIG_BCM7
	{ "wl_pspretend_threshold", "0", 0 },	/* Disable PsPretend Threshold */
#else
	{ "wl_taf_enable", "0", 0 }, 		/* Disable TAF */
#endif
#ifdef BCM_BSD
	{ "bsd_role", "3", 0 },			/* Disable Band Steer Daemon */
						/* 0:Disable, 1:Primary, 2:Helper, 3:Standalone */
	{ "bsd_hport", "9877", 0 },		/* BSD helper port */
	{ "bsd_pport", "9878", 0 },		/* BSD Primary port */
	{ "bsd_helper", "192.168.1.2", 0 },	/* BSD primary ipaddr */
	{ "bsd_primary", "192.168.1.1", 0 },	/* BSD Helper ipaddr */
	{ "smart_connect_x", "0", 0 },		/* 0:Disable, 1:Tri-band, 2:5GHz */
#if 0
	{ "bsd_msglevel", "0x000010", 0 },	/* BSD_DEBUG_STEER */
	{ "bsd_dbg", "1"},
#endif
#ifdef RTAC3200
	{"bsd_ifnames", "eth2 eth1 eth3", 0 },
	{"wl0_bsd_steering_policy", "0 5 3 -52 0 110 0x22", 0 },
	{"wl1_bsd_steering_policy", "80 5 3 -82 0 0 0x0", 0 },
	{"wl2_bsd_steering_policy", "0 5 3 -82 0 0 0x8", 0 },
	{"wl0_bsd_sta_select_policy", "2 -52 0 110 0 0 -1 0 0 0 0x122", 0 },
	{"wl1_bsd_sta_select_policy", "2 -82 0 0 0 0 1 0 0 0 0x4", 0 },
	{"wl2_bsd_sta_select_policy", "2 -82 0 0 0 0 1 0 0 0 0x8", 0 },
	{"wl0_bsd_if_select_policy", "eth3 eth1", 0 },
	{"wl1_bsd_if_select_policy", "eth2 eth3", 0 },
	{"wl2_bsd_if_select_policy", "eth2 eth1", 0 },
	{"wl0_bsd_if_qualify_policy", "0 0x0", 0 },
	{"wl1_bsd_if_qualify_policy", "60 0x0", 0 },
	{"wl2_bsd_if_qualify_policy", "0 0x4", 0 },
	{"bsd_bounce_detect", "180 1 3600", 0 },
	{"bsd_aclist_timeout", "1", 0},
#elif RTAC5300
	// Tri-Band
	{"bsd_ifnames", "eth1 eth2 eth3", 0 },
	{"wl0_bsd_steering_policy", "0 5 3 -52 0 110 0x22", 0 },
	{"wl1_bsd_steering_policy", "80 5 3 -82 0 0 0x0", 0 },
	{"wl2_bsd_steering_policy", "0 5 3 -82 0 0 0x8", 0 },
	{"wl0_bsd_sta_select_policy", "2 -52 0 110 0 0 -1 0 0 0 0x122", 0 },
	{"wl1_bsd_sta_select_policy", "2 -82 0 0 0 0 1 0 0 0 0x4", 0 },
	{"wl2_bsd_sta_select_policy", "2 -82 0 0 0 0 1 0 0 0 0x8", 0 },
	{"wl0_bsd_if_select_policy", "eth3 eth2", 0 },
	{"wl1_bsd_if_select_policy", "eth1 eth3", 0 },
	{"wl2_bsd_if_select_policy", "eth1 eth2", 0 },
	{"wl0_bsd_if_qualify_policy", "0 0x0", 0 },
	{"wl1_bsd_if_qualify_policy", "60 0x0", 0 },
	{"wl2_bsd_if_qualify_policy", "0 0x4", 0 },
	{"bsd_bounce_detect", "180 1 3600", 0 },
	{"bsd_aclist_timeout", "1", 0},
	// 5GHz Only
	{"bsd_ifnames_x", "eth2 eth3", 0 },
	{"wl1_bsd_steering_policy_x", "80 5 3 0 0 0 0x4", 0 },
	{"wl2_bsd_steering_policy_x", "0 5 3 0 0 0 0x8", 0 },
	{"wl1_bsd_sta_select_policy_x", "4 0 0 0 0 0 1 0 0 0 0x4", 0 },
	{"wl2_bsd_sta_select_policy_x", "4 0 0 0 0 0 1 0 0 0 0x8", 0 },
	{"wl1_bsd_if_select_policy_x", "eth3", 0 },
	{"wl2_bsd_if_select_policy_x", "eth2", 0 },
	{"wl1_bsd_if_qualify_policy_x", "60 0x2", 0 },
	{"wl2_bsd_if_qualify_policy_x", "0 0x4", 0 },
	{"bsd_bounce_detect_x", "180 1 3600", 0 },
	{"bsd_aclist_timeout", "3", 0},
#endif

	{"bsd_scheme", "2", 0 },
#endif
#ifdef BCM_SSD
	{ "ssd_enable", "0", 0 },		/* Disable SSID Steer Daemon */
	{ "wl_ssd_type", "0", 0 },		/* default ssd_type "disabled" */
#endif
#if defined(RTCONFIG_BCM7) || defined(RTCONFIG_BCM_7114)
	{ "wl_dfs_pref", "" },			/* DFS Preferred channel value */
	{ "wl_probresp_mf", "0", 0 },		/* MAC filter based probe response */
	{ "wl_probresp_sw", "1", 0 },		/* SW probe response */
	{ "wl_vht_features", "-1", 0 }, 	/* VHT features */
#endif
#endif

	// make sure its purpose
	// advanced-ctnf
	{ "ct_tcp_timeout",		""},
	{ "ct_udp_timeout",		"30 180"},
	{ "ct_timeout",			""},
	{ "ct_max",			"300000"}, //All the models we support would default to this value

#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
	{ "ctf_disable",		"0"		},
	{ "ctf_disable_force", 		"0"		},
#ifdef RTCONFIG_BCMFA
	{ "ctf_fa_mode",		"0"		},
#endif
#ifdef RTCONFIG_BCMARM
	{ "ctf_pt_udp",			"0"		},
#endif
#if 0
#ifdef RTCONFIG_USB_MODEM
	// TODO: for the bad CTF. After updating CTF, need to mark these codes.
	{ "ctf_disable_modem", 		"0"		},
#endif
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
#if defined(RTCONFIG_LED_BTN) || defined(RTCONFIG_WPS_ALLLED_BTN)
	{ "AllLED",		"1"		},
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
#if defined(RTCONFIG_DEFAULT_AP_MODE)
	{ "lan_proto",			"dhcp"	},	// DHCP server [static|dhcp]	//Barry add 2004 09 16
#else
	{ "lan_proto",			"static"	},	// DHCP server [static|dhcp]	//Barry add 2004 09 16
#endif
#if defined(RTN300)
	{ "lan_ipaddr",			"192.168.50.1"	},	// LAN IP address
	{ "lan_ipaddr_rt",		"192.168.50.1"	},
#else
	{ "lan_ipaddr",			"192.168.1.1"	},	// LAN IP address
	{ "lan_ipaddr_rt",		"192.168.1.1"	},
#endif
	{ "lan_netmask",		"255.255.255.0"	},	// LAN netmask
	{ "lan_netmask_rt",		"255.255.255.0" },
	{ "lan_gateway",		"0.0.0.0"	},	// LAN Gateway

	{ "lan_wins",			""		},	// x.x.x.x x.x.x.x ...
	{ "lan_domain",			""		},	// LAN domain name
	{ "lan_lease",			"86400"		},	// LAN lease time in seconds
	{ "lan_stp",			"1"		},	// LAN spanning tree protocol
#ifdef RTCONFIG_LACP
	{ "lacp_enabled",			"0"		},	// LACP enable or not
#endif
	{ "lan_route",			""		},	// Static routes (ipaddr:netmask:gateway:metric:ifname ...)

#if defined(RTCONFIG_DEFAULT_AP_MODE)
	{ "lan_dnsenable_x", "1"},
#else
	{ "lan_dnsenable_x", "0"},
#endif
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
#if defined(RTN300)
	{ "dhcp_start", "192.168.50.2"},
	{ "dhcp_end", "192.168.50.254"},
#else
	{ "dhcp_start", "192.168.1.2"},
	{ "dhcp_end", "192.168.1.254"},
#endif
	{ "dhcp_lease", "86400" },
	{ "dhcp_gateway_x", "" },
	{ "dhcp_dns1_x", "" },
	{ "dhcp_dns2_x", "" },
	{ "dhcp_wins_x", "" },
	{ "dhcp_static_x", "0"},
	{ "dhcp_staticlist", ""},
	//{ "dhcpd_lmax", "253"},
	{ "dhcpc_mode", "1"},
#ifdef RTCONFIG_DHCP_OVERRIDE
	{ "dnsqmode", "2"},
#endif
	{ "dhcpd_querylog", "1"},
	{ "dhcpd_dns_router", "1"},

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
	{ "time_zone_dstoff", "M3.2.0/2,M10.2.0/2" },
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
#ifdef RTCONFIG_QCA
	{ "qca_sfe", "1"},
	{ "wl_wds_vht", 	"1"	},
	{ "traffic_5g", "0"},
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
#ifdef RTCONFIG_DSL
	{ "wan_pppoe_auth", "" },
#endif
	{ "wan_ppp_echo", "1"},

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

	// miniupnpd - PCP-related values
	{ "upnp_min_lifetime", "120" },
	{ "upnp_max_lifetime", "86400" },
	{ "upnp_pinhole_enable", "0" },
#ifdef RTCONFIG_DUALWAN // RTCONFIG_DUALWAN
#if defined(RT4GAC55U)
	{ "wans_mode", "lb" },
#else
	{ "wans_mode", "fo" }, 		// off/failover/failback/loadbance(off/fo/fb/lb)
#endif
#ifdef RTCONFIG_DSL
	{ "wans_dualwan", "dsl " DEF_SECOND_WANIF},
#else
	{ "wans_dualwan", "wan " DEF_SECOND_WANIF},
#endif
	{ "wans_standby", "0"},
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
#ifdef RT4GAC55U
	{ "wandog_interval", "3" }, // Be the same with lteled's interval.
#else
	{ "wandog_interval", "5" },
#endif
	{ "wandog_maxfail", "12" },
	{ "wandog_delay", "0" },
	{ "wandog_fb_count", "4" },
#endif // RTCONFIG_DUALWAN

#ifdef RTCONFIG_DSL
	{ "dslx_modulation", "5" }, // multiple mode
	{ "dslx_snrm_offset", "0" }, /* Paul add 2012/9/24, for SNR Margin tweaking. */
	{ "dslx_sra", "1" }, /* Paul add 2012/10/15, for setting SRA. */
	{ "dslx_bitswap", "1" }, /* Paul add 2013/10/23, for Bitswap control. */
	{ "dslx_adsl_rx_agc", "Default" }, /* Renjie add 2014/12/23, for ADSL Rx AGC(Auto Gain Control) */
#ifdef RTCONFIG_DSL_ANNEX_B //Paul add 2012/8/21
	{ "dslx_annex", "6" }, // Annex BJM (EnumAdslTypeB_J_M)
#else
	{"dslx_testlab", "disable"}, //Country-Specific Setting for AU or GB, default = disable.
	{ "dslx_annex", "4" }, // Annex AIJLM(EnumAdslTypeA_I_J_L_M)
#endif
	{ "dslx_ginp", "0" },
	{ "dslx_dla_enable", "1" },
	{ "dslx_diag_enable", "0" },
	{ "dslx_diag_duration", "0" },
	{ "dslx_diag_state", "0" },
	{ "dslx_diag_end_uptime", "0" },
	{ "dslx_diag_log_path", "" },
#ifdef RTCONFIG_VDSL
	{ "dslx_vdsl_bitswap", "1" },
	{ "dslx_vdsl_vectoring", "1" },
	{ "dslx_vdsl_nonstd_vectoring", "0" },
	{ "dslx_vdsl_target_snrm", "32767" },
	{ "dslx_vdsl_tx_gain_off", "32767" },
	{ "dslx_vdsl_rx_agc", "65535" },
	{ "dslx_vdsl_upbo", "auto" },
	{ "dslx_vdsl_esnp", "0" }, //Enhanced Sudden Noise Protection
	{ "dslx_vdsl_profile", "0" },
	{ "dslx_rmvlan", "0" },
#endif

// Viz add dsl transmode
	{ "dslx_transmode", "atm"},	//atm,ptm
	{ "dsltmp_transmode", "atm"},	//atm,ptm for UI display

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
	{ "dslx_pppoe_auth", "" },
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
	{ "dsl_subunit", "-1" },
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
#ifdef RTCONFIG_DSL_TCLINUX
	{ "dsl_dot1q", "0"},
	{ "dsl_vid", ""},
	{ "dsl_dot1p", ""},
#endif

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
#ifdef RTCONFIG_DSL_TCLINUX
	{ "dsl0_dot1q", "0"},
	{ "dsl0_vid", ""},
	{ "dsl0_dot1p", ""},
#endif
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
#ifdef RTCONFIG_DSL_TCLINUX
	{ "dsl1_dot1q", "0"},
	{ "dsl1_vid", ""},
	{ "dsl1_dot1p", ""},
#endif
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
#ifdef RTCONFIG_DSL_TCLINUX
	{ "dsl2_dot1q", "0"},
	{ "dsl2_vid", ""},
	{ "dsl2_dot1p", ""},
#endif
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
#ifdef RTCONFIG_DSL_TCLINUX
	{ "dsl3_dot1q", "0"},
	{ "dsl3_vid", ""},
	{ "dsl3_dot1p", ""},
#endif
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
#ifdef RTCONFIG_DSL_TCLINUX
	{ "dsl4_dot1q", "0"},
	{ "dsl4_vid", ""},
	{ "dsl4_dot1p", ""},
#endif
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
#ifdef RTCONFIG_DSL_TCLINUX
	{ "dsl6_dot1q", "0"},
	{ "dsl6_vid", ""},
	{ "dsl6_dot1p", ""},
#endif
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
#ifdef RTCONFIG_DSL_TCLINUX
	{ "dsl6_dot1q", "0"},
	{ "dsl6_vid", ""},
	{ "dsl6_dot1p", ""},
#endif
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
#ifdef RTCONFIG_DSL_TCLINUX
	{ "dsl7_dot1q", "0"},
	{ "dsl7_vid", ""},
	{ "dsl7_dot1p", ""},
#endif
// number of PVC , program generated
	{ "dslx_config_num", "0"},
// for debug , program generated
	{ "dslx_debug", "0"},

#ifdef RTCONFIG_VDSL
	{ "dsl8_enable", "0"},
	{ "dsl8_proto", ""},
	{ "dsl8_dot1q", "0"},
	{ "dsl8_vid", ""},
	{ "dsl8_dot1p", ""},
	{ "dsl8.1_enable", "0"},
	{ "dsl8.1_proto", ""},
	{ "dsl8.1_dot1q", "0"},
	{ "dsl8.1_vid", ""},
	{ "dsl8.1_dot1p", ""},
	{ "dsl8.2_enable", "0"},
	{ "dsl8.2_proto", ""},
	{ "dsl8.2_dot1q", "0"},
	{ "dsl8.2_vid", ""},
	{ "dsl8.2_dot1p", ""},
	{ "dsl8.3_enable", "0"},
	{ "dsl8.3_proto", ""},
	{ "dsl8.3_dot1q", "0"},
	{ "dsl8.3_vid", ""},
	{ "dsl8.3_dot1p", ""},
	{ "dsl8.4_enable", "0"},
	{ "dsl8.4_proto", ""},
	{ "dsl8.4_dot1q", "0"},
	{ "dsl8.4_vid", ""},
	{ "dsl8.4_dot1p", ""},
	{ "dsl8.5_enable", "0"},
	{ "dsl8.5_proto", ""},
	{ "dsl8.5_dot1q", "0"},
	{ "dsl8.5_vid", ""},
	{ "dsl8.5_dot1p", ""},
	{ "dsl8.6_enable", "0"},
	{ "dsl8.6_proto", ""},
	{ "dsl8.6_dot1q", "0"},
	{ "dsl8.6_vid", ""},
	{ "dsl8.6_dot1p", ""},
	{ "dsl8.7_enable", "0"},
	{ "dsl8.7_proto", ""},
	{ "dsl8.7_dot1q", "0"},
	{ "dsl8.7_vid", ""},
	{ "dsl8.7_dot1p", ""},
//
	{ "dsl9_enable", "0"},
	{ "dsl9_proto", ""},
	{ "dsl9_dot1q", "0"},
	{ "dsl9_vid", ""},
#endif

#endif

	// NVRAM for start_firewall/start_qos
	// QOS
	// Maybe removed later
	{ "qos_rulelist", "<Web Surf>>80>tcp>0~512>0<HTTPS>>443>tcp>0~512>0<File Transfer>>80>tcp>512~>3<File Transfer>>443>tcp>512~>3"},

	{ "qos_orates",	"80-100,10-100,5-100,3-100,2-95,0-0,0-0,0-0,0-0,0-0"},
	{ "qos_irates",	"100,100,100,100,100,0,0,0,0,0"},
	{ "qos_enable",			"0"				}, // 0: off, 1: on
#ifdef RTCONFIG_BWDPI
	{ "qos_type",			"1"				}, // 0: Traditional, 1: Adaptive, 2 : bandwidth limiter
#else
	{ "qos_type",			"0"				}, // 0: Traditional, 1: Adaptive, 2 : bandwidth limiter
#endif
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
	{ "qos_bw_rulelist",		""				},

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
	{ "ddns_regular_check", "0"},
	{ "ddns_regular_period", "60"},
	{ "ddns_transfer", ""},
	{ "ddns_refresh_x", "21"},	// Forced refresh period (in days)

	// NVRAM for start_firewall
	// Firewall
#ifdef RTCONFIG_PARENTALCTRL
	{"MULTIFILTER_ALL", "0"},
	{"MULTIFILTER_ENABLE", "" },
	{"MULTIFILTER_MAC", "" },
	{"MULTIFILTER_DEVICENAME", "" },
	{"MULTIFILTER_MACFILTER_DAYTIME", "" },
	{"MULTIFILTER_URL_ENABLE", "" },
	{"MULTIFILTER_URL", "" },

#ifdef RTCONFIG_BWDPI
	// TrendMicro module
	{ "wrs_enable", "0" },
	{ "wrs_rulelist", "" },			// <switch1>MAC1>Adult1>...<switch2>MAC2>Adult2>...
	{ "wrs_cc_enable", "0" },
	{ "wrs_vp_enable", "0" },
	{ "wrs_app_enable", "0" },
	{ "wrs_app_rulelist", "" },		// <switch1>MAC1>Commercial1>...<switch2>MAC2>Commercial2>...
	{ "wrs_mals_enable", "0" },
	{ "wrs_adblock_stream", "0" },
	{ "wrs_adblock_popup", "0" },
	{ "wrs_mail_bit", "6" },		// alert mail option, default enable : VP + CC
	{ "bwdpi_db_enable", "0" },		// traffic statistics switch
	{ "bwdpi_db_type", "0" },		// 0: flash, 1: USB / HDD, 2: cloud
	{ "bwdpi_db_path", "" },		// database path
	{ "bwdpi_db_debug", "0" },		// 0: 1 hour(default) ;1: 30 sec
	{ "bwdpi_rsa_check", "0" },		// signature update check
	{ "bwdpi_alive", "10" },		// check dpi engine alive timeout
	{ "bwdpi_app_rulelist", "9,20<8<4<0,5,6,15,17<13,24<1,3,14<7,10,11,21,23<<" }, // default APP priority of QoS
	{ "bwdpi_sig_ver", "" },		// dpi engine signature version
	{ "TM_EULA", "0"},			// EULA of Trend Micro
	{ "apps_analysis", "0"},		// Apps Analysis in Adaptive QoS Live
#endif
#endif	/* RTCONFIG_PARENTALCTRL */
#ifdef RTCONFIG_YANDEXDNS
	{ "yadns_enable_x", "0"},	/* 0: Disable, 1: Enable Yandex.DNS */
	{ "yadns_mode", "0"},		/* 0: Basic, 1: Safe, 2: Family, -1: Disabled */
	{ "yadns_rulelist", ""},	/* List client modes <devname>hh:ww:aa:dd:dd:rr>mode>enable... */
#endif /* RTCONFIG_YANDEXDNS */
#ifdef RTCONFIG_DNSFILTER
	{ "dnsfilter_enable_x", "0"},
	{ "dnsfilter_mode", "0"},	/* Default to no global filtering (only per client rules) */
	{ "dnsfilter_rulelist", ""},	/* List client modes <devname>hh:ww:aa:dd:dd:rr>mode... */
	{ "dnsfilter_custom1", "8.8.8.8"},	/* User-defined DNS filter 1 */
	{ "dnsfilter_custom2", "8.8.8.8"},     /* User-defined DNS filter 2 */
	{ "dnsfilter_custom3", "8.8.8.8"},     /* User-defined DNS filter 3 */
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
	{ "fw_nat_loopback", "2"},
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
#if defined(RTAC87U) || defined(DSL_AC68U)
	{ "usb_usb3", "1"},
#else
	{ "usb_usb3", "0"},
#endif
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
#ifdef RT4GAC55U
	{ "usb_ohci", "0"},
	{ "usb_gobi", "1"},
#else
	{ "usb_ohci", "1"},
#endif
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
//	{ "smbd_autoshare", "1"},
	{ "smbd_cpage", ""},
	{ "smbd_cset", "utf8"},
	{ "smbd_custom", ""},
	{ "smbd_master", "0"},
//	{ "smbd_passwd", ""},
//	{ "smbd_shares", "share</mnt<Default Share<1<0>root$</Hidden Root<0<1"},
	{ "smbd_user", "nas"},
//	{ "smbd_wgroup", "WORKGROUP"},
	{ "smbd_wins", "0"},
	{ "smbd_wanac", "0"},
	{ "smbd_simpler_naming", "0"},
	{ "smbd_enable_smb2", "0"},

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
	{ "message_loglevel", "5"},	/* KERN_INFO */

#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2)
	{ "jffs2_enable", "1" },
	{ "jffs2_scripts", "0" },
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
#if defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300)
	{ "st_samba_mode", "1"},
	{ "st_samba_force_mode", "1"},
#else
	{ "st_samba_mode", "4"},
#endif
	{ "st_ftp_mode", "2"},
	{ "enable_ftp", "0"},
	{ "enable_samba", "1"},
	{ "st_max_user", "5"},
	{ "computer_name", ""},
	{ "st_samba_workgroup", "WORKGROUP"},
	{ "ftp_lang", "EN" },
	{ "ftp_wanac", "0"},

#ifdef RTCONFIG_REBOOT_SCHEDULE
	{ "reboot_schedule_enable", "0", 0},
	{ "reboot_schedule", "00000000000", 0},
#endif

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
	{ "dms_dbdir", "/var/cache/minidlna"},
	{ "dms_dir", "/mnt"	},
	{ "dms_tivo", "0"	},
	{ "dms_stdlna", "0"	},
	{ "dms_sas", 	"0"	},
	{ "dms_dir_x", "</mnt"},
	{ "dms_dir_type_x", "<AVP"},
	{ "dms_dir_manual", "0"	},
	{ "dms_friendly_name",""},
	{ "daapd_enable", "0" 	},
	{ "daapd_friendly_name",""},
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

#if !defined(RTCONFIG_BCMARM) && !defined(RTCONFIG_QCA)
	{ "apps_ipkg_old", "1" },
#else
	{ "apps_ipkg_old", "0" },
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
	{ "http_clientlist", ""},
	{ "http_autologout", "30"},
	{ "custom_clientlist", ""},	// for custom client list

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
	{ "btn_ez_mode", "0"},	//Turn WPS into led toggle. 0: wps, 1:led
	//TODO: maybe
	//1: led. led on/off
	//2: radio. Wifi radio on/off (btn_ez_radiotoggle=1) ?
	//3: reset to default (RTCONFIG_WPS_RST_BTN: RT-N11P) ?
#ifdef RTCONFIG_WIFI_TOG_BTN
	{ "btn_wifi_toggle", "0"},
#endif
#ifdef RTCONFIG_LED_BTN
	{ "btn_led", "0"},
	{ "btn_led_mode", "0"},
#ifdef RTAC87U
	{ "LED_switch_count", "1"},
#endif
#endif
#ifdef RTCONFIG_TURBO
	{ "btn_turbo", "1"},
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
#ifdef RTCONFIG_SSH
	{ "sshd_enable", "0"},
	{ "sshd_port", "22"},
	{ "sshd_pass", "1"},
	{ "sshd_authkeys", ""},
	{ "sshd_wan","0"},
	{ "sshd_bfp","0"},
	{ "sshd_forwarding","0"},
#endif

#ifdef RTCONFIG_USB_MODEM
	{ "usb_qmi", "1"},
	{ "Dev3G", "AUTO"},
	{ "modem_android", "0"},
	{ "modem_enable", "1"}, // 0: disabled, 1: WCDMA, 2: CDMA2000, 3: TD-SCDMA, 4: WiMAX.
	{ "modem_autoapn", "1"}, // 0: disabled, 1: enabled, 2: don't change modem nvrams.
	{ "modem_roaming", "0"}, // 0: disabled, 1: enabled.
	{ "modem_roaming_mode", "1"}, // 0: automatically, 1: manually.
	{ "modem_roaming_isp", ""}, // operator at the long format.
	{ "modem_roaming_imsi", ""}, // MCC+MNC
	{ "modem_roaming_scantime", "120"}, // second.
	{ "modem_roaming_scanlist", "/tmp/cops.txt"}, // station lists.
	{ "modem_pincode", ""},
	{ "modem_country", ""},
	{ "modem_pdp", "0"},	// 0: PDP-IP(IPv4), 1: PDP-PPP, 2: PDP-IPv6, 3: PDP-IPv4v6
	{ "modem_isp", ""},
	{ "modem_spn", ""},
	{ "modem_apn", "internet"},
	{ "modem_dialnum", "*99#"},
	{ "modem_authmode", "0"},
	{ "modem_user", ""},
	{ "modem_pass", ""},
	{ "modem_apn_v6", "internet6"},
	{ "modem_authmode_v6", "0"},
	{ "modem_user_v6", ""},
	{ "modem_pass_v6", ""},
	{ "modem_ttlsid", ""},
	{ "modem_mode", "0"}, // 0: auto, 2: 2G only, 3: 3G only, 4: 4G only, 43: 4G/3G.
	{ "modem_running", "0"},
#ifdef RTCONFIG_USB_MODEM_PIN
	{ "modem_pincode_opt", "1"},
#endif
	{ "modem_sim_num", "10"},
	{ "modem_limit_unit", "0"}, /* 0: GBytes  1: MBytes*/
	{ "modem_warning_unit", "0"}, /* 0: GBytes  1: MBytes*/
	{ "modem_sms_limit", "0"}, // 0: disable, 1: enable.
	{ "modem_sms_phone", ""},
	{ "modem_sms_message1", "This is a alert about the data usage is over:"},
	{ "modem_sms_message2", "The data usage has reached the limit:"},
	{ "modem_reg_time", "10"},
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	{ "modem_bytes_data_cycle", "31"},
	{ "modem_bytes_data_limit", "0"}, /* 0: disabled */
	{ "modem_bytes_data_warning", "0"}, /* 0: disabled */
	{ "modem_bytes_data_save", "1800"}, // the interval to save the data usage. need to be 30 times.
#endif
	{ "modem_mtu", "0"}, // 0: auto, 576 <= x < interface mtu
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

	{"env_path", ""},
#if 0
	{"debug_logeval", "0"},
	{"debug_cprintf", "0"},
	{"debug_cprintf_file", "0"},
	{"debug_logrc", "0"},
	{"debug_ovrc", "0"},
	{"debug_abrst", "0"},
#endif
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

#if defined(RTCONFIG_PPTPD) || defined(RTCONFIG_ACCEL_PPTPD)
	{"pptpd_enable", 	"0"},
	{"pptpd_broadcast",	"0"},
	{"pptpd_ms_network",	"1"},
	{"pptpd_chap", 		"0"},	 // 0/1/2(Auto/MS-CHAPv1/MS-CHAPv2)
	{"pptpd_mppe", 		"13"}, 	 // 1|4|8(MPPE-128|MPPE-40|No Encryption)
	{"pptpd_dns1", 		""},
	{"pptpd_dns2", 		""},
	{"pptpd_wins1", 	""},
	{"pptpd_wins2", 	""},
	{"pptpd_server", 	""},
	{"pptpd_clients", 	"192.168.10.2-11"},
	{"pptpd_clientlist", 	""},
	{"pptpd_mru",		"1450"},
	{"pptpd_mtu",		"1450"},
	{"pptpd_sr_rulelist",	""},
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
	{ "vpnc_auto_conn", ""},
	{ "vpnc_pptp_options_x_list", ""},
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
	{ "vpn_serverx_start",		""		},
	{ "vpn_serverx_dns",		""		},
	{ "vpn_serverx_clientlist",	""		},
	{ "vpn_server_poll",		"0"		},
	{ "vpn_server_if",		"tun"		},
	{ "vpn_server_proto",		"udp"		},
	{ "vpn_server_port",		"1194"		},
	{ "vpn_server_sockbuf",		"1"		},
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
	{ "vpn_server1_poll",		"0"		},
	{ "vpn_server1_if",		"tun"		},
	{ "vpn_server1_proto",		"udp"		},
	{ "vpn_server1_port",		"1194"		},
	{ "vpn_server_sockbuf",         "1"     	},
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
	{ "vpn_crt_server1_crl",	""		},
	{ "vpn_server2_poll",		"0"		},
	{ "vpn_server2_if",		"tun"		},
	{ "vpn_server2_proto",		"udp"		},
	{ "vpn_server2_port",		"1194"		},
	{ "vpn_server_sockbuf",         "1"     	},
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
	{ "vpn_crt_server2_crl",	""		},
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
	{ "vpn_crt_client1_crl",	""		},
	{ "vpn_client1_userauth",	"0"		},
	{ "vpn_client1_username",	""		},
	{ "vpn_client1_password",	""		},
	{ "vpn_client1_useronly",	"0"		},
	{ "vpn_client1_state",		"0"		},
	{ "vpn_client1_errno",		"0"		},
	{ "vpn_client1_clientlist",	""		},
	{ "vpn_client1_enforce",	"0"		},
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
	{ "vpn_crt_client2_crl",	""		},
	{ "vpn_client2_userauth",	"0"		},
	{ "vpn_client2_username",	""		},
	{ "vpn_client2_password",	""		},
	{ "vpn_client2_state",		"0"		},
	{ "vpn_client2_errno",		"0"		},
	{ "vpn_client2_clientlist",	""		},
	{ "vpn_client2_enforce",	"0"		},

	{ "vpn_client3_poll",		"0"		},
	{ "vpn_client3_if",		"tun"		},
	{ "vpn_client3_bridge",		"1"		},
	{ "vpn_client3_nat",		"1"		},
	{ "vpn_client3_proto",		"udp"		},
	{ "vpn_client3_addr",		""		},
	{ "vpn_client3_port",		"1194"		},
	{ "vpn_client3_retry",		"30"		},
	{ "vpn_client3_rg",		"0"		},
	{ "vpn_client3_firewall",	"auto"		},
	{ "vpn_client3_crypt",		"tls"		},
	{ "vpn_client3_comp",		"adaptive"	},
	{ "vpn_client3_cipher",		"default"	},
	{ "vpn_client3_local",		"10.8.0.2"	},
	{ "vpn_client3_remote",		"10.8.0.1"	},
	{ "vpn_client3_nm",		"255.255.255.0"	},
	{ "vpn_client3_reneg",		"-1"		},
	{ "vpn_client3_hmac",		"-1"		},
	{ "vpn_client3_adns",		"0"		},
	{ "vpn_client3_rgw",		"0"		},
	{ "vpn_client3_gw",		""		},
	{ "vpn_client3_custom",		""		},
	{ "vpn_client3_cn",		""		},
	{ "vpn_client3_tlsremote",	"0"		},
	{ "vpn_client3_useronly",	"0"		},
	{ "vpn_crt_client3_static",	""		},
	{ "vpn_crt_client3_ca",		""		},
	{ "vpn_crt_client3_crt",	""		},
	{ "vpn_crt_client3_key",	""		},
	{ "vpn_crt_client3_crl",	""		},
	{ "vpn_client3_userauth",	"0"		},
	{ "vpn_client3_username",	""		},
	{ "vpn_client3_password",	""		},
	{ "vpn_client3_state",		"0"		},
	{ "vpn_client3_errno",		"0"		},
	{ "vpn_client3_clientlist",	""		},
	{ "vpn_client3_enforce",	"0"		},

	{ "vpn_client4_poll",		"0"		},
	{ "vpn_client4_if",		"tun"		},
	{ "vpn_client4_bridge",		"1"		},
	{ "vpn_client4_nat",		"1"		},
	{ "vpn_client4_proto",		"udp"		},
	{ "vpn_client4_addr",		""		},
	{ "vpn_client4_port",		"1194"		},
	{ "vpn_client4_retry",		"30"		},
	{ "vpn_client4_rg",		"0"		},
	{ "vpn_client4_firewall",	"auto"		},
	{ "vpn_client4_crypt",		"tls"		},
	{ "vpn_client4_comp",		"adaptive"	},
	{ "vpn_client4_cipher",		"default"	},
	{ "vpn_client4_local",		"10.8.0.2"	},
	{ "vpn_client4_remote",		"10.8.0.1"	},
	{ "vpn_client4_nm",		"255.255.255.0"	},
	{ "vpn_client4_reneg",		"-1"		},
	{ "vpn_client4_hmac",		"-1"		},
	{ "vpn_client4_adns",		"0"		},
	{ "vpn_client4_rgw",		"0"		},
	{ "vpn_client4_gw",		""		},
	{ "vpn_client4_custom",		""		},
	{ "vpn_client4_cn",		""		},
	{ "vpn_client4_tlsremote",	"0"		},
	{ "vpn_client4_useronly",	"0"		},
	{ "vpn_crt_client4_static",	""		},
	{ "vpn_crt_client4_ca",		""		},
	{ "vpn_crt_client4_crt",	""		},
	{ "vpn_crt_client4_key",	""		},
	{ "vpn_crt_client4_crl",	""		},
	{ "vpn_client4_userauth",	"0"		},
	{ "vpn_client4_username",	""		},
	{ "vpn_client4_password",	""		},
	{ "vpn_client4_state",		"0"		},
	{ "vpn_client4_errno",		"0"		},
	{ "vpn_client4_clientlist",	""		},
	{ "vpn_client4_enforce",	"0"		},

	{ "vpn_client5_poll",		"0"		},
	{ "vpn_client5_if",		"tun"		},
	{ "vpn_client5_bridge",		"1"		},
	{ "vpn_client5_nat",		"1"		},
	{ "vpn_client5_proto",		"udp"		},
	{ "vpn_client5_addr",		""		},
	{ "vpn_client5_port",		"1194"		},
	{ "vpn_client5_retry",		"30"		},
	{ "vpn_client5_rg",		"0"		},
	{ "vpn_client5_firewall",	"auto"		},
	{ "vpn_client5_crypt",		"tls"		},
	{ "vpn_client5_comp",		"adaptive"	},
	{ "vpn_client5_cipher",		"default"	},
	{ "vpn_client5_local",		"10.8.0.2"	},
	{ "vpn_client5_remote",		"10.8.0.1"	},
	{ "vpn_client5_nm",		"255.255.255.0"	},
	{ "vpn_client5_reneg",		"-1"		},
	{ "vpn_client5_hmac",		"-1"		},
	{ "vpn_client5_adns",		"0"		},
	{ "vpn_client5_rgw",		"0"		},
	{ "vpn_client5_gw",		""		},
	{ "vpn_client5_custom",		""		},
	{ "vpn_client5_cn",		""		},
	{ "vpn_client5_tlsremote",	"0"		},
	{ "vpn_client5_useronly",	"0"		},
	{ "vpn_crt_client5_static",	""		},
	{ "vpn_crt_client5_ca",		""		},
	{ "vpn_crt_client5_crt",	""		},
	{ "vpn_crt_client5_key",	""		},
	{ "vpn_crt_client5_crl",	""		},
	{ "vpn_client5_userauth",	"0"		},
	{ "vpn_client5_username",	""		},
	{ "vpn_client5_password",	""		},
	{ "vpn_client5_state",		"0"		},
	{ "vpn_client5_errno",		"0"		},
	{ "vpn_client5_clientlist",	""		},
	{ "vpn_client5_enforce",	"0"		},

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
	{ "vpn_client_clientlist",	""		},
	{ "vpn_client_enforce",		"0"		},
#endif

#ifdef RTCONFIG_PUSH_EMAIL
	{ "PM_enable", "0"},				/* Enable Push Mail feature. */
	{ "PM_type", "0"},				/* Send the tracking to the of xDSL team's official gmail. */
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
	{ "PM_title", ""},				/* The title of mail. */
	{ "PM_target", ""},				/* The address of Mail Server. */
	{ "PM_restart", "0"},				/* reset the Push Mail Service. */
	{ "PM_freq", "0"},				/* 0:daily, 1:weekly, 2:monthly. */
	{ "PM_mon", "0"},				/* months since January (0 to 11). */
	{ "PM_day", "0"},				/* days since Sunday (0 to 6 Sunday=0). */
	{ "PM_hour", "0"},				/* hours since midnight (0 to 23). */
	{ "pushnotify_httplogin", "1"},
	{ "pushnotify_diskmonitor", "1"},
	{ "PM_attach_syslog", ""},
	{ "PM_attach_cfgfile", ""},
	{ "PM_attach_modemlog", ""},
	{ "fb_country", ""},
	{ "fb_email", ""},
	{ "fb_comment", ""},
	{ "fb_email_dbg", ""},	//send to email address
	{ "fb_state", ""},
	{ "fb_browserInfo", ""},
#ifdef RTCONFIG_DSL
	// for web ui udentify , 0=notificaton, 1=close
	{ "fb_experience", "0"},
	{ "fb_ISP", ""},
	{ "fb_Subscribed_Info", ""},
	{ "PM_attach_iptables", ""},
	{ "fb_availability", ""},
#else
	{ "fb_ptype", ""},
	{ "fb_pdesc", ""},
#endif /* RTCONFIG_DSL */
#endif /* RTCONFIG_PUSH_EMAIL */

#ifdef __CONFIG_NORTON__
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
	{ "ipv6_service",	"disabled"	},	// disabled/dhcp6/6in4/6to4/6rd/other
	{ "ipv6_ifdev",		"ppp"		},
	{ "ipv6_prefix",	""		},	// The global-scope IPv6 prefix to route/advertise
	{ "ipv6_prefix_length",	"64"		},	// The bit length of the prefix. Used by dhcp6c. For radvd, /64 is always assumed.
	{ "ipv6_dhcp_pd",	"1"		},	// Enable DHCP-PD (DHCP Prefix Delegation) by default
	{ "ipv6_rtr_addr",	""		},	// defaults to $ipv6_prefix::1
	{ "ipv6_prefix_len_wan","64"		},	// used in ipv6_service other
	{ "ipv6_ipaddr",	""		},	// used in ipv6_service other
	{ "ipv6_gateway",	""		},	// used in ipv6_service other
	{ "ipv6_radvd",		"1"		},	// Enable Router Advertisement (radvd)
	{ "ipv6_relay",		"192.88.99.1"	},	// IPv6 Anycast Address
	{ "ipv6_tun_v4end",	"0.0.0.0"	},	// Foreign IPv4 endpoint of SIT tunnel
	{ "ipv6_tun_addr",	""		},	// IPv6 address to assign to local tunnel endpoint
	{ "ipv6_tun_addrlen",	"64"		},	// CIDR prefix length for tunnel's IPv6 address
	{ "ipv6_tun_peer",	""		},	// IPv6 address of remote tunnel endpoint
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
	{ "ipv6_autoconf_type", "0"		},	// IPv6 LAN auconfiguration type, 0: stateless, 1: stateful
	{ "ipv6_dhcp_lifetime", "86400"		},	// valid lifetimes of IPv6 prefix in seconds
	{ "ipv6_dhcp_start",	""		},	// start of address pool
	{ "ipv6_dhcp_end",	""		},	// end of address pool
	{ "ipv6_fw_enable",	"1"		},	// Default FORWARD table to DROP packets
	{ "ipv6_fw_rulelist",	""		},	// IPv6 allowed forward rules
	{ "ipv6_prefix_s",	""		},	// for ipv6 6in4
	{ "ipv6_prefix_length_s", "64"		},	// for ipv6 6in4/other
	{ "ipv6_rtr_addr_s",	""		},	// for ipv6 other
	{ "ipv6_dhcp6s_enable",	"1"		},	// DHCP6 Server for LAN
	{ "ipv6_ns_drop", "0" 		},	 	// Filter out neighbour solicitation flood on Comcast network
#endif
	{ "web_redirect", 	"1"		},	// Only NOLINK is redirected in default, it is overwrited in init_nvram
	{ "disiosdet",		"1"		},
	{ "nat_redirect_enable",	"1"},

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

#ifdef RTCONFIG_SNMPD
	{ "snmpd_enable", "0"},
	{ "snmpd_wan", "0"},
	{ "sysName", ""},
	{ "sysContact", ""},
	{ "sysLocation", ""},
	{ "roCommunity", ""},
	{ "rwCommunity", ""},
	{ "v3_auth_type", ""},
	{ "v3_priv_type", ""},
	{ "v3_auth_passwd", ""},
	{ "v3_priv_passwd", ""},
#endif

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
#if defined(RTCONFIG_TR069)
	{ "tr_enable", "0"},
	{ "tr_discovery", "0" },
	{ "tr_inform_enable", "1"},
	{ "tr_inform_interval", "86400"},
	{ "tr_acs_url", ""},
	{ "tr_username", ""},
	{ "tr_passwd", ""},
	{ "tr_conn_username", "admin"},
	{ "tr_conn_passwd", "admin"},
	{ "tr_conn_port", "7547"},
	{ "tr_ca_cert", ""},
	{ "tr_client_cert", ""},
	{ "tr_client_key", ""},
	{ "tr_client_key", ""},
	{ "pvgcode", "ASUS"},
#endif
	{ "Ate_fw_fail",		"10"},
	{ "Ate_reboot_delay",		"1"},
#if (defined(RTCONFIG_USER_LOW_RSSI) || defined(RTCONFIG_NEW_USER_LOW_RSSI))
	{ "wl_user_rssi",		"0"},		/* disabled by default, setting range: -70 ~ -90 */
#ifndef RTCONFIG_BCMARM
	{ "wl_lrc",			"2"},
#endif
	{ "rast_idlrt",			"2"},		/* roaming assistant idle rate (Kbps) */
#endif
	{ "webui_resolve_conn",		"0"},
	{ "led_disable",		"0"},
	{ "custom_clientlist",		""},		/* for customize device name */
	{ "nmp_client_list",		""},
	{ "ttl_inc_enable",		"0"},		/* enable TTL increment */
#ifdef RTCONFIG_TOR
	{ "Tor_enable",                 "0"},           /* enable Tor Transparent Proxy */
	{ "Tor_socksport",              "9050"},
	{ "Tor_transport",              "9040"},
	{ "Tor_dnsport",                "9053"},
	{ "Tor_redir_list",             ""},
#endif
#ifdef RTCONFIG_JFFS2USERICON
	{ "custom_usericon",	""},
	{ "custom_usericon_del",	""},
#endif
#ifdef RTCONFIG_TRAFFIC_CONTROL
	{ "traffic_control_enable", "0"},
	{ "traffic_control_date_start", ""},
	{ "traffic_control_alert_enable", "1"},
	{ "traffic_control_alert_max", "40"},
	{ "traffic_control_limit_enable", "0"},
	{ "traffic_control_limit_max", "45"},
#endif
#ifdef RTCONFIG_CLOUDCHECK
	{ "enable_cloudcheck",	"0"},
#endif
#ifdef RTCONFIG_BCMARM
	{ "drop_caches", "1"},
#endif
#ifdef RTCONFIG_RESTRICT_GUI
	{ "fw_restrict_gui",	"0"},
	{ "fw_restrict_gui_mac",	""},
#endif
#ifdef RTCONFIG_KEY_GUARD
	{ "kg_enable",	"0"},
	{ "kg_wan_enable",	"1"},
	{ "kg_powersaving_enable",	"1"},
	{ "kg_wl_radio_enable",	"0"},
	{ "kg_wl_radio",	"0"},
	{ "kg_device_enable",	""},
	{ "kg_devicename",	""},
	{ "kg_mac",	""},
#endif
#ifdef RTCONFIG_WTFAST
	{ "wtf_username", ""},
	{ "wtf_passwd", ""},
	{ "wtf_account_type", ""},
	{ "wtf_max_clients", ""},
	{ "wtf_rulelist", ""},
	{ "wtf_login", 	""},
#endif
	{ "guard_mode",	"0"},
#ifdef RTCONFIG_BCM_7114
	{ "stop_tweak_usb",	"1"},
#endif
	{ NULL, NULL }
}; // router_defaults

// nvram for system control state
struct nvram_tuple router_state_defaults[] = {
	{ "rc_service", ""},

	{ "asus_mfg", "0"},
	{ "btn_rst", "0"},
	{ "btn_ez", "0"},
#ifdef RTCONFIG_WIFI_TOG_BTN
	{ "btn_wifi_toggle", "0"},
#endif

	{ "wan_primary", "0" }, // Always first run in WAN port.
	{ "wan0_primary", "1" },
	{ "wan1_primary", "0" },
	{ "wan_pppoe_ifname", "" },
	{ "wan_state_t", "0" },
	{ "wan_sbstate_t", "0" },	// record substate for each wan state
	{ "wan_auxstate_t", "0" },	// which is maintained by wanduck
	{ "wan_proto_t", "" },

#ifdef RTCONFIG_GETREALIP
	{ "wan_realip_state",	"0"},
	{ "wan_realip_ip",	""},
#endif

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

	{ "autodet_state", ""},
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
	{ "webs_notif_flag", "" },

	{ "ftp_ports", ""},

#ifdef RTCONFIG_DSL
// name starting with 'dsl' are reserved for dsl unit
// for temp variable please use dsltmp_xxx
	{ "dsltmp_autodet_state", "down"},
	{ "dsltmp_autodet_vpi", "0"},
	{ "dsltmp_autodet_vci", "35"},
	{ "dsltmp_autodet_encap", "0"},
	{ "dsltmp_autodet_wan_type", "ATM"},
// manually config
	{ "dsltmp_cfg_prctl", "0"},
	{ "dsltmp_cfg_vpi", "0"},
	{ "dsltmp_cfg_vci", "35"},
	{ "dsltmp_cfg_encap", "0"},
	{ "dsltmp_cfg_iptv_idx", ""},
	{ "dsltmp_cfg_iptv_num_pvc", "0"},
	{ "dsltmp_cfg_ispname", ""},
	{ "dsltmp_cfg_country", ""},
#ifdef RTCONFIG_VDSL
	{ "dsltmp_cfg_dot1q", "0"},
	{ "dsltmp_cfg_vid", ""},
#endif

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
	{ "dsltmp_qis_admin_username", ""},
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
//	log sync status
	{ "dsltmp_syncloss", "0"},
	{ "dsltmp_syncloss_apply", "0"},
	{ "dsltmp_syncup_cnt", "0"},
//	dla for notification
	{ "dsltmp_dla_modified", "0"},

#ifdef RTCONFIG_DSL_TCLINUX
	{ "dsllog_fwver", ""},
	{ "dsllog_drvver", ""},
	//{ "dsllog_linestate", ""},	//dsltmp_adslsyncsts
	{ "dsllog_opmode", ""},
	{ "dsllog_adsltype", ""},
	{ "dsllog_snrmargindown", ""},
	{ "dsllog_snrmarginup", ""},
	{ "dsllog_attendown", ""},
	{ "dsllog_attenup", ""},
	{ "dsllog_wanlistmode", ""},
	{ "dsllog_dataratedown", ""},
	{ "dsllog_datarateup", ""},
	{ "dsllog_attaindown", ""},
	{ "dsllog_attainup", ""},
	{ "dsllog_powerdown", ""},
	{ "dsllog_powerup", ""},
	{ "dsllog_crcdown", ""},
	{ "dsllog_crcup", ""},
	{ "dsllog_farendvendorid", ""},
	{ "dsllog_pathmodedown", ""},
	{ "dsllog_pathmodeup", ""},
	{ "dsllog_interleavedepthdown", ""},
	{ "dsllog_interleavedepthup", ""},
	{ "dsllog_tcm", ""},
	{ "dsllog_vdslcurrentprofile", ""},
#endif

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
	/* Cherry added temporarily or mobile broadband  web implementation in 2014/8/20.*/
	{ "data_usage", "3"},
	{ "data_usage_cycle", "30"},
	{ "data_usage_limit", "8"},
	{ "data_usage_warning", "6"},
	{ "modem_idletime", "600"},
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
	{ "pci/2/1/ledbh10", "7", 0 },

	{ 0, 0, 0 }
};
#else
#ifdef RTAC3200
struct nvram_tuple bcm4360ac_defaults[] = {
	{ "devpath1", "pcie/1/4", 0 },
	{ "1:devpath1", "sb/1/", 0 },
	{ "1:boardrev", "0x1421", 0 },
	{ "1:boardvendor", "0x14e4", 0 },
	{ "1:devid", "0x43bb", 0 },
	{ "1:sromrev", "11", 0 },
	{ "1:boardflags", "0x20001000", 0 },
	{ "1:boardflags2", "0x00100002", 0 },
	{ "1:venvid", "0x14e4", 0 },
	{ "1:boardflags3", "0x4000005", 0 },
	{ "1:aa2g", "7", 0 },
	{ "1:agbg0", "0x0", 0 },
	{ "1:agbg1", "0x0", 0 },
	{ "1:agbg2", "0x0", 0 },
	{ "1:txchain", "7", 0 },
	{ "1:rxchain", "7", 0 },
	{ "1:antswitch", "0", 0 },
	{ "1:femctrl", "3", 0 },
	{ "1:tssiposslope2g", "1", 0 },
	{ "1:epagain2g", "0", 0 },
	{ "1:pdgain2g", "21", 0 },
	{ "1:tworangetssi2g", "0", 0 },
	{ "1:papdcap2g", "0", 0 },
	{ "1:gainctrlsph", "0", 0 },
	{ "1:tempthresh", "120", 0 },
	{ "1:tempoffset", "255", 0 },
	{ "1:rawtempsense", "0x1ff", 0 },
	{ "1:tempsense_slope", "0xff", 0 },
	{ "1:tempcorrx", "0x3f", 0 },
	{ "1:tempsense_option", "0x3", 0 },
	{ "1:xtalfreq", "40000", 0 },
	{ "1:phycal_tempdelta", "15", 0 },
	{ "1:temps_period", "5", 0 },
	{ "1:temps_hysteresis", "5", 0 },
	{ "1:pdoffset2g40ma0", "15", 0 },
	{ "1:pdoffset2g40ma1", "15", 0 },
	{ "1:pdoffset2g40ma2", "15", 0 },
	{ "1:pdoffset2g40mvalid", "1", 0 },
	{ "1:pdoffset40ma0", "0", 0 },
	{ "1:pdoffset40ma1", "0", 0 },
	{ "1:pdoffset40ma2", "0", 0 },
	{ "1:pdoffset80ma0", "0", 0 },
	{ "1:pdoffset80ma1", "0", 0 },
	{ "1:pdoffset80ma2", "0", 0 },
	{ "1:cckbw202gpo", "0", 0 },
	{ "1:cckbw20ul2gpo", "0", 0 },
	{ "1:dot11agofdmhrbw202gpo", "0x2000", 0 },
	{ "1:ofdmlrbw202gpo", "0", 0 },
	{ "1:dot11agduphrpo", "0", 0 },
	{ "1:dot11agduplrpo", "0", 0 },
	{ "1:maxp2ga0", "102", 0 },
	{ "1:rxgains2gelnagaina0", "4", 0 },
	{ "1:rxgains2gtrisoa0", "7", 0 },
	{ "1:rxgains2gtrelnabypa0", "1", 0 },
	{ "1:maxp2ga1", "102", 0 },
	{ "1:rxgains2gelnagaina1", "4", 0 },
	{ "1:rxgains2gtrisoa1", "7", 0 },
	{ "1:rxgains2gtrelnabypa1", "1", 0 },
	{ "1:maxp2ga2", "102", 0 },
	{ "1:rxgains2gelnagaina2", "4", 0 },
	{ "1:rxgains2gtrisoa2", "7", 0 },
	{ "1:rxgains2gtrelnabypa2", "1", 0 },
	{ "1:ledbh10", "7", 0 },
	{ "devpath0", "pcie/1/3", 0 },
	{ "0:devpath0", "sb/1/", 0 },
	{ "0:boardrev", "0x1421", 0 },
	{ "0:boardvendor", "0x14e4", 0 },
	{ "0:devid", "0x43bc", 0 },
	{ "0:sromrev", "11", 0 },
	{ "0:boardflags", "0x30040000", 0 },
	{ "0:boardflags2", "0x00220102", 0 },
	{ "0:venid", "0x14e4", 0 },
	{ "0:boardflags3", "0x0", 0 },
	{ "0:aa5g", "7", 0 },
	{ "0:aga0", "0x0", 0 },
	{ "0:aga1", "0x0", 0 },
	{ "0:aga2", "0x0", 0 },
	{ "0:txchain", "7", 0 },
	{ "0:rxchain", "7", 0 },
	{ "0:antswitch", "0", 0 },
	{ "0:femctrl", "3", 0 },
	{ "0:tssiposslope5g", "1", 0 },
	{ "0:epagain5g", "0", 0 },
	{ "0:pdgain5g", "4", 0 },
	{ "0:tworangetssi5g", "0", 0 },
	{ "0:papdcap5g", "0", 0 },
	{ "0:gainctrlsph", "0", 0 },
	{ "0:tempthresh", "125", 0 },
	{ "0:tempoffset", "255", 0 },
	{ "0:rawtempsense", "0x1ff", 0 },
	{ "0:tempsense_slope", "0xff", 0 },
	{ "0:tempcorrx", "0x3f", 0 },
	{ "0:tempsense_option", "0x3", 0 },
	{ "0:xtalfreq", "40000", 0 },
	{ "0:phycal_tempdelta", "15", 0 },
	{ "0:temps_period", "5", 0 },
	{ "0:temps_hysteresis", "5", 0 },
	{ "0:pdoffset40ma0", "4369", 0 },
	{ "0:pdoffset40ma1", "4369", 0 },
	{ "0:pdoffset40ma2", "4369", 0 },
	{ "0:pdoffset80ma0", "0", 0 },
	{ "0:pdoffset80ma1", "0", 0 },
	{ "0:pdoffset80ma2", "0", 0 },
	{ "0:subband5gver", "0x4", 0 },
	{ "0:mcsbw1605glpo", "0", 0 },
	{ "0:mcsbw1605gmpo", "0", 0 },
	{ "0:mcsbw1605ghpo", "0", 0 },
	{ "0:mcslr5glpo", "0", 0 },
	{ "0:mcslr5gmpo", "0", 0 },
	{ "0:mcslr5ghpo", "0", 0 },
	{ "0:dot11agduphrpo", "0", 0 },
	{ "0:dot11agduplrpo", "0", 0 },
	{ "0:rxgains5gmelnagaina0", "2", 0 },
	{ "0:rxgains5gmtrisoa0", "5", 0 },
	{ "0:rxgains5gmtrelnabypa0", "1", 0 },
	{ "0:rxgains5ghelnagaina0", "2", 0 },
	{ "0:rxgains5ghtrisoa0", "5", 0 },
	{ "0:rxgains5ghtrelnabypa0", "1", 0 },
	{ "0:rxgains5gelnagaina0", "2", 0 },
	{ "0:rxgains5gtrisoa0", "5", 0 },
	{ "0:rxgains5gtrelnabypa0", "1", 0 },
	{ "0:maxp5ga0", "94,94,90,90", 0 },
	{ "0:rxgains5gmelnagaina1", "2", 0 },
	{ "0:rxgains5gmtrisoa1", "5", 0 },
	{ "0:rxgains5gmtrelnabypa1", "1", 0 },
	{ "0:rxgains5ghelnagaina1", "2", 0 },
	{ "0:rxgains5ghtrisoa1", "5", 0 },
	{ "0:rxgains5ghtrelnabypa1", "1", 0 },
	{ "0:rxgains5gelnagaina1", "2", 0 },
	{ "0:rxgains5gtrisoa1", "5", 0 },
	{ "0:rxgains5gtrelnabypa1", "1", 0 },
	{ "0:maxp5ga1", "94,94,90,90", 0 },
	{ "0:rxgains5gmelnagaina2", "2", 0 },
	{ "0:rxgains5gmtrisoa2", "5", 0 },
	{ "0:rxgains5gmtrelnabypa2", "1", 0 },
	{ "0:rxgains5ghelnagaina2", "2", 0 },
	{ "0:rxgains5ghtrisoa2", "5", 0 },
	{ "0:rxgains5ghtrelnabypa2", "1", 0 },
	{ "0:rxgains5gelnagaina2", "2", 0 },
	{ "0:rxgains5gtrisoa2", "5", 0 },
	{ "0:rxgains5gtrelnabypa2", "1", 0 },
	{ "0:maxp5ga2", "94,94,90,90", 0 },
	{ "0:ledbh10", "7", 0 },
	{ "devpath2", "pcie/2/1", 0 },
	{ "2:devpath2", "sb/1/", 0 },
	{ "2:boardrev", "0x1421", 0 },
	{ "2:boardvendor", "0x14e4", 0 },
	{ "2:devid", "0x43bc", 0 },
	{ "2:sromrev", "11", 0 },
	{ "2:boardflags", "0x30040000", 0 },
	{ "2:boardflags2", "0x00220102", 0 },
	{ "2:venid", "0x14e4", 0 },
	{ "2:boardflags3", "0x0", 0 },
	{ "2:aa5g", "7", 0 },
	{ "2:aga0", "0x0", 0 },
	{ "2:aga1", "0x0", 0 },
	{ "2:aga2", "0x0", 0 },
	{ "2:txchain", "7", 0 },
	{ "2:rxchain", "7", 0 },
	{ "2:antswitch", "0", 0 },
	{ "2:femctrl", "3", 0 },
	{ "2:tssiposslope5g", "1", 0 },
	{ "2:epagain5g", "0", 0 },
	{ "2:pdgain5g", "4", 0 },
	{ "2:tworangetssi5g", "0", 0 },
	{ "2:papdcap5g", "0", 0 },
	{ "2:gainctrlsph", "0", 0 },
	{ "2:tempthresh", "120", 0 },
	{ "2:tempoffset", "255", 0 },
	{ "2:rawtempsense", "0x1ff", 0 },
	{ "2:tempsense_slope", "0xff", 0 },
	{ "2:tempcorrx", "0x3f", 0 },
	{ "2:tempsense_option", "0x3", 0 },
	{ "2:xtalfreq", "40000", 0 },
	{ "2:phycal_tempdelta", "15", 0 },
	{ "2:temps_period", "5", 0 },
	{ "2:temps_hysteresis", "5", 0 },
	{ "2:pdoffset40ma0", "4369", 0 },
	{ "2:pdoffset40ma1", "4369", 0 },
	{ "2:pdoffset40ma2", "4369", 0 },
	{ "2:pdoffset80ma0", "0", 0 },
	{ "2:pdoffset80ma1", "0", 0 },
	{ "2:pdoffset80ma2", "0", 0 },
	{ "2:subband5gver", "0x4", 0 },
	{ "2:mcsbw1605glpo", "0", 0 },
	{ "2:mcsbw1605gmpo", "0", 0 },
	{ "2:mcsbw1605ghpo", "0", 0 },
	{ "2:mcslr5glpo", "0", 0 },
	{ "2:mcslr5gmpo", "0", 0 },
	{ "2:mcslr5ghpo", "0", 0 },
	{ "2:dot11agduphrpo", "0", 0 },
	{ "2:dot11agduplrpo", "0", 0 },
	{ "2:rxgains5gmelnagaina0", "2", 0 },
	{ "2:rxgains5gmtrisoa0", "5", 0 },
	{ "2:rxgains5gmtrelnabypa0", "1", 0 },
	{ "2:rxgains5ghelnagaina0", "2", 0 },
	{ "2:rxgains5ghtrisoa0", "5", 0 },
	{ "2:rxgains5ghtrelnabypa0", "1", 0 },
	{ "2:rxgains5gelnagaina0", "2", 0 },
	{ "2:rxgains5gtrisoa0", "5", 0 },
	{ "2:rxgains5gtrelnabypa0", "1", 0 },
	{ "2:maxp5ga0", "90,90,106,106", 0 },
	{ "2:rxgains5gmelnagaina1", "2", 0 },
	{ "2:rxgains5gmtrisoa1", "5", 0 },
	{ "2:rxgains5gmtrelnabypa1", "1", 0 },
	{ "2:rxgains5ghelnagaina1", "2", 0 },
	{ "2:rxgains5ghtrisoa1", "5", 0 },
	{ "2:rxgains5ghtrelnabypa1", "1", 0 },
	{ "2:rxgains5gelnagaina1", "2", 0 },
	{ "2:rxgains5gtrisoa1", "5", 0 },
	{ "2:rxgains5gtrelnabypa1", "1", 0 },
	{ "2:maxp5ga1", "90,90,106,106", 0 },
	{ "2:rxgains5gmelnagaina2", "2", 0 },
	{ "2:rxgains5gmtrisoa2", "5", 0 },
	{ "2:rxgains5gmtrelnabypa2", "1", 0 },
	{ "2:rxgains5ghelnagaina2", "2", 0 },
	{ "2:rxgains5ghtrisoa2", "5", 0 },
	{ "2:rxgains5ghtrelnabypa2", "1", 0 },
	{ "2:rxgains5gelnagaina2", "2", 0 },
	{ "2:rxgains5gtrisoa2", "5", 0 },
	{ "2:rxgains5gtrelnabypa2", "1", 0 },
	{ "2:maxp5ga2", "90,90,106,106", 0 },
	{ "2:ledbh10", "7", 0 },
	{ 0, 0, 0 }
};
#elif defined(RTAC88U) || defined(RTAC3100)
struct nvram_tuple bcm4360ac_defaults[] = {
#if 0
	{ "devpath0", "pci/1/1", 0 },
	{ "0:sromrev", "13", 0 },
	{ "0:boardrev", "0x1104", 0 },
	{ "0:boardflags", "0x10001000", 0 },
	{ "0:boardflags2", "0x4", 0 },
	{ "0:boardflags3", "0x0", 0 },
	{ "0:boardflags4", "0x0", 0 },
	{ "0:devid", "0x43c4", 0 },
	{ "0:subvid", "0x14e4", 0 },
	//{ "0:macaddr", "00:90:4c:1d:b0:0e", 0 },
	//{ "0:ccode", "ALL", 0 },
	//{ "0:regrev", "0", 0 },
	{ "0:aa2g", "15", 0 },
	{ "0:agbg0", "133", 0 },
	{ "0:agbg1", "133", 0 },
	{ "0:agbg2", "133", 0 },
	{ "0:agbg3", "5", 0 },
	{ "0:aga0", "71", 0 },
	{ "0:aga1", "71", 0 },
	{ "0:aga2", "71", 0 },
	{ "0:aga3", "133", 0 },
	{ "0:antswitch", "0", 0 },
	{ "0:tssiposslope2g", "1", 0 },
	{ "0:epagain2g", "0", 0 },
	{ "0:pdgain2g", "2", 0 },
	{ "0:tworangetssi2g", "0", 0 },
	{ "0:papdcap2g", "0", 0 },
	{ "0:femctrl", "2", 0 },
	{ "0:gainctrlsph", "0", 0 },
	{ "0:tempthresh", "120", 0 },
	{ "0:tempoffset", "255", 0 },
	{ "0:rawtempsense", "0x1ff", 0 },
	{ "0:measpower", "0x7f", 0 },
	{ "0:tempsense_slope", "0xff", 0 },
	{ "0:tempcorrx", "0x3f", 0 },
	{ "0:tempsense_option", "0x3", 0 },
	{ "0:xtalfreq", "40000", 0 },
	{ "0:phycal_tempdelta", "40", 0 },
	{ "0:temps_period", "1", 0 },
	{ "0:temps_hysteresis", "5", 0 },
	{ "0:measpower1", "0x7f", 0 },
	{ "0:measpower2", "0x7e", 0 },
	{ "0:tssifloor2g", "0x1e", 0 },
	{ "0:pdoffset2g40ma0", "5", 0 },
	{ "0:pdoffset2g40ma1", "8", 0 },
	{ "0:pdoffset2g40ma2", "5", 0 },
	{ "0:pdoffset2g40mvalid", "0", 0 },
	{ "0:pdoffset40ma0", "43452", 0 },
	{ "0:pdoffset40ma1", "43452", 0 },
	{ "0:pdoffset40ma2", "43452", 0 },
	{ "0:pdoffset80ma0", "43452", 0 },
	{ "0:pdoffset80ma1", "65535", 0 },
	{ "0:pdoffset80ma2", "65535", 0 },
	{ "0:pdoffset20in40m2g", "31710", 0 },
	{ "0:pdoffset20in40m2gcore3", "30", 0 },
	{ "0:pdoffsetcckma0", "15", 0 },
	{ "0:pdoffsetcckma1", "15", 0 },
	{ "0:pdoffsetcckma2", "15", 0 },
	{ "0:pdoffsetcck", "0", 0 },
	{ "0:paparambwver", "0", 0 },
	{ "0:sar2g", "18", 0 },
	{ "0:noiselvl2ga0", "31", 0 },
	{ "0:noiselvl2ga1", "31", 0 },
	{ "0:noiselvl2ga2", "31", 0 },
	{ "0:noiselvl2ga3", "31", 0 },
	//{ "0:rxgainerr2ga0", "63", 0 },
	//{ "0:rxgainerr2ga1", "31", 0 },
	//{ "0:rxgainerr2ga2", "31", 0 },
	//{ "0:rxgainerr2ga3", "31", 0 },
	//{ "0:rpcal2g", "65535", 0 },
	{ "0:txidxcap2g", "0", 0 },
	{ "0:gpdn", "0xffffffff", 0 },
	{ "0:eu_edthresh2g", "255", 0 },
	{ "0:ulbpproffs2g", "65535", 0 },
	{ "0:mcs8poexp", "0", 0 },
	{ "0:mcs9poexp", "0", 0 },
	{ "0:ulbpdoffs2ga0", "65535", 0 },
	{ "0:ulbpdoffs2ga1", "65535", 0 },
	{ "0:ulbpdoffs2ga2", "65535", 0 },
	{ "0:ulbpdoffs2ga3", "65535", 0 },
	{ "0:rpcal5gb4", "65535", 0 },
	{ "0:sb20in40hrlrpox", "65535", 0 },
	{ "0:swctrlmap4_cfg", "5", 0 },
	{ "0:swctrlmap4_TX2g_fem3to0", "43690", 0 },
	{ "0:swctrlmap4_RX2g_fem3to0", "17476", 0 },
	{ "0:swctrlmap4_RXByp2g_fem3to0", "26214", 0 },
	{ "0:swctrlmap4_misc2g_fem3to0", "8738", 0 },
	{ "0:swctrlmap4_TX5g_fem3to0", "0", 0 },
	{ "0:swctrlmap4_RX5g_fem3to0", "0", 0 },
	{ "0:swctrlmap4_RXByp5g_fem3to0", "0", 0 },
	{ "0:swctrlmap4_misc5g_fem3to0", "0", 0 },
	{ "0:swctrlmap4_TX2g_fem7to4", "0", 0 },
	{ "0:swctrlmap4_RX2g_fem7to4", "0", 0 },
	{ "0:swctrlmap4_RXByp2g_fem7to4", "0", 0 },
	{ "0:swctrlmap4_misc2g_fem7to4", "0", 0 },
	{ "0:swctrlmap4_TX5g_fem7to4", "0", 0 },
	{ "0:swctrlmap4_RX5g_fem7to4", "0", 0 },
	{ "0:swctrlmap4_RXByp5g_fem7to4", "0", 0 },
	{ "0:swctrlmap4_misc5g_fem7to4", "0", 0 },
	{ "0:maxp2ga0", "96", 0 },
	{ "0:maxp2ga1", "96", 0 },
	{ "0:maxp2ga2", "96", 0 },
	{ "0:maxp2ga3", "96", 0 },
	{ "0:cckbw202gpo", "0x0000", 0 },
	{ "0:cckbw20ul2gpo", "0x0000", 0 },
	{ "0:ofdmlrbw202gpo", "0x6622", 0 },
	{ "0:dot11agofdmhrbw202gpo", "0xaa88", 0 },
	{ "0:mcsbw202gpo", "0xccaa8862", 0 },
	{ "0:mcsbw402gpo", "0xccaa8862", 0 },
	{ "0:mcs1024qam2gpo", "0xeeee", 0 },
	{ "0:dot11agduphrpo", "0", 0 },
	{ "0:dot11agduplrpo", "0", 0 },
	{ "0:mcs10poexp", "0", 0 },
	{ "0:mcs11poexp", "0", 0 },
	{ "0:sb20in40hrpo", "0", 0 },
	{ "0:sb20in80and160hr5glpo", "0", 0 },
	{ "0:sb40and80hr5glpo", "0", 0 },
	{ "0:sb20in80and160hr5gmpo", "0", 0 },
	{ "0:sb40and80hr5gmpo", "0", 0 },
	{ "0:sb20in80and160hr5ghpo", "0", 0 },
	{ "0:sb40and80hr5ghpo", "0", 0 },
	{ "0:sb20in40lrpo", "0", 0 },
	{ "0:sb20in80and160lr5glpo", "0", 0 },
	{ "0:sb40and80lr5glpo", "0", 0 },
	{ "0:sb20in80and160lr5gmpo", "0", 0 },
	{ "0:sb40and80lr5gmpo", "0", 0 },
	{ "0:sb20in80and160lr5ghpo", "0", 0 },
	{ "0:sb40and80lr5ghpo", "0", 0 },
	//{ "0:pa2ga0", "0x1d05,0xd4c7,0x1e54,0x2407", 0 },
	//{ "0:pa2g40a0", "0x1a98,0xe279,0x3583,0x262b", 0 },
	//{ "0:pa2ga1", "0x1ce3,0xd4e9,0x1e27,0x2409", 0 },
	//{ "0:pa2g40a1", "0x1a8a,0xe06c,0x3246,0x260a", 0 },
	//{ "0:pa2ga2", "0x1cf3,0xd56b,0x2017,0x2447", 0 },
	//{ "0:pa2g40a2", "0x1b7b,0xde56,0x2d05,0x254e", 0 },
	//{ "0:pa2ga3", "0x1d10,0xd52a,0x1f11,0x2433", 0 },
	//{ "0:pa2g40a3", "0x1ae4,0xe123,0x35ca,0x2658", 0 },
	{ "0:rxgains2gelnagaina0", "4", 0 },
	{ "0:rxgains2gtrisoa0", "7", 0 },
	{ "0:rxgains2gtrelnabypa0", "1", 0 },
	{ "0:rxgains2gelnagaina1", "4", 0 },
	{ "0:rxgains2gtrisoa1", "7", 0 },
	{ "0:rxgains2gtrelnabypa1", "1", 0 },
	{ "0:rxgains2gelnagaina2", "4", 0 },
	{ "0:rxgains2gtrisoa2", "7", 0 },
	{ "0:rxgains2gtrelnabypa2", "1", 0 },
	{ "0:rxgains2gelnagaina3", "4", 0 },
	{ "0:rxgains2gtrisoa3", "7", 0 },
	{ "0:rxgains2gtrelnabypa3", "1", 0 },
	{ "0:maxp5gb4a0", "100", 0 },
	{ "0:maxp5gb4a1", "100", 0 },
	{ "0:maxp5gb4a2", "100", 0 },
	{ "0:maxp5gb4a3", "100", 0 },
	{ "0:tssifloor5g", "0x3ff,0x1bc,0x3ff,0x3ff", 0 },
	{ "0:subband5gver", "0x5", 0 },
	{ "0:aa5g", "15", 0 },
	{ "0:sar5g", "15", 0 },
	{ "0:noiselvl5ga0", "31,31,31,31", 0 },
	{ "0:noiselvl5ga1", "31,31,31,31", 0 },
	{ "0:noiselvl5ga2", "31,31,31,31", 0 },
	{ "0:noiselvl5ga3", "31,31,31,31", 0 },
	{ "0:rxgainerr5ga0", "63,63,63,63", 0 },
	{ "0:rxgainerr5ga1", "31,31,31,31", 0 },
	{ "0:rxgainerr5ga2", "31,31,31,31", 0 },
	{ "0:rxgainerr5ga3", "31,31,31,31", 0 },
	{ "0:rpcal5gb0", "65535", 0 },
	{ "0:rpcal5gb1", "65535", 0 },
	{ "0:rpcal5gb2", "65535", 0 },
	{ "0:rpcal5gb3", "65535", 0 },
	{ "0:txidxcap5g", "0", 0 },
	{ "0:tssiposslope5g", "1", 0 },
	{ "0:epagain5g", "0", 0 },
	{ "0:pdgain5g", "2", 0 },
	{ "0:tworangetssi5g", "0", 0 },
	{ "0:papdcap5g", "0", 0 },
	{ "0:maxp5gb0a0", "72", 0 },
	{ "0:maxp5gb1a0", "72", 0 },
	{ "0:maxp5gb2a0", "72", 0 },
	{ "0:maxp5gb3a0", "72", 0 },
	{ "0:maxp5gb0a1", "72", 0 },
	{ "0:maxp5gb1a1", "72", 0 },
	{ "0:maxp5gb2a1", "72", 0 },
	{ "0:maxp5gb3a1", "72", 0 },
	{ "0:maxp5gb0a2", "72", 0 },
	{ "0:maxp5gb1a2", "72", 0 },
	{ "0:maxp5gb2a2", "72", 0 },
	{ "0:maxp5gb3a2", "72", 0 },
	{ "0:maxp5gb0a3", "72", 0 },
	{ "0:maxp5gb1a3", "72", 0 },
	{ "0:maxp5gb2a3", "72", 0 },
	{ "0:maxp5gb3a3", "72", 0 },
	{ "0:pa5ga0", "0x1cd8,0xd5ab,0x2104,0x23e7,0x1be8,0xc593,0xfd93,0x1,0x1c55,0xc466,0xff0e,0xc2e,0x1c6c,0xc488,0xfedf,0xa95,0x1cb8,0xc426,0xff5e,0xf6b", 0 },
	{ "0:pa5g40a0", "0x1d24,0xd717,0x22f8,0x23fe,0x1c18,0xd4bc,0xf510,0x142,0x1c26,0xd57c,0xf51a,0x2e1,0x1c40,0xd556,0xf556,0x59f,0x1c37,0xd70b,0xf46c,0x1b0", 0 },
	{ "0:pa5gbw40a0", "0x0,0x0,0x0,0x1111,0x1111,0x1111,0x1111,0xffff,0xffff,0xffff,0xffff,0xffff", 0 },
	{ "0:pa5gbw80a0", "0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff", 0 },
	{ "0:pa5gbw4080a0", "0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff", 0 },
	{ "0:pa5g80a0", "0x1b7b,0xd023,0xf801,0x0,0x1b7d,0xcfdd,0xf833,0x0,0x1bee,0xcc6e,0xfa82,0xac2,0x1bfe,0xd091,0xf8f8,0xf5c,0x1c2c,0xd035,0xf934,0x108f", 0 },
	{ "0:pa5ga1", "0x1cab,0xd612,0x20a4,0x23d6,0x1b8a,0xc732,0xfd18,0x12d,0x1c44,0xc3c3,0xff8d,0xce1,0x1c76,0xc38e,0xfffc,0x1292,0x1cb0,0xc3b1,0x0,0x13c4", 0 },
	{ "0:pa5g40a1", "0x1ceb,0xd6cd,0x2227,0x23ee,0x1c1d,0xd35a,0xf5b7,0x0,0x1c54,0xd392,0xf64d,0x664,0x1c62,0xd402,0xf640,0x82f,0x1c76,0xd486,0xf60e,0x76d", 0 },
	{ "0:pa5gbw4080a1", "0xffff,0x7bde,0xa9bc,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff", 0 },
	{ "0:pa5g80a1", "0x1b63,0xce8d,0xf8f7,0x0,0x1b5d,0xcedf,0xf8e4,0x0,0x1bd7,0xcbdc,0xfb03,0xb9f,0x1c19,0xcf5b,0xf9c5,0x1067,0x1c31,0xcf44,0xf9dd,0x11c3", 0 },
	{ "0:pa5ga2", "0x1cc5,0xd58b,0x2128,0x23e8,0x1beb,0xc321,0xfeff,0x0,0x1ba1,0xc7f3,0xfd06,0x131,0x1bbf,0xc844,0xfce8,0x132,0x1c9b,0xc2fe,0xffff,0xd50", 0 },
	{ "0:pa5g40a2", "0x1d18,0xd6dd,0x233a,0x240f,0x1c14,0xd2d1,0xf5df,0x0,0x1c35,0xd395,0xf5dc,0xc2,0x1c3d,0xd44d,0xf5ab,0x259,0x1c78,0xd4b4,0xf597,0x329", 0 },
	{ "0:pa5g80a2", "0x1b5c,0xce4b,0xf8f3,0x0,0x1b7a,0xcda7,0xf96d,0x0,0x1bc7,0xcbc4,0xfa89,0x1fc,0x1c15,0xce78,0xfa0e,0xee7,0x1c21,0xcf67,0xf975,0xc6f", 0 },
	{ "0:pa5ga3", "0x1ce2,0xd631,0x2166,0x23ed,0x1924,0xc6f7,0xfdc6,0x0,0x1936,0xc789,0xfdd5,0x0,0x196d,0xc732,0xfe05,0x0,0x19b0,0xc669,0xfe52,0x0", 0 },
	{ "0:pa5g40a3", "0x1d3e,0xd6b3,0x226f,0x23ec,0x1972,0xd7d6,0xf3fc,0x0,0x1988,0xd856,0xf41c,0x0,0x19af,0xd80e,0xf451,0x0,0x1a00,0xd743,0xf494,0x0", 0 },
	{ "0:pa5g80a3", "0x1933,0xd0fe,0xf81f,0x0,0x1937,0xd10e,0xf849,0x0,0x1924,0xd27b,0xf7e7,0x0,0x1974,0xd21c,0xf81a,0x0,0x1994,0xd202", 0 },
	{ "0:rxgains5gelnagaina0", "1", 0 },
	{ "0:rxgains5gtrisoa0", "5", 0 },
	{ "0:rxgains5gtrelnabypa0", "1", 0 },
	{ "0:rxgains5gelnagaina1", "1", 0 },
	{ "0:rxgains5gtrisoa1", "5", 0 },
	{ "0:rxgains5gtrelnabypa1", "1", 0 },
	{ "0:rxgains5gelnagaina2", "1", 0 },
	{ "0:rxgains5gtrisoa2", "5", 0 },
	{ "0:rxgains5gtrelnabypa2", "1", 0 },
	{ "0:mcsbw205glpo", "2573623296", 0 },
	{ "0:mcsbw405glpo", "2573623842", 0 },
	{ "0:mcsbw805glpo", "2573623842", 0 },
	{ "0:mcsbw205gmpo", "2573623296", 0 },
	{ "0:mcsbw405gmpo", "2573623842", 0 },
	{ "0:mcsbw805gmpo", "2573624388", 0 },
	{ "0:mcsbw205ghpo", "2858835968", 0 },
	{ "0:mcsbw405ghpo", "2858836514", 0 },
	{ "0:mcsbw805ghpo", "2858837060", 0 },
	{ "0:mcslr5glpo", "0", 0 },
	{ "0:mcslr5gmpo", "0", 0 },
	{ "0:mcslr5ghpo", "0", 0 },
	{ "0:ulbpdoffs5gb0a0", "65535", 0 },
	{ "0:ulbpdoffs5gb0a1", "65535", 0 },
	{ "0:ulbpdoffs5gb0a2", "65535", 0 },
	{ "0:ulbpdoffs5gb0a3", "65535", 0 },
	{ "0:ulbpdoffs5gb1a0", "65535", 0 },
	{ "0:ulbpdoffs5gb1a1", "65535", 0 },
	{ "0:ulbpdoffs5gb1a2", "65535", 0 },
	{ "0:ulbpdoffs5gb1a3", "65535", 0 },
	{ "0:ulbpdoffs5gb2a0", "65535", 0 },
	{ "0:ulbpdoffs5gb2a1", "65535", 0 },
	{ "0:ulbpdoffs5gb2a2", "65535", 0 },
	{ "0:ulbpdoffs5gb2a3", "65535", 0 },
	{ "0:ulbpdoffs5gb3a0", "65535", 0 },
	{ "0:ulbpdoffs5gb3a1", "65535", 0 },
	{ "0:ulbpdoffs5gb3a2", "65535", 0 },
	{ "0:ulbpdoffs5gb3a3", "65535", 0 },
	{ "0:ulbpdoffs5gb4a0", "65535", 0 },
	{ "0:ulbpdoffs5gb4a1", "65535", 0 },
	{ "0:ulbpdoffs5gb4a2", "65535", 0 },
	{ "0:ulbpdoffs5gb4a3", "65535", 0 },
	{ "0:mcs1024qam5glpo", "4294967295", 0 },
	{ "0:mcs1024qam5gmpo", "4294967295", 0 },
	{ "0:mcs1024qam5ghpo", "4294967295", 0 },
	{ "0:mcs1024qam5gx1po", "4294967295", 0 },
	{ "0:mcs1024qam5gx2po", "4294967295", 0 },
	{ "0:mcsbw1605glpo", "4294967295", 0 },
	{ "0:mcsbw1605gmpo", "4294967295", 0 },
	{ "0:mcsbw1605ghpo", "4294967295", 0 },
	{ "0:mcsbw1605gx1po", "4294967295", 0 },
	{ "0:mcsbw1605gx2po", "4294967295", 0 },
	{ "0:pdoffset20in40m5gb0", "65535", 0 },
	{ "0:pdoffset20in40m5gb1", "65535", 0 },
	{ "0:pdoffset20in40m5gb2", "65535", 0 },
	{ "0:pdoffset20in40m5gb3", "65535", 0 },
	{ "0:pdoffset20in40m5gb4", "65535", 0 },
	{ "0:pdoffset40in80m5gb0", "65535", 0 },
	{ "0:pdoffset40in80m5gb1", "65535", 0 },
	{ "0:pdoffset40in80m5gb2", "65535", 0 },
	{ "0:pdoffset40in80m5gb3", "65535", 0 },
	{ "0:pdoffset40in80m5gb4", "65535", 0 },
	{ "0:pdoffset20in80m5gb0", "28539", 0 },
	{ "0:pdoffset20in80m5gb1", "28539", 0 },
	{ "0:pdoffset20in80m5gb2", "28539", 0 },
	{ "0:pdoffset20in80m5gb3", "28539", 0 },
	{ "0:pdoffset20in80m5gb4", "28539", 0 },
	{ "0:pdoffset20in40m5gcore3_1", "65535", 0 },
	{ "0:pdoffset20in80m5gcore3_1", "65535", 0 },
	{ "0:pdoffset40in80m5gcore3_1", "65535", 0 },
	{ "0:mcsbw205gx1po", "2858835968", 0 },
	{ "0:mcsbw405gx1po", "2858836514", 0 },
	{ "0:mcsbw805gx1po", "2858837060", 0 },
	{ "0:mcslr5gx1po", "0", 0 },
	{ "0:sb20in80and160hr5gx1po", "0", 0 },
	{ "0:sb40and80hr5gx1po", "0", 0 },
	{ "0:mcsbw205gx2po", "2858835968", 0 },
	{ "0:mcsbw405gx2po", "2858836514", 0 },
	{ "0:mcsbw805gx2po", "2858837060", 0 },
	{ "0:mcslr5gx2po", "0", 0 },
	{ "0:sb40and80lr5gx2po", "0", 0 },
	{ "0:sb20in80and160hr5gx2po", "0", 0 },
	{ "0:sb40and80hr5gx2po", "0", 0 },
	{ "0:sb20in80and160lr5gx2po", "0", 0 },
	{ "0:rxgains5gmelnagaina0", "4", 0 },
	{ "0:rxgains5gmelnagaina1", "4", 0 },
	{ "0:rxgains5gmelnagaina2", "4", 0 },
	{ "0:rxgains5gmtrisoa0", "7", 0 },
	{ "0:rxgains5gmtrisoa1", "7", 0 },
	{ "0:rxgains5gmtrisoa2", "7", 0 },
	{ "0:rxgains5gmtrelnabypa0", "1", 0 },
	{ "0:rxgains5gmtrelnabypa1", "1", 0 },
	{ "0:rxgains5gmtrelnabypa2", "1", 0 },
	{ "0:rxgains5ghelnagaina0", "1", 0 },
	{ "0:rxgains5ghelnagaina1", "1", 0 },
	{ "0:rxgains5ghelnagaina2", "1", 0 },
	{ "0:rxgains5ghtrisoa0", "5", 0 },
	{ "0:rxgains5ghtrisoa1", "5", 0 },
	{ "0:rxgains5ghtrisoa2", "5", 0 },
	{ "0:rxgains5ghtrelnabypa0", "1", 0 },
	{ "0:rxgains5ghtrelnabypa1", "1", 0 },
	{ "0:rxgains5ghtrelnabypa2", "1", 0 },
	{ "0:eu_edthresh5g", "255", 0 },
	{ "0:rxgains5gmelnagaina3", "4", 0 },
	{ "0:rxgains5gmtrisoa3", "7", 0 },
	{ "0:rxgains5gmtrelnabypa3", "1", 0 },
	{ "0:rxgains5ghelnagaina3", "1", 0 },
	{ "0:rxgains5ghtrisoa3", "5", 0 },
	{ "0:rxgains5ghtrelnabypa3", "1", 0 },
	{ "0:pdoffset20in40m5gcore3", "4294967295", 0 },
	{ "0:pdoffset40in80m5gcore3", "4294967295", 0 },
	{ "0:pdoffset20in80m5gcore3", "4294967295", 0 },
	{ "devpath1", "pci/2/1", 0 },
	{ "1:sromrev", "13", 0 },
	{ "1:boardflags", "0x10001000", 0 },
	{ "1:boardflags2", "0x4", 0 },
	{ "1:boardflags3", "0x0", 0 },
	{ "1:boardflags4", "0xe", 0 },
	{ "1:devid", "0x43c5", 0 },
	{ "1:subvid", "0x14e4", 0 },
	{ "1:boardnum", "53259", 0 },
	//{ "1:macaddr", "00:90:4c:1f:d0:0b", 0 },
	//{ "1:ccode", "ALL", 0 },
	//{ "1:regrev", "0", 0 },
	{ "1:aa5g", "15", 0 },
	{ "1:agbg0", "133", 0 },
	{ "1:agbg1", "133", 0 },
	{ "1:agbg2", "133", 0 },
	{ "1:aga0", "71", 0 },
	{ "1:aga1", "71", 0 },
	{ "1:aga2", "71", 0 },
	{ "1:antswitch", "0", 0 },
	{ "1:femctrl", "2", 0 },
	{ "1:tssiposslope5g", "1", 0 },
	{ "1:epagain5g", "0", 0 },
	{ "1:pdgain5g", "2", 0 },
	{ "1:tworangetssi5g", "0", 0 },
	{ "1:papdcap5g", "0", 0 },
	{ "1:gainctrlsph", "0", 0 },
	{ "1:tempthresh", "120", 0 },
	{ "1:tempoffset", "255", 0 },
	{ "1:rawtempsense", "0x1ff", 0 },
	{ "1:measpower", "0x7f", 0 },
	{ "1:tempsense_slope", "0xff", 0 },
	{ "1:tempcorrx", "0x3f", 0 },
	{ "1:tempsense_option", "0x3", 0 },
	{ "1:xtalfreq", "40000", 0 },
	{ "1:phycal_tempdelta", "40", 0 },
	{ "1:temps_period", "1", 0 },
	{ "1:temps_hysteresis", "5", 0 },
	{ "1:measpower1", "0x7f", 0 },
	{ "1:measpower2", "0x7e", 0 },
	{ "1:tssifloor5g", "0x0,0x3bb,0x3bd,0x3bd", 0 },
	{ "1:pdoffset40ma0", "46011", 0 },
	{ "1:pdoffset40ma1", "46011", 0 },
	{ "1:pdoffset40ma2", "46011", 0 },
	{ "1:pdoffset80ma0", "46011", 0 },
	{ "1:pdoffset80ma1", "1058", 0 },
	{ "1:pdoffset80ma2", "65", 0 },
	{ "1:subband5gver", "0x5", 0 },
	{ "1:paparambwver", "0", 0 },
	{ "1:cckbw202gpo", "0", 0 },
	{ "1:cckbw20ul2gpo", "0", 0 },
	{ "1:mcsbw202gpo", "0", 0 },
	{ "1:mcsbw402gpo", "0", 0 },
	{ "1:dot11agofdmhrbw202gpo", "0", 0 },
	{ "1:ofdmlrbw202gpo", "0", 0 },
	{ "1:sb20in40lrpo", "0", 0 },
	{ "1:sb20in80and160lr5glpo", "0", 0 },
	{ "1:sb40and80lr5glpo", "0", 0 },
	{ "1:sb20in80and160lr5gmpo", "0", 0 },
	{ "1:sb40and80lr5gmpo", "0", 0 },
	{ "1:sb20in80and160lr5ghpo", "0", 0 },
	{ "1:sb40and80lr5ghpo", "0", 0 },
	{ "1:dot11agduphrpo", "0", 0 },
	{ "1:dot11agduplrpo", "0", 0 },
	{ "1:sar5g", "15", 0 },
	{ "1:noiselvl5ga0", "31,31,31,31", 0 },
	{ "1:noiselvl5ga1", "31,31,31,31", 0 },
	{ "1:noiselvl5ga2", "31,31,31,31", 0 },
	//{ "1:rpcal5gb0", "0", 0 },
	//{ "1:rpcal5gb1", "0", 0 },
	//{ "1:rpcal5gb2", "0", 0 },
	//{ "1:rpcal5gb3", "0", 0 },
	{ "1:txidxcap5g", "0", 0 },
	//{ "1:rpcal5gb4", "0", 0 },
	{ "1:sb20in40hrlrpox", "0", 0 },
	{ "1:pdoffsetcckma0", "0", 0 },
	{ "1:pdoffsetcckma1", "0", 0 },
	{ "1:pdoffsetcckma2", "0", 0 },
	{ "1:pdoffsetcck", "65535", 0 },
	{ "1:pdoffset20in40m5gb0", "31677", 0 },
	{ "1:pdoffset20in40m5gb1", "31677", 0 },
	{ "1:pdoffset20in40m5gb2", "30653", 0 },
	{ "1:pdoffset20in40m5gb3", "30653", 0 },
	{ "1:pdoffset20in40m5gb4", "30653", 0 },
	{ "1:pdoffset40in80m5gb0", "0", 0 },
	{ "1:pdoffset40in80m5gb1", "1056", 0 },
	{ "1:pdoffset40in80m5gb2", "1088", 0 },
	{ "1:pdoffset40in80m5gb3", "2080", 0 },
	{ "1:pdoffset40in80m5gb4", "2113", 0 },
	{ "1:pdoffset20in80m5gb0", "31709", 0 },
	{ "1:pdoffset20in80m5gb1", "32767", 0 },
	{ "1:pdoffset20in80m5gb2", "31710", 0 },
	{ "1:pdoffset20in80m5gb3", "31710", 0 },
	{ "1:pdoffset20in80m5gb4", "31710", 0 },
	{ "1:pdoffset20in40m5gcore3_1", "957", 0 },
	{ "1:pdoffset20in80m5gcore3_1", "990", 0 },
	{ "1:pdoffset40in80m5gcore3_1", "65", 0 },
	{ "1:pdoffset20in40m5gcore3", "62748605", 0 },
	{ "1:pdoffset40in80m5gcore3", "4260898", 0 },
	{ "1:pdoffset20in80m5gcore3", "64912382", 0 },
	{ "1:mcsbw205gx1po", "4001923616", 0 },
	{ "1:mcsbw405gx1po", "4001923616", 0 },
	//{ "1:mcsbw805gx1po", "4001923616", 0 },
	{ "1:mcsbw205gx2po", "4001923616", 0 },
	{ "1:mcsbw405gx2po", "4001923616", 0 },
	//{ "1:mcsbw805gx2po", "4001923616", 0 },
	{ "1:mcslr5gx1po", "0", 0 },
	{ "1:sb20in80and160hr5gx1po", "0", 0 },
	{ "1:sb40and80hr5gx1po", "0", 0 },
	{ "1:mcslr5gx2po", "0", 0 },
	{ "1:sb40and80lr5gx2po", "0", 0 },
	{ "1:sb20in80and160hr5gx2po", "0", 0 },
	{ "1:sb40and80hr5gx2po", "0", 0 },
	{ "1:sb20in80and160lr5gx2po", "0", 0 },
	{ "1:gpdn", "0xffffffff", 0 },
	{ "1:eu_edthresh5g", "255", 0 },
	{ "1:agbg3", "5", 0 },
	{ "1:aga3", "133", 0 },
	{ "1:noiselvl5ga3", "31,31,31,31", 0 },
	{ "1:ulbpdoffs5gb0a0", "65535", 0 },
	{ "1:ulbpdoffs5gb0a1", "65535", 0 },
	{ "1:ulbpdoffs5gb0a2", "65535", 0 },
	{ "1:ulbpdoffs5gb0a3", "65535", 0 },
	{ "1:ulbpdoffs5gb1a0", "65535", 0 },
	{ "1:ulbpdoffs5gb1a1", "65535", 0 },
	{ "1:ulbpdoffs5gb1a2", "65535", 0 },
	{ "1:ulbpdoffs5gb1a3", "65535", 0 },
	{ "1:ulbpdoffs5gb2a0", "65535", 0 },
	{ "1:ulbpdoffs5gb2a1", "65535", 0 },
	{ "1:ulbpdoffs5gb2a2", "65535", 0 },
	{ "1:ulbpdoffs5gb2a3", "65535", 0 },
	{ "1:ulbpdoffs5gb3a0", "65535", 0 },
	{ "1:ulbpdoffs5gb3a1", "65535", 0 },
	{ "1:ulbpdoffs5gb3a2", "65535", 0 },
	{ "1:ulbpdoffs5gb3a3", "65535", 0 },
	{ "1:ulbpdoffs5gb4a0", "65535", 0 },
	{ "1:ulbpdoffs5gb4a1", "65535", 0 },
	{ "1:ulbpdoffs5gb4a2", "65535", 0 },
	{ "1:ulbpdoffs5gb4a3", "65535", 0 },
	{ "1:swctrlmap4_cfg", "1", 0 },
	{ "1:swctrlmap4_TX2g_fem3to0", "0", 0 },
	{ "1:swctrlmap4_RX2g_fem3to0", "0", 0 },
	{ "1:swctrlmap4_RXByp2g_fem3to0", "0", 0 },
	{ "1:swctrlmap4_misc2g_fem3to0", "0", 0 },
	{ "1:swctrlmap4_TX5g_fem3to0", "26214", 0 },
	{ "1:swctrlmap4_RX5g_fem3to0", "4369", 0 },
	{ "1:swctrlmap4_RXByp5g_fem3to0", "13107", 0 },
	{ "1:swctrlmap4_misc5g_fem3to0", "0", 0 },
	{ "1:swctrlmap4_TX2g_fem7to4", "0", 0 },
	{ "1:swctrlmap4_RX2g_fem7to4", "0", 0 },
	{ "1:swctrlmap4_RXByp2g_fem7to4", "0", 0 },
	{ "1:swctrlmap4_misc2g_fem7to4", "0", 0 },
	{ "1:swctrlmap4_TX5g_fem7to4", "0", 0 },
	{ "1:swctrlmap4_RX5g_fem7to4", "0", 0 },
	{ "1:swctrlmap4_RXByp5g_fem7to4", "0", 0 },
	{ "1:swctrlmap4_misc5g_fem7to4", "0", 0 },
	{ "1:maxp5gb0a0", "96", 0 },
	{ "1:maxp5gb1a0", "96", 0 },
	{ "1:maxp5gb2a0", "96", 0 },
	{ "1:maxp5gb3a0", "96", 0 },
	{ "1:maxp5gb4a0", "96", 0 },
	{ "1:maxp5gb0a1", "96", 0 },
	{ "1:maxp5gb1a1", "96", 0 },
	{ "1:maxp5gb2a1", "96", 0 },
	{ "1:maxp5gb3a1", "96", 0 },
	{ "1:maxp5gb4a1", "96", 0 },
	{ "1:maxp5gb0a2", "96", 0 },
	{ "1:maxp5gb1a2", "96", 0 },
	{ "1:maxp5gb2a2", "96", 0 },
	{ "1:maxp5gb3a2", "96", 0 },
	{ "1:maxp5gb4a2", "96", 0 },
	{ "1:maxp5gb0a3", "96", 0 },
	{ "1:maxp5gb1a3", "96", 0 },
	{ "1:maxp5gb2a3", "96", 0 },
	{ "1:maxp5gb3a3", "96", 0 },
	{ "1:maxp5gb4a3", "96", 0 },
	{ "1:mcsbw205glpo", "0xccaa8862", 0 },
	{ "1:mcsbw405glpo", "0xccaa8862", 0 },
	//{ "1:mcsbw805glpo", "0xccaa8862", 0 },
	{ "1:mcsbw1605glpo", "0xccaa8862", 0 },
	{ "1:mcs1024qam5glpo", "0xeeeeeeee", 0 },
	{ "1:mcslr5glpo", "0x4444", 0 },
	{ "1:mcsbw205gmpo", "0xccaa8862", 0 },
	{ "1:mcsbw405gmpo", "0xccaa8862", 0 },
	//{ "1:mcsbw805gmpo", "0xccaa8862", 0 },
	{ "1:mcsbw1605gmpo", "0xccaa8862", 0 },
	{ "1:mcs1024qam5gmpo", "0xeeeeeeee", 0 },
	{ "1:mcslr5gmpo", "0x4444", 0 },
	{ "1:mcsbw205ghpo", "0xccaa8862", 0 },
	{ "1:mcsbw405ghpo", "0xccaa8862", 0 },
	//{ "1:mcsbw805ghpo", "0xccaa8862", 0 },
	{ "1:mcsbw1605ghpo", "0xccaa8862", 0 },
	{ "1:mcsbw1605gx1po", "0xccaa8862", 0 },
	{ "1:mcsbw1605gx2po", "0xccaa8862", 0 },
	{ "1:mcs1024qam5ghpo", "0xeeeeeeee", 0 },
	{ "1:mcs1024qam5gx1po", "0xeeeeeeee", 0 },
	{ "1:mcs1024qam5gx2po", "0xeeeeeeee", 0 },
	{ "1:mcslr5ghpo", "0x4444", 0 },
	{ "1:mcs8poexp", "0", 0 },
	{ "1:mcs9poexp", "0", 0 },
	{ "1:mcs10poexp", "0", 0 },
	{ "1:mcs11poexp", "0", 0 },
	{ "1:sb20in40hrpo", "0", 0 },
	{ "1:sb20in80and160hr5glpo", "0", 0 },
	{ "1:sb40and80hr5glpo", "0", 0 },
	{ "1:sb20in80and160hr5gmpo", "0", 0 },
	{ "1:sb40and80hr5gmpo", "0", 0 },
	{ "1:sb20in80and160hr5ghpo", "0", 0 },
	{ "1:sb40and80hr5ghpo", "0", 0 },
	//{ "1:pa5ga0", "0x1e2f,0xc9a2,0xc9a2,0xc9a2,0x1e28,0xc89f,0x2369,0x2393,0x1e19,0xc94b,0x25b1,0x240e,0x1de8,0xc6a4,0x1e89,0x22e2,0x1dbc,0xc54f,0x1e4c,0x2304", 0 },
	//{ "1:pa5g40a0", "0x1f17,0xd2ac,0x3aec,0x25fc,0x1f24,0xd3fb,0x3ded,0x2654,0x1edc,0xd851,0x487d,0x275a,0x1ed3,0xd46f,0x3ff4,0x2696,0x1ea9,0xd358,0x41bf,0x26ee", 0 },
	//{ "1:pa5g80a0", "0x1fc5,0xdda2,0x4b70,0x26f9,0x1fa8,0xe03a,0x50a1,0x2772,0x1f93,0xdfd1,0x50cf,0x2779,0x1f8d,0xdd8f,0x4d83,0x2765,0x1f61,0xdb09,0x4aa3,0x273f", 0 },
	//{ "1:pa5ga1", "0x1dd7,0xc766,0x221f,0x237e,0x1dc4,0xc7f8,0x22fd,0x23a5,0x1dc9,0xc860,0x2325,0x23a8,0x1da0,0xc6c8,0x1e8d,0x22df,0x1d8e,0xc65b,0x1f2f,0x232c", 0 },
	//{ "1:pa5g40a1", "0x1e82,0xd57a,0x43a1,0x270e,0x1e87,0xd71a,0x4681,0x273f,0x1e78,0xd697,0x42c6,0x26d8,0x1e72,0xd394,0x3cab,0x265a,0x1e6f,0xd116,0x37c0,0x25e3", 0 },
	//{ "1:pa5g80a1", "0x1f01,0xdfcd,0x522d,0x27af,0x1f18,0xdd68,0x4c14,0x273e,0x1ef2,0xde6b,0x4ee8,0x278a,0x1f0b,0xdaf3,0x471b,0x26fe,0x1ef9,0xd83d,0x431c,0x26c7", 0 },
	//{ "1:pa5ga2", "0x1e2b,0xc851,0x24f1,0x23d3,0x1e31,0xc9ff,0x27fd,0x2451,0x1e42,0xca20,0x2791,0x2437,0x1e13,0xc8d1,0x242c,0x23ac,0x1dda,0xc938,0x26df,0x2427", 0 },
	//{ "1:pa5g40a2", "0x1ec0,0xdac4,0x50b3,0x2807,0x1ef6,0xda38,0x4d94,0x27bd,0x1ec4,0xddc6,0x543b,0x281a,0x1ede,0xd8e1,0x4a5d,0x276d,0x1efc,0xd0de,0x3889,0x25c5", 0 },
	//{ "1:pa5g80a2", "0x1f9d,0xe18b,0x51ae,0x2754,0x1fbf,0xdf13,0x4b5e,0x26e1,0x1f87,0xe071,0x4dd6,0x270b,0x1fa4,0xdf99,0x4f1c,0x2744,0x1fbf,0xd8f1,0x4323,0x2673", 0 },
	//{ "1:pa5ga3", "0x1e2c,0xc9e2,0x282d,0x245c,0x1e2e,0xcb5b,0x2a14,0x2498,0x1e3e,0xca6e,0x27aa,0x2433,0x1e0b,0xc835,0x23aa,0x23bf,0x1dcd,0xc6d0,0x221f,0x2395", 0 },
	//{ "1:pa5g40a3", "0x1ed4,0xd999,0x4c9c,0x27be,0x1ee9,0xdb17,0x4e7c,0x27d1,0x1ee8,0xda30,0x4b75,0x276c,0x1ef6,0xd489,0x3f53,0x2671,0x1eeb,0xcf2e,0x3527,0x25a1", 0 },
	//{ "1:pa5g80a3", "0x1fac,0xe182,0x5211,0x276c,0x1f4c,0xe7be,0x5d6d,0x2843,0x1f8f,0xe325,0x5480,0x2791,0x1fb0,0xde51,0x4c9e,0x2733,0x1e50,0xda8b,0x490b,0x2703", 0 },
	{ "1:rxgains5gelnagaina0", "3", 0 },
	{ "1:rxgains5gelnagaina1", "3", 0 },
	{ "1:rxgains5gelnagaina2", "3", 0 },
	{ "1:rxgains5gelnagaina3", "3", 0 },
	{ "1:rxgains5gtrisoa0", "6", 0 },
	{ "1:rxgains5gtrisoa1", "6", 0 },
	{ "1:rxgains5gtrisoa2", "6", 0 },
	{ "1:rxgains5gtrisoa3", "6", 0 },
	{ "1:rxgains5gtrelnabypa0", "1", 0 },
	{ "1:rxgains5gtrelnabypa1", "1", 0 },
	{ "1:rxgains5gtrelnabypa2", "1", 0 },
	{ "1:rxgains5gtrelnabypa3", "1", 0 },
	{ "1:rxgains5gmelnagaina0", "3", 0 },
	{ "1:rxgains5gmelnagaina1", "3", 0 },
	{ "1:rxgains5gmelnagaina2", "3", 0 },
	{ "1:rxgains5gmelnagaina3", "3", 0 },
	{ "1:rxgains5gmtrisoa0", "7", 0 },
	{ "1:rxgains5gmtrisoa1", "7", 0 },
	{ "1:rxgains5gmtrisoa2", "7", 0 },
	{ "1:rxgains5gmtrisoa3", "7", 0 },
	{ "1:rxgains5gmtrelnabypa0", "1", 0 },
	{ "1:rxgains5gmtrelnabypa1", "1", 0 },
	{ "1:rxgains5gmtrelnabypa2", "1", 0 },
	{ "1:rxgains5gmtrelnabypa3", "1", 0 },
	{ "1:rxgains5ghelnagaina0", "3", 0 },
	{ "1:rxgains5ghelnagaina1", "3", 0 },
	{ "1:rxgains5ghelnagaina2", "3", 0 },
	{ "1:rxgains5ghelnagaina3", "3", 0 },
	{ "1:rxgains5ghtrisoa0", "6", 0 },
	{ "1:rxgains5ghtrisoa1", "6", 0 },
	{ "1:rxgains5ghtrisoa2", "6", 0 },
	{ "1:rxgains5ghtrisoa3", "6", 0 },
	{ "1:rxgains5ghtrelnabypa0", "1", 0 },
	{ "1:rxgains5ghtrelnabypa1", "1", 0 },
	{ "1:rxgains5ghtrelnabypa2", "1", 0 },
	{ "1:rxgains5ghtrelnabypa3", "1", 0 },
	//{ "1:rxgainerr5ga0", "63,63,63,63", 0 },
	//{ "1:rxgainerr5ga1", "31,31,31,31", 0 },
	//{ "1:rxgainerr5ga2", "31,31,31,31", 0 },
	//{ "1:rxgainerr5ga3", "31,31,31,31", 0 },
	{ "1:ulbpdoffs2ga0", "65535", 0 },
	{ "1:ulbpdoffs2ga1", "65535", 0 },
	{ "1:ulbpdoffs2ga2", "65535", 0 },
	{ "1:ulbpdoffs2ga3", "65535", 0 },
	{ "1:pdoffset20in40m2g", "65535", 0 },
	{ "1:pdoffset20in40m2gcore3", "65535", 0 },
	{ "1:eu_edthresh2g", "255", 0 },
	{ "1:rxgainerr2ga3", "31", 0 },
	{ "1:mcs1024qam2gpo", "0", 0 },
	{ "1:ulbpproffs2g", "65535", 0 },
	{ "1:sar2g", "18", 0 },
	{ "1:noiselvl2ga0", "31", 0 },
	{ "1:noiselvl2ga1", "31", 0 },
	{ "1:noiselvl2ga2", "31", 0 },
	{ "1:rxgainerr2ga0", "63", 0 },
	{ "1:rxgainerr2ga1", "31", 0 },
	{ "1:rxgainerr2ga2", "31", 0 },
	{ "1:rpcal2g", "0", 0 },
	{ "1:txidxcap2g", "0", 0 },
	{ "1:tssifloor2g", "0x3ff", 0 },
	{ "1:pdoffset2g40ma0", "5", 0 },
	{ "1:pdoffset2g40ma1", "8", 0 },
	{ "1:pdoffset2g40ma2", "5", 0 },
	{ "1:pdoffset2g40mvalid", "0", 0 },
	{ "1:pa2gccka0", "0xffff,0xffff,0xffff", 0 },
	{ "1:aa2g", "15", 0 },
	{ "1:tssiposslope2g", "1", 0 },
	{ "1:epagain2g", "0", 0 },
	{ "1:pdgain2g", "2", 0 },
	{ "1:tworangetssi2g", "0", 0 },
	{ "1:papdcap2g", "0", 0 },
	{ "1:maxp2ga1", "108", 0 },
	{ "1:maxp2ga0", "108", 0 },
	{ "1:maxp2ga2", "108", 0 },
	{ "1:maxp2ga3", "108", 0 },
	{ "1:pa2ga0", "0x1e5f,0xbd9b,0x0,0x0", 0 },
	//{ "1:pa2g40a0", "0xffff,0xffff,0xffff,0xffff", 0 },
	{ "1:pa2ga1", "0x1e02,0xbe29,0x0,0x0", 0 },
	//{ "1:pa2g40a1", "0xffff,0xffff,0xffff,0xffff", 0 },
	{ "1:pa2ga2", "0x1de0,0xbe0e,0x0,0x0", 0 },
	//{ "1:pa2g40a2", "0xffff,0xffff,0xffff,0xffff", 0 },
	{ "1:pa2ga3", "0x1bf7,0xbf37,0x0,0x0", 0 },
	//{ "1:pa2g40a3", "0x1d6b,0xde8e,0x2fcb,0x25d7", 0 },
	{ "1:rxgains2gelnagaina0", "3", 0 },
	{ "1:rxgains2gtrisoa0", "7", 0 },
	{ "1:rxgains2gtrelnabypa0", "1", 0 },
	{ "1:rxgains2gelnagaina3", "3", 0 },
	{ "1:rxgains2gtrisoa3", "7", 0 },
	{ "1:rxgains2gtrelnabypa3", "1", 0 },
	{ "1:rxgains2gelnagaina1", "3", 0 },
	{ "1:rxgains2gtrisoa1", "7", 0 },
	{ "1:rxgains2gtrelnabypa1", "1", 0 },
	{ "1:rxgains2gelnagaina2", "3", 0 },
	{ "1:rxgains2gtrisoa2", "7", 0 },
	{ "1:rxgains2gtrelnabypa2", "1", 0 },
	{ "1:noiselvl2ga3", "31", 0 },
#endif
	{ 0, 0, 0 }
};
#elif defined(RTAC5300)
struct nvram_tuple bcm4360ac_defaults[] = {
#if 0
	{ "devpath0", "pcie/1/3", 0 },
	{ "0:boardvendor", "0x14e4", 0 },
	{ "0:venid", "0x14e4", 0 },
	{ "0:devid", "0x43c4", 0 },
	{ "0:sromrev", "13", 0 },
	{ "0:boardflags", "0x00001000", 0 },
	{ "0:boardflags2", "0x4", 0 },
	{ "0:boardflags3", "0x0", 0 },
	{ "0:boardflags4", "0xe", 0 },
	{ "0:aa2g", "15", 0 },
	{ "0:agbg0", "133", 0 },
	{ "0:agbg1", "133", 0 },
	{ "0:agbg2", "133", 0 },
	{ "0:agbg3", "133", 0 },
	{ "0:aga0", "71", 0 },
	{ "0:aga1", "71", 0 },
	{ "0:aga2", "71", 0 },
	{ "0:aga3", "71", 0 },
	{ "0:txchain", "0xf", 0 },
	{ "0:rxchain", "0xf", 0 },
	{ "0:antswitch", "0", 0 },
	{ "0:femctrl", "2", 0 },
	{ "0:tssiposslope2g", "1", 0 },
	{ "0:epagain2g", "0", 0 },
	{ "0:pdgain2g", "2", 0 },
	{ "0:tworangetssi2g", "0", 0 },
	{ "0:papdcap2g", "0", 0 },
	{ "0:gainctrlsph", "0", 0 },
	{ "0:tempthresh", "120", 0 },
	{ "0:tempoffset", "255", 0 },
	{ "0:rawtempsense", "0x1ff", 0 },
	{ "0:tempsense_slope", "0xff", 0 },
	{ "0:tempcorrx", "0x3f", 0 },
	{ "0:tempsense_option", "0x3", 0 },
	{ "0:phycal_tempdelta", "40", 0 },
	{ "0:temps_period", "10", 0 },
	{ "0:temps_hysteresis", "5", 0 },
	{ "0:xtalfreq", "40000", 0 },
	{ "0:measpower", "0x7f", 0 },
	{ "0:measpower1", "0x7f", 0 },
	{ "0:measpower2", "0x7e", 0 },
	{ "0:tssifloor2g", "0x339", 0 },
	{ "0:pdoffset2gcck", "0x0000", 0 },
	{ "0:pdoffset20in40m2g", "0x7BDE", 0 },
	{ "0:pdoffset20in40m2gcore3", "0x001E", 0 },
	{ "0:pdoffset2g40ma0", "0", 0 },
	{ "0:pdoffset2g40ma1", "0", 0 },
	{ "0:pdoffset2g40ma2", "0", 0 },
	{ "0:pdoffset2g40mvalid", "0", 0 },
	{ "0:pdoffset40ma0", "43452", 0 },
	{ "0:pdoffset40ma1", "43452", 0 },
	{ "0:pdoffset40ma2", "43452", 0 },
	{ "0:pdoffset80ma0", "43452", 0 },
	{ "0:pdoffset80ma1", "2114", 0 },
	{ "0:pdoffset80ma2", "99", 0 },
	{ "0:subband5gver", "0x5", 0 },
	{ "0:paparambwver", "0", 0 },
	{ "0:mcs1024qam2gpo", "0x2222", 0 },
	{ "0:mcs8poexp", "0", 0 },
	{ "0:mcs9poexp", "0", 0 },
	{ "0:mcs10poexp", "0x11111111", 0 },
	{ "0:mcs11poexp", "0x11111111", 0 },
	{ "0:ulbpproffs2g", "65535", 0 },
	{ "0:ulbpdoffs2ga0", "65535", 0 },
	{ "0:ulbpdoffs2ga1", "65535", 0 },
	{ "0:ulbpdoffs2ga2", "65535", 0 },
	{ "0:ulbpdoffs2ga3", "65535", 0 },
	{ "0:cckbw202gpo", "0", 0 },
	{ "0:cckbw20ul2gpo", "0", 0 },
	{ "0:mcsbw202gpo", "0xCC774400", 0 },
	{ "0:mcsbw402gpo", "0xCC774400", 0 },
	{ "0:dot11agofdmhrbw202gpo", "7400", 0 },
	{ "0:ofdmlrbw202gpo", "0", 0 },
	{ "0:sb20in40hrpo", "0", 0 },
	{ "0:sb20in40lrpo", "0", 0 },
	{ "0:sb20in40hrlrpox", "0", 0 },
	{ "0:dot11agduphrpo", "0", 0 },
	{ "0:dot11agduplrpo", "0", 0 },
	{ "0:dot11agduphrlrpox", "0", 0 },
	{ "0:pcieingress_war", "15", 0 },
	{ "0:sar2g", "18", 0 },
	{ "0:noiselvl2ga0", "31", 0 },
	{ "0:noiselvl2ga1", "31", 0 },
	{ "0:noiselvl2ga2", "31", 0 },
	{ "0:noiselvl2ga3", "31", 0 },
	{ "0:rxgainerr2ga0", "63", 0 },
	{ "0:rxgainerr2ga1", "31", 0 },
	{ "0:rxgainerr2ga2", "31", 0 },
	{ "0:rxgainerr2ga3", "31", 0 },
	{ "0:gpdn", "0xffffffff", 0 },
	{ "0:eu_edthresh2g", "255", 0 },
	{ "0:maxp2ga0", "0x64", 0 },
	{ "0:maxp2ga1", "0x64", 0 },
	{ "0:maxp2ga2", "0x64", 0 },
	{ "0:maxp2ga3", "0x64", 0 },
	{ "0:pa5ga0", "0x1c46,0xd12d,0x1a64,0x22cb", 0 },
	{ "0:pa5g40a0", "0x1c46,0xd12d,0x1a64,0x22cb", 0 },
	{ "0:pa5ga1", "0x1bb5,0xd227,0x19e7,0x22d9", 0 },
	{ "0:pa5g40a1", "0x1bb5,0xd227,0x19e7,0x22d9", 0 },
	{ "0:pa5ga2", "0x1c3e,0xd325,0x1c24,0x2330", 0 },
	{ "0:pa5g40a2", "0x1c3e,0xd325,0x1c24,0x2330", 0 },
	{ "0:pa5ga3", "0x1c34,0xcfee,0x17c4,0x228a", 0 },
	{ "0:pa5g40a3", "0x1c34,0xcfee,0x17c4,0x228a", 0 },
	{ "0:pa2ga0", "0x1c0a,0xd723,0x1ee0,0x23ae", 0 },
	{ "0:pa2g40a0", "0x1c88,0xda62,0x2135,0x23b7", 0 },
	{ "0:pa2ga1", "0x1bfe,0xd4c6,0x1a68,0x2343", 0 },
	{ "0:pa2g40a1", "0x1ca7,0xd616,0x1cc8,0x2360", 0 },
	{ "0:pa2ga2", "0x1c3a,0xd3bc,0x1a1d,0x2346", 0 },
	{ "0:pa2g40a2", "0x1ccc,0xd6fb,0x1d15,0x2352", 0 },
	{ "0:pa2ga3", "0x1c73,0xd180,0x16c2,0x22d3", 0 },
	{ "0:pa2g40a3", "0x1d4e,0xd2ed,0x1a5b,0x230e", 0 },
	{ "0:rxgains2gelnagaina0", "3", 0 },
	{ "0:rxgains2gelnagaina1", "3", 0 },
	{ "0:rxgains2gelnagaina2", "3", 0 },
	{ "0:rxgains2gelnagaina3", "3", 0 },
	{ "0:rxgains2gtrisoa0", "7", 0 },
	{ "0:rxgains2gtrisoa1", "7", 0 },
	{ "0:rxgains2gtrisoa2", "7", 0 },
	{ "0:rxgains2gtrisoa3", "7", 0 },
	{ "0:rxgains2gtrelnabypa0", "1", 0 },
	{ "0:rxgains2gtrelnabypa1", "1", 0 },
	{ "0:rxgains2gtrelnabypa2", "1", 0 },
	{ "0:rxgains2gtrelnabypa3", "1", 0 },
	{ "0:rpcal2g", "0", 0 },
	{ "0:txidxcap2g", "0", 0 },
	{ "0:pdoffsetcckma0", "15", 0 },
	{ "0:pdoffsetcckma1", "15", 0 },
	{ "0:pdoffsetcckma2", "15", 0 },
	{ "0:swctrlmap4_cfg", "0x5", 0 },
	{ "0:swctrlmap4_TX2g_fem3to0", "0xaaaa", 0 },
	{ "0:swctrlmap4_RX2g_fem3to0", "0x4444", 0 },
	{ "0:swctrlmap4_RXByp2g_fem3to0", "0x6666", 0 },
	{ "0:swctrlmap4_misc2g_fem3to0", "0x2222", 0 },
	{ "0:swctrlmap4_TX5g_fem3to0", "0x0", 0 },
	{ "0:swctrlmap4_RX5g_fem3to0", "0x0", 0 },
	{ "0:swctrlmap4_RXByp5g_fem3to0", "0x0", 0 },
	{ "0:swctrlmap4_misc5g_fem3to0", "0x0", 0 },
	{ "0:swctrlmap4_TX2g_fem7to4", "0", 0 },
	{ "0:swctrlmap4_RX2g_fem7to4", "0", 0 },
	{ "0:swctrlmap4_RXByp2g_fem7to4", "0", 0 },
	{ "0:swctrlmap4_misc2g_fem7to4", "0", 0 },
	{ "0:swctrlmap4_TX5g_fem7to4", "0", 0 },
	{ "0:swctrlmap4_RX5g_fem7to4", "0", 0 },
	{ "0:swctrlmap4_RXByp5g_fem7to4", "0", 0 },
	{ "0:swctrlmap4_misc5g_fem7to4", "0", 0 },
	{ "0:srom13sig", "0x4D55", 0 },
	{ "1:sromrev", "13", 0 },
	{ "devpath1", "pcie/1/4", 0 },
	{ "1:boardrev", "0x1105", 0 },
	{ "1:boardvendor", "0x14e4", 0 },
	{ "1:devid", "0x43c5", 0 },
	{ "1:venid", "0x14e4", 0 },
	{ "1:boardflags", "0x10000000", 0 },
	{ "1:boardflags2", "0x4", 0 },
	{ "1:boardflags3", "0x0", 0 },
	{ "1:boardflags4", "0xE", 0 },
	{ "1:aa5g", "15", 0 },
	{ "1:agbg0", "133", 0 },
	{ "1:agbg1", "133", 0 },
	{ "1:agbg2", "133", 0 },
	{ "1:agbg3", "5", 0 },
	{ "1:aga0", "71", 0 },
	{ "1:aga1", "71", 0 },
	{ "1:aga2", "71", 0 },
	{ "1:aga3", "133", 0 },
	{ "1:txchain", "0xf", 0 },
	{ "1:rxchain", "0xf", 0 },
	{ "1:antswitch", "0", 0 },
	{ "1:femctrl", "2", 0 },
	{ "1:tssiposslope5g", "1", 0 },
	{ "1:epagain5g", "0", 0 },
	{ "1:pdgain5g", "2", 0 },
	{ "1:tworangetssi5g", "0", 0 },
	{ "1:papdcap5g", "0", 0 },
	{ "1:gainctrlsph", "0", 0 },
	{ "1:tempthresh", "120", 0 },
	{ "1:tempoffset", "255", 0 },
	{ "1:rawtempsense", "0x1ff", 0 },
	{ "1:tempsense_slope", "0xff", 0 },
	{ "1:tempcorrx", "0x3f", 0 },
	{ "1:tempsense_option", "0x3", 0 },
	{ "1:phycal_tempdelta", "40", 0 },
	{ "1:temps_period", "10", 0 },
	{ "1:temps_hysteresis", "5", 0 },
	{ "1:xtalfreq", "40000", 0 },
	{ "1:measpower", "0x7f", 0 },
	{ "1:measpower1", "0x7f", 0 },
	{ "1:measpower2", "0x7e", 0 },
	{ "1:tssifloor5g", "0x0,0x3ff,0x335,0x318", 0 },
	{ "1:subband5gver", "0x5", 0 },
	{ "1:paparambwver", "0", 0 },
	{ "1:pdoffset20in40m5gcore3", "0x77BD", 0 },
	{ "1:pdoffset20in40m5gcore3_1", "0x03BD", 0 },
	{ "1:pdoffset40in80m5gcore3", "0x0422", 0 },
	{ "1:pdoffset40in80m5gcore3_1", "0x0041", 0 },
	{ "1:pdoffset20in80m5gcore3", "0x7BFE", 0 },
	{ "1:pdoffset20in80m5gcore3_1", "0x03DE", 0 },
	{ "1:mcs1024qam5glpo", "0xEEEEEEEE", 0 },
	{ "1:mcs1024qam5gmpo", "0xEEEEEEEE", 0 },
	{ "1:mcs1024qam5ghpo", "0x66666666", 0 },
	{ "1:mcs1024qam5gx1po", "0x66666666", 0 },
	{ "1:mcs1024qam5gx2po", "0x66666666", 0 },
	{ "1:mcs8poexp", "0", 0 },
	{ "1:mcs9poexp", "0", 0 },
	{ "1:mcs10poexp", "0xF8F8F8F8", 0 },
	{ "1:mcs11poexp", "0xF8F8F8F8", 0 },
	{ "1:ulbpproffs5gb0", "0", 0 },
	{ "1:ulbpproffs5gb1", "0", 0 },
	{ "1:ulbpproffs5gb2", "0", 0 },
	{ "1:ulbpproffs5gb3", "0", 0 },
	{ "1:ulbpproffs5gb4", "0", 0 },
	{ "1:ulbpdoffs5gb0a0", "65535", 0 },
	{ "1:ulbpdoffs5gb1a0", "65535", 0 },
	{ "1:ulbpdoffs5gb2a0", "65535", 0 },
	{ "1:ulbpdoffs5gb3a0", "65535", 0 },
	{ "1:ulbpdoffs5gb4a0", "65535", 0 },
	{ "1:ulbpdoffs5gb0a1", "65535", 0 },
	{ "1:ulbpdoffs5gb1a1", "65535", 0 },
	{ "1:ulbpdoffs5gb2a1", "65535", 0 },
	{ "1:ulbpdoffs5gb3a1", "65535", 0 },
	{ "1:ulbpdoffs5gb4a1", "65535", 0 },
	{ "1:ulbpdoffs5gb0a2", "65535", 0 },
	{ "1:ulbpdoffs5gb1a2", "65535", 0 },
	{ "1:ulbpdoffs5gb2a2", "65535", 0 },
	{ "1:ulbpdoffs5gb3a2", "65535", 0 },
	{ "1:ulbpdoffs5gb4a2", "65535", 0 },
	{ "1:ulbpdoffs5gb0a3", "65535", 0 },
	{ "1:ulbpdoffs5gb1a3", "65535", 0 },
	{ "1:ulbpdoffs5gb2a3", "65535", 0 },
	{ "1:ulbpdoffs5gb3a3", "65535", 0 },
	{ "1:ulbpdoffs5gb4a3", "65535", 0 },
	{ "1:mcsbw205glpo", "0xAA888220", 0 },
	{ "1:mcsbw405glpo", "0xAA888220", 0 },
	{ "1:mcsbw805glpo", "0xAA888220", 0 },
	{ "1:mcsbw1605glpo", "0xFFFFFFFF ", 0 },
	{ "1:mcsbw205gmpo", "0xAA888220", 0 },
	{ "1:mcsbw405gmpo", "0xAA888220", 0 },
	{ "1:mcsbw805gmpo", "0xAA888220", 0 },
	{ "1:mcsbw1605gmpo", "0xFFFFFFFF", 0 },
	{ "1:mcsbw205ghpo", "0xEE888220", 0 },
	{ "1:mcsbw405ghpo", "0xEE888220", 0 },
	{ "1:mcsbw805ghpo", "0xEE888220", 0 },
	{ "1:mcsbw1605ghpo", "0xFFFFFFFF", 0 },
	{ "1:mcsbw205gx1po", "0xEE888220", 0 },
	{ "1:mcsbw405gx1po", "0xEE888220", 0 },
	{ "1:mcsbw805gx1po", "0xEE888220", 0 },
	{ "1:mcsbw1605gx1po", "0xFFFFFFFF", 0 },
	{ "1:mcsbw205gx2po", "0xEE888220", 0 },
	{ "1:mcsbw405gx2po", "0xEE888220", 0 },
	{ "1:mcsbw805gx2po", "0xEE888220", 0 },
	{ "1:mcsbw1605gx2po", "0xFFFFFFFF", 0 },
	{ "1:mcslr5glpo", "0", 0 },
	{ "1:mcslr5gmpo", "0", 0 },
	{ "1:mcslr5ghpo", "0", 0 },
	{ "1:mcslr5gx1po", "0", 0 },
	{ "1:mcslr5gx2po", "0", 0 },
	{ "1:sb20in40hrpo", "0", 0 },
	{ "1:sb20in40lrpo", "0", 0 },
	{ "1:sb20in40hrlrpox", "0", 0 },
	{ "1:dot11agduphrpo", "0", 0 },
	{ "1:dot11agduplrpo", "0", 0 },
	{ "1:dot11agduphrlrpox", "0", 0 },
	{ "1:sb40and80hr5glpo", "0", 0 },
	{ "1:sb40and80hr5gmpo", "0", 0 },
	{ "1:sb40and80hr5ghpo", "0", 0 },
	{ "1:sb40and80hr5gx1po", "0", 0 },
	{ "1:sb40and80hr5gx2po", "0", 0 },
	{ "1:sb20in80and160hr5glpo", "0", 0 },
	{ "1:sb20in80and160hr5gmpo", "0", 0 },
	{ "1:sb20in80and160hr5ghpo", "0", 0 },
	{ "1:sb20in80and160hr5gx1po", "0", 0 },
	{ "1:sb20in80and160hr5gx2po", "0", 0 },
	{ "1:sb40and80lr5glpo", "0", 0 },
	{ "1:sb40and80lr5gmpo", "0", 0 },
	{ "1:sb40and80lr5ghpo", "0", 0 },
	{ "1:sb40and80lr5gx1po", "0", 0 },
	{ "1:sb40and80lr5gx2po", "0", 0 },
	{ "1:sb20in80and160lr5glpo", "0", 0 },
	{ "1:sb20in80and160lr5gmpo", "0", 0 },
	{ "1:sb20in80and160lr5ghpo", "0", 0 },
	{ "1:sb20in80and160lr5gx1po", "0", 0 },
	{ "1:sb20in80and160lr5gx2po", "0", 0 },
	{ "1:pcieingress_war", "15", 0 },
	{ "1:sar5g", "15", 0 },
	{ "1:noiselvl5ga0", "31,31,31,31", 0 },
	{ "1:noiselvl5ga1", "31,31,31,31", 0 },
	{ "1:noiselvl5ga2", "31,31,31,31", 0 },
	{ "1:noiselvl5ga3", "31,31,31,31", 0 },
	{ "1:rxgainerr5ga0", "63,63,63,63", 0 },
	{ "1:rxgainerr5ga1", "31,31,31,31", 0 },
	{ "1:rxgainerr5ga2", "31,31,31,31", 0 },
	{ "1:rxgainerr5ga3", "31,31,31,31", 0 },
	{ "1:rxgains5gmelnagaina0", "3", 0 },
	{ "1:rxgains5gmelnagaina1", "3", 0 },
	{ "1:rxgains5gmelnagaina2", "3", 0 },
	{ "1:rxgains5gmelnagaina3", "3", 0 },
	{ "1:rxgains5gmtrisoa0", "6", 0 },
	{ "1:rxgains5gmtrisoa1", "6", 0 },
	{ "1:rxgains5gmtrisoa2", "6", 0 },
	{ "1:rxgains5gmtrisoa3", "6", 0 },
	{ "1:rxgains5gmtrelnabypa0", "1", 0 },
	{ "1:rxgains5gmtrelnabypa1", "1", 0 },
	{ "1:rxgains5gmtrelnabypa2", "1", 0 },
	{ "1:rxgains5gmtrelnabypa3", "1", 0 },
	{ "1:rxgains5ghelnagaina0", "3", 0 },
	{ "1:rxgains5ghelnagaina1", "3", 0 },
	{ "1:rxgains5ghelnagaina2", "3", 0 },
	{ "1:rxgains5ghelnagaina3", "3", 0 },
	{ "1:rxgains5ghtrisoa0", "6", 0 },
	{ "1:rxgains5ghtrisoa1", "6", 0 },
	{ "1:rxgains5ghtrisoa2", "6", 0 },
	{ "1:rxgains5ghtrisoa3", "6", 0 },
	{ "1:rxgains5ghtrelnabypa0", "1", 0 },
	{ "1:rxgains5ghtrelnabypa1", "1", 0 },
	{ "1:rxgains5ghtrelnabypa2", "1", 0 },
	{ "1:rxgains5ghtrelnabypa3", "1", 0 },
	{ "1:gpdn", "0xffffffff", 0 },
	{ "1:eu_edthresh5g", "255", 0 },
	{ "1:rxgains5gelnagaina0", "3", 0 },
	{ "1:rxgains5gelnagaina1", "3", 0 },
	{ "1:rxgains5gelnagaina2", "3", 0 },
	{ "1:rxgains5gelnagaina3", "3", 0 },
	{ "1:rxgains5gtrisoa0", "6", 0 },
	{ "1:rxgains5gtrisoa1", "6", 0 },
	{ "1:rxgains5gtrisoa2", "6", 0 },
	{ "1:rxgains5gtrisoa3", "6", 0 },
	{ "1:rxgains5gtrelnabypa0", "1", 0 },
	{ "1:rxgains5gtrelnabypa1", "1", 0 },
	{ "1:rxgains5gtrelnabypa2", "1", 0 },
	{ "1:rxgains5gtrelnabypa3", "1", 0 },
	{ "1:maxp5gb0a0", "0x64", 0 },
	{ "1:maxp5gb1a0", "0x64", 0 },
	{ "1:maxp5gb2a0", "0x64", 0 },
	{ "1:maxp5gb3a0", "0x64", 0 },
	{ "1:maxp5gb4a0", "0x64", 0 },
	{ "1:maxp5gb0a1", "0x64", 0 },
	{ "1:maxp5gb1a1", "0x64", 0 },
	{ "1:maxp5gb2a1", "0x64", 0 },
	{ "1:maxp5gb3a1", "0x64", 0 },
	{ "1:maxp5gb4a1", "0x64", 0 },
	{ "1:maxp5gb0a2", "0x64", 0 },
	{ "1:maxp5gb1a2", "0x64", 0 },
	{ "1:maxp5gb2a2", "0x64", 0 },
	{ "1:maxp5gb3a2", "0x64", 0 },
	{ "1:maxp5gb4a2", "0x64", 0 },
	{ "1:maxp5gb0a3", "0x64", 0 },
	{ "1:maxp5gb1a3", "0x64", 0 },
	{ "1:maxp5gb2a3", "0x64", 0 },
	{ "1:maxp5gb3a3", "0x64", 0 },
	{ "1:maxp5gb4a3", "0x64", 0 },
	{ "1:pa5ga0", "0x1e2f,0xc9a2,0xc9a2,0xc9a2,0x1e28,0xc89f,0x2369,0x2393,0x1e19,0xc94b,0x25b1,0x240e,0x1de8,0xc6a4,0x1e89,0x22e2,0x199f,0xd1fe,0x3bde,0x2678", 0 },
	{ "1:pa5g40a0", "0x1f17,0xd2ac,0x3aec,0x25fc,0x1f24,0xd3fb,0x3ded,0x2654,0x1edc,0xd851,0x487d,0x275a,0x1ed3,0xd46f,0x3ff4,0x2696,0x1aa9,0xd277,0x3ab1,0x25cd", 0 },
	{ "1:pa5g80a0", "0x1fc5,0xdda2,0x4b70,0x26f9,0x1fa8,0xe03a,0x50a1,0x2772,0x1f93,0xdfd1,0x50cf,0x2779,0x1f8d,0xdd8f,0x4d83,0x2765,0x1c1b,0xd348,0x3c15,0x25a0", 0 },
	{ "1:pa5ga1", "0x1dd7,0xc766,0x221f,0x237e,0x1dc4,0xc7f8,0x22fd,0x23a5,0x1dc9,0xc860,0x2325,0x23a8,0x1da0,0xc6c8,0x1e8d,0x22df,0x194c,0xc9a1,0x292a,0x2477", 0 },
	{ "1:pa5g40a1", "0x1e82,0xd57a,0x43a1,0x270e,0x1e87,0xd71a,0x4681,0x273f,0x1e78,0xd697,0x42c6,0x26d8,0x1e72,0xd394,0x3cab,0x265a,0x1a54,0xce71,0x3523,0x254d", 0 },
	{ "1:pa5g80a1", "0x1f01,0xdfcd,0x522d,0x27af,0x1f18,0xdd68,0x4c14,0x273e,0x1ef2,0xde6b,0x4ee8,0x278a,0x1f0b,0xdaf3,0x471b,0x26fe,0x1b80,0xcde7,0x32e5,0x2505", 0 },
	{ "1:pa5ga2", "0x1e2b,0xc851,0x24f1,0x23d3,0x1e31,0xc9ff,0x27fd,0x2451,0x1e42,0xca20,0x2791,0x2437,0x1e13,0xc8d1,0x242c,0x23ac,0x1a23,0xcec8,0x3216,0x2550", 0 },
	{ "1:pa5g40a2", "0x1ec0,0xdac4,0x50b3,0x2807,0x1ef6,0xda38,0x4d94,0x27bd,0x1ec4,0xddc6,0x543b,0x281a,0x1ede,0xd8e1,0x4a5d,0x276d,0x1b07,0xd11b,0x351c,0x2536", 0 },
	{ "1:pa5g80a2", "0x1f9d,0xe18b,0x51ae,0x2754,0x1fbf,0xdf13,0x4b5e,0x26e1,0x1f87,0xe071,0x4dd6,0x270b,0x1fa4,0xdf99,0x4f1c,0x2744,0x1c3e,0xd70e,0x455f,0x267b", 0 },
	{ "1:pa5ga3", "0x1e2c,0xc9e2,0x282d,0x245c,0x1e2e,0xcb5b,0x2a14,0x2498,0x1e3e,0xca6e,0x27aa,0x2433,0x1e0b,0xc835,0x23aa,0x23bf,0x1a82,0xd441,0x3a0e,0x2601", 0 },
	{ "1:pa5g40a3", "0x1ed4,0xd999,0x4c9c,0x27be,0x1ee9,0xdb17,0x4e7c,0x27d1,0x1ee8,0xda30,0x4b75,0x276c,0x1ef6,0xd489,0x3f53,0x2671,0x1b05,0xe45e,0x6302,0x28f2", 0 },
	{ "1:pa5g80a3", "0x1fac,0xe182,0x5211,0x276c,0x1f4c,0xe7be,0x5d6d,0x2843,0x1f8f,0xe325,0x5480,0x2791,0x1fb0,0xde51,0x4c9e,0x2733,0x1c6b,0xdb66,0x4337,0x2607", 0 },
	{ "1:rpcal5gb0", "0", 0 },
	{ "1:rpcal5gb1", "0", 0 },
	{ "1:rpcal5gb2", "0", 0 },
	{ "1:rpcal5gb3", "0", 0 },
	{ "1:rpcal5gb4", "0", 0 },
	{ "1:txidxcap5g", "0", 0 },
	{ "1:pdoffsetcckma0", "15", 0 },
	{ "1:pdoffsetcckma1", "15", 0 },
	{ "1:pdoffsetcckma2", "15", 0 },
	{ "1:pdoffsetcck", "0", 0 },
	{ "1:pdoffset20in40m5gb0", "0x7BBD", 0 },
	{ "1:pdoffset20in40m5gb1", "0x7BBD", 0 },
	{ "1:pdoffset20in40m5gb2", "0x77BD", 0 },
	{ "1:pdoffset20in40m5gb3", "0x77BD", 0 },
	{ "1:pdoffset20in40m5gb4", "0x77BD", 0 },
	{ "1:pdoffset40in80m5gb0", "0x0000", 0 },
	{ "1:pdoffset40in80m5gb1", "0x0420", 0 },
	{ "1:pdoffset40in80m5gb2", "0x0440", 0 },
	{ "1:pdoffset40in80m5gb3", "0x0820", 0 },
	{ "1:pdoffset40in80m5gb4", "0x0841", 0 },
	{ "1:pdoffset20in80m5gb0", "0x7BDD", 0 },
	{ "1:pdoffset20in80m5gb1", "0x7FFF", 0 },
	{ "1:pdoffset20in80m5gb2", "0x7BDE", 0 },
	{ "1:pdoffset20in80m5gb3", "0x7BDE", 0 },
	{ "1:pdoffset20in80m5gb4", "0x7BDE", 0 },
	{ "1:swctrlmap4_cfg", "1", 0 },
	{ "1:swctrlmap4_TX2g_fem3to0", "0x0", 0 },
	{ "1:swctrlmap4_RX2g_fem3to0", "0x0", 0 },
	{ "1:swctrlmap4_RXByp2g_fem3to0", "0x0", 0 },
	{ "1:swctrlmap4_misc2g_fem3to0", "0x0", 0 },
	{ "1:swctrlmap4_TX5g_fem3to0", "0x6666", 0 },
	{ "1:swctrlmap4_RX5g_fem3to0", "0x1111", 0 },
	{ "1:swctrlmap4_RXByp5g_fem3to0", "0x3333", 0 },
	{ "1:swctrlmap4_misc5g_fem3to0", "0x0000", 0 },
	{ "1:swctrlmap4_TX2g_fem7to4", "0", 0 },
	{ "1:swctrlmap4_RX2g_fem7to4", "0", 0 },
	{ "1:swctrlmap4_RXByp2g_fem7to4", "0", 0 },
	{ "1:swctrlmap4_misc2g_fem7to4", "0", 0 },
	{ "1:swctrlmap4_TX5g_fem7to4", "0", 0 },
	{ "1:swctrlmap4_RX5g_fem7to4", "0", 0 },
	{ "1:swctrlmap4_RXByp5g_fem7to4", "0", 0 },
	{ "1:swctrlmap4_misc5g_fem7to4", "0", 0 },
	{ "1:srom13sig", "0x4D55", 0 },
	{ "2:sromrev", "13", 0 },
	{ "devpath2", "pcie/2/1", 0 },
	{ "2:boardvendor", "0x14e4", 0 },
	{ "2:devid", "0x43c5", 0 },
	{ "2:venid", "0x14e4", 0 },
	{ "2:boardflags", "0x10000000", 0 },
	{ "2:boardflags2", "0x4", 0 },
	{ "2:boardflags3", "0x0", 0 },
	{ "2:boardflags4", "0xE", 0 },
	{ "2:aa5g", "15", 0 },
	{ "2:agbg0", "133", 0 },
	{ "2:agbg1", "133", 0 },
	{ "2:agbg2", "133", 0 },
	{ "2:agbg3", "5", 0 },
	{ "2:aga0", "71", 0 },
	{ "2:aga1", "71", 0 },
	{ "2:aga2", "71", 0 },
	{ "2:aga3", "133", 0 },
	{ "2:txchain", "0xf", 0 },
	{ "2:rxchain", "0xf", 0 },
	{ "2:antswitch", "0", 0 },
	{ "2:femctrl", "2", 0 },
	{ "2:tssiposslope5g", "1", 0 },
	{ "2:epagain5g", "0", 0 },
	{ "2:pdgain5g", "2", 0 },
	{ "2:tworangetssi5g", "0", 0 },
	{ "2:papdcap5g", "0", 0 },
	{ "2:gainctrlsph", "0", 0 },
	{ "2:tempthresh", "120", 0 },
	{ "2:tempoffset", "255", 0 },
	{ "2:rawtempsense", "0x1ff", 0 },
	{ "2:tempsense_slope", "0xff", 0 },
	{ "2:tempcorrx", "0x3f", 0 },
	{ "2:tempsense_option", "0x3", 0 },
	{ "2:phycal_tempdelta", "40", 0 },
	{ "2:temps_period", "10", 0 },
	{ "2:temps_hysteresis", "5", 0 },
	{ "2:xtalfreq", "40000", 0 },
	{ "2:measpower", "0x7f", 0 },
	{ "2:measpower1", "0x7f", 0 },
	{ "2:measpower2", "0x7e", 0 },
	{ "2:tssifloor5g", "0x0,0x3ff,0x335,0x318", 0 },
	{ "2:subband5gver", "0x5", 0 },
	{ "2:paparambwver", "0", 0 },
	{ "2:pdoffset20in40m5gcore3", "0x77BD", 0 },
	{ "2:pdoffset20in40m5gcore3_1", "0x03BD", 0 },
	{ "2:pdoffset40in80m5gcore3", "0x0422", 0 },
	{ "2:pdoffset40in80m5gcore3_1", "0x0041", 0 },
	{ "2:pdoffset20in80m5gcore3", "0x7BFE", 0 },
	{ "2:pdoffset20in80m5gcore3_1", "0x03DE", 0 },
	{ "2:mcs1024qam5glpo", "0xEEEEEEEE", 0 },
	{ "2:mcs1024qam5gmpo", "0xEEEEEEEE", 0 },
	{ "2:mcs1024qam5ghpo", "0x66666666", 0 },
	{ "2:mcs1024qam5gx1po", "0x66666666", 0 },
	{ "2:mcs1024qam5gx2po", "0x66666666", 0 },
	{ "2:mcs8poexp", "0", 0 },
	{ "2:mcs9poexp", "0", 0 },
	{ "2:mcs10poexp", "0xF8F8F8F8", 0 },
	{ "2:mcs11poexp", "0xF8F8F8F8", 0 },
	{ "2:ulbpproffs5gb0", "0", 0 },
	{ "2:ulbpproffs5gb1", "0", 0 },
	{ "2:ulbpproffs5gb2", "0", 0 },
	{ "2:ulbpproffs5gb3", "0", 0 },
	{ "2:ulbpproffs5gb4", "0", 0 },
	{ "2:ulbpdoffs5gb0a0", "65535", 0 },
	{ "2:ulbpdoffs5gb1a0", "65535", 0 },
	{ "2:ulbpdoffs5gb2a0", "65535", 0 },
	{ "2:ulbpdoffs5gb3a0", "65535", 0 },
	{ "2:ulbpdoffs5gb4a0", "65535", 0 },
	{ "2:ulbpdoffs5gb0a1", "65535", 0 },
	{ "2:ulbpdoffs5gb1a1", "65535", 0 },
	{ "2:ulbpdoffs5gb2a1", "65535", 0 },
	{ "2:ulbpdoffs5gb3a1", "65535", 0 },
	{ "2:ulbpdoffs5gb4a1", "65535", 0 },
	{ "2:ulbpdoffs5gb0a2", "65535", 0 },
	{ "2:ulbpdoffs5gb1a2", "65535", 0 },
	{ "2:ulbpdoffs5gb2a2", "65535", 0 },
	{ "2:ulbpdoffs5gb3a2", "65535", 0 },
	{ "2:ulbpdoffs5gb4a2", "65535", 0 },
	{ "2:ulbpdoffs5gb0a3", "65535", 0 },
	{ "2:ulbpdoffs5gb1a3", "65535", 0 },
	{ "2:ulbpdoffs5gb2a3", "65535", 0 },
	{ "2:ulbpdoffs5gb3a3", "65535", 0 },
	{ "2:ulbpdoffs5gb4a3", "65535", 0 },
	{ "2:mcsbw205glpo", "0xAA888220", 0 },
	{ "2:mcsbw405glpo", "0xAA888220", 0 },
	{ "2:mcsbw805glpo", "0xAA888220", 0 },
	{ "2:mcsbw1605glpo", "0xFFFFFFFF ", 0 },
	{ "2:mcsbw205gmpo", "0xAA888220", 0 },
	{ "2:mcsbw405gmpo", "0xAA888220", 0 },
	{ "2:mcsbw805gmpo", "0xAA888220", 0 },
	{ "2:mcsbw1605gmpo", "0xFFFFFFFF", 0 },
	{ "2:mcsbw205ghpo", "0xEE888220", 0 },
	{ "2:mcsbw405ghpo", "0xEE888220", 0 },
	{ "2:mcsbw805ghpo", "0xEE888220", 0 },
	{ "2:mcsbw1605ghpo", "0xFFFFFFFF", 0 },
	{ "2:mcsbw205gx1po", "0xEE888220", 0 },
	{ "2:mcsbw405gx1po", "0xEE888220", 0 },
	{ "2:mcsbw805gx1po", "0xEE888220", 0 },
	{ "2:mcsbw1605gx1po", "0xFFFFFFFF", 0 },
	{ "2:mcsbw205gx2po", "0xEE888220", 0 },
	{ "2:mcsbw405gx2po", "0xEE888220", 0 },
	{ "2:mcsbw805gx2po", "0xEE888220", 0 },
	{ "2:mcsbw1605gx2po", "0xFFFFFFFF", 0 },
	{ "2:mcslr5glpo", "0", 0 },
	{ "2:mcslr5gmpo", "0", 0 },
	{ "2:mcslr5ghpo", "0", 0 },
	{ "2:mcslr5gx1po", "0", 0 },
	{ "2:mcslr5gx2po", "0", 0 },
	{ "2:sb20in40hrpo", "0", 0 },
	{ "2:sb20in40lrpo", "0", 0 },
	{ "2:sb20in40hrlrpox", "0", 0 },
	{ "2:dot11agduphrpo", "0", 0 },
	{ "2:dot11agduplrpo", "0", 0 },
	{ "2:dot11agduphrlrpox", "0", 0 },
	{ "2:sb40and80hr5glpo", "0", 0 },
	{ "2:sb40and80hr5gmpo", "0", 0 },
	{ "2:sb40and80hr5ghpo", "0", 0 },
	{ "2:sb40and80hr5gx1po", "0", 0 },
	{ "2:sb40and80hr5gx2po", "0", 0 },
	{ "2:sb20in80and160hr5glpo", "0", 0 },
	{ "2:sb20in80and160hr5gmpo", "0", 0 },
	{ "2:sb20in80and160hr5ghpo", "0", 0 },
	{ "2:sb20in80and160hr5gx1po", "0", 0 },
	{ "2:sb20in80and160hr5gx2po", "0", 0 },
	{ "2:sb40and80lr5glpo", "0", 0 },
	{ "2:sb40and80lr5gmpo", "0", 0 },
	{ "2:sb40and80lr5ghpo", "0", 0 },
	{ "2:sb40and80lr5gx1po", "0", 0 },
	{ "2:sb40and80lr5gx2po", "0", 0 },
	{ "2:sb20in80and160lr5glpo", "0", 0 },
	{ "2:sb20in80and160lr5gmpo", "0", 0 },
	{ "2:sb20in80and160lr5ghpo", "0", 0 },
	{ "2:sb20in80and160lr5gx1po", "0", 0 },
	{ "2:sb20in80and160lr5gx2po", "0", 0 },
	{ "2:pcieingress_war", "15", 0 },
	{ "2:sar5g", "15", 0 },
	{ "2:noiselvl5ga0", "31,31,31,31", 0 },
	{ "2:noiselvl5ga1", "31,31,31,31", 0 },
	{ "2:noiselvl5ga2", "31,31,31,31", 0 },
	{ "2:noiselvl5ga3", "31,31,31,31", 0 },
	{ "2:rxgainerr5ga0", "63,63,63,63", 0 },
	{ "2:rxgainerr5ga1", "31,31,31,31", 0 },
	{ "2:rxgainerr5ga2", "31,31,31,31", 0 },
	{ "2:rxgainerr5ga3", "31,31,31,31", 0 },
	{ "2:rxgains5gmelnagaina0", "3", 0 },
	{ "2:rxgains5gmelnagaina1", "3", 0 },
	{ "2:rxgains5gmelnagaina2", "3", 0 },
	{ "2:rxgains5gmelnagaina3", "3", 0 },
	{ "2:rxgains5gmtrisoa0", "6", 0 },
	{ "2:rxgains5gmtrisoa1", "6", 0 },
	{ "2:rxgains5gmtrisoa2", "6", 0 },
	{ "2:rxgains5gmtrisoa3", "6", 0 },
	{ "2:rxgains5gmtrelnabypa0", "1", 0 },
	{ "2:rxgains5gmtrelnabypa1", "1", 0 },
	{ "2:rxgains5gmtrelnabypa2", "1", 0 },
	{ "2:rxgains5gmtrelnabypa3", "1", 0 },
	{ "2:rxgains5ghelnagaina0", "3", 0 },
	{ "2:rxgains5ghelnagaina1", "3", 0 },
	{ "2:rxgains5ghelnagaina2", "3", 0 },
	{ "2:rxgains5ghelnagaina3", "3", 0 },
	{ "2:rxgains5ghtrisoa0", "6", 0 },
	{ "2:rxgains5ghtrisoa1", "6", 0 },
	{ "2:rxgains5ghtrisoa2", "6", 0 },
	{ "2:rxgains5ghtrisoa3", "6", 0 },
	{ "2:rxgains5ghtrelnabypa0", "1", 0 },
	{ "2:rxgains5ghtrelnabypa1", "1", 0 },
	{ "2:rxgains5ghtrelnabypa2", "1", 0 },
	{ "2:rxgains5ghtrelnabypa3", "1", 0 },
	{ "2:gpdn", "0xffffffff", 0 },
	{ "2:eu_edthresh5g", "255", 0 },
	{ "2:rxgains5gelnagaina0", "3", 0 },
	{ "2:rxgains5gelnagaina1", "3", 0 },
	{ "2:rxgains5gelnagaina2", "3", 0 },
	{ "2:rxgains5gelnagaina3", "3", 0 },
	{ "2:rxgains5gtrisoa0", "6", 0 },
	{ "2:rxgains5gtrisoa1", "6", 0 },
	{ "2:rxgains5gtrisoa2", "6", 0 },
	{ "2:rxgains5gtrisoa3", "6", 0 },
	{ "2:rxgains5gtrelnabypa0", "1", 0 },
	{ "2:rxgains5gtrelnabypa1", "1", 0 },
	{ "2:rxgains5gtrelnabypa2", "1", 0 },
	{ "2:rxgains5gtrelnabypa3", "1", 0 },
	{ "2:maxp5gb0a0", "0x64", 0 },
	{ "2:maxp5gb1a0", "0x64", 0 },
	{ "2:maxp5gb2a0", "0x64", 0 },
	{ "2:maxp5gb3a0", "0x64", 0 },
	{ "2:maxp5gb4a0", "0x64", 0 },
	{ "2:maxp5gb0a1", "0x64", 0 },
	{ "2:maxp5gb1a1", "0x64", 0 },
	{ "2:maxp5gb2a1", "0x64", 0 },
	{ "2:maxp5gb3a1", "0x64", 0 },
	{ "2:maxp5gb4a1", "0x64", 0 },
	{ "2:maxp5gb0a2", "0x64", 0 },
	{ "2:maxp5gb1a2", "0x64", 0 },
	{ "2:maxp5gb2a2", "0x64", 0 },
	{ "2:maxp5gb3a2", "0x64", 0 },
	{ "2:maxp5gb4a2", "0x64", 0 },
	{ "2:maxp5gb0a3", "0x64", 0 },
	{ "2:maxp5gb1a3", "0x64", 0 },
	{ "2:maxp5gb2a3", "0x64", 0 },
	{ "2:maxp5gb3a3", "0x64", 0 },
	{ "2:maxp5gb4a3", "0x64", 0 },
	{ "2:pa5ga0", "0x1bc1,0xd294,0x33ea,0x2531,0x1e28,0xc89f,0x2369,0x2393,0x1e19,0xc94b,0x25b1,0x240e,0x1de8,0xc6a4,0x1e89,0x22e2,0x1dbc,0xc54f,0x1e4c,0x2304", 0 },
	{ "2:pa5g40a0", "0x1caf,0xd4a0,0x3684,0x2529,0x1f24,0xd3fb,0x3ded,0x2654,0x1edc,0xd851,0x487d,0x275a,0x1ed3,0xd46f,0x3ff4,0x2696,0x1ea9,0xd358,0x41bf,0x26ee", 0 },
	{ "2:pa5g80a0", "0x1e6a,0xcf3e,0x2edd,0x245e,0x1fa8,0xe03a,0x50a1,0x2772,0x1f93,0xdfd1,0x50cf,0x2779,0x1f8d,0xdd8f,0x4d83,0x2765,0x1f61,0xdb09,0x4aa3,0x273f", 0 },
	{ "2:pa5ga1", "0x1c89,0xcfe7,0x303e,0x24f1,0x1dc4,0xc7f8,0x22fd,0x23a5,0x1dc9,0xc860,0x2325,0x23a8,0x1da0,0xc6c8,0x1e8d,0x22df,0x1d8e,0xc65b,0x1f2f,0x232c", 0 },
	{ "2:pa5g40a1", "0x1d60,0xd1eb,0x3147,0x24a6,0x1e87,0xd71a,0x4681,0x273f,0x1e78,0xd697,0x42c6,0x26d8,0x1e72,0xd394,0x3cab,0x265a,0x1e6f,0xd116,0x37c0,0x25e3", 0 },
	{ "2:pa5g80a1", "0x1e79,0xd0f0,0x30a9,0x2479,0x1f18,0xdd68,0x4c14,0x273e,0x1ef2,0xde6b,0x4ee8,0x278a,0x1f0b,0xdaf3,0x471b,0x26fe,0x1ef9,0xd83d,0x431c,0x26c7", 0 },
	{ "2:pa5ga2", "0x1c3f,0xd1a0,0x3717,0x25af,0x1e31,0xc9ff,0x27fd,0x2451,0x1e42,0xca20,0x2791,0x2437,0x1e13,0xc8d1,0x242c,0x23ac,0x1dda,0xc938,0x26df,0x2427", 0 },
	{ "2:pa5g40a2", "0x1d34,0xd4a8,0x3ad8,0x25ad,0x1ef6,0xda38,0x4d94,0x27bd,0x1ec4,0xddc6,0x543b,0x281a,0x1ede,0xd8e1,0x4a5d,0x276d,0x1efc,0xd0de,0x3889,0x25c5", 0 },
	{ "2:pa5g80a2", "0x1ead,0xcde0,0x2c6e,0x2410,0x1fbf,0xdf13,0x4b5e,0x26e1,0x1f87,0xe071,0x4dd6,0x270b,0x1fa4,0xdf99,0x4f1c,0x2744,0x1fbf,0xd8f1,0x4323,0x2673", 0 },
	{ "2:pa5ga3", "0x1c90,0xd31d,0x37b6,0x25c5,0x1e2e,0xcb5b,0x2a14,0x2498,0x1e3e,0xca6e,0x27aa,0x2433,0x1e0b,0xc835,0x23aa,0x23bf,0x1dcd,0xc6d0,0x221f,0x2395", 0 },
	{ "2:pa5g40a3", "0x1d9b,0xd4a3,0x380f,0x2537,0x1ee9,0xdb17,0x4e7c,0x27d1,0x1ee8,0xda30,0x4b75,0x276c,0x1ef6,0xd489,0x3f53,0x2671,0x1eeb,0xcf2e,0x3527,0x25a1", 0 },
	{ "2:pa5g80a3", "0x1f3d,0xd0b2,0x336e,0x2514,0x1f4c,0xe7be,0x5d6d,0x2843,0x1f8f,0xe325,0x5480,0x2791,0x1fb0,0xde51,0x4c9e,0x2733,0x1e50,0xda8b,0x490b,0x2703", 0 },
	{ "2:rpcal5gb0", "0", 0 },
	{ "2:rpcal5gb1", "0", 0 },
	{ "2:rpcal5gb2", "0", 0 },
	{ "2:rpcal5gb3", "0", 0 },
	{ "2:rpcal5gb4", "0", 0 },
	{ "2:txidxcap5g", "0", 0 },
	{ "2:pdoffsetcckma0", "15", 0 },
	{ "2:pdoffsetcckma1", "15", 0 },
	{ "2:pdoffsetcckma2", "15", 0 },
	{ "2:pdoffsetcck", "0", 0 },
	{ "2:pdoffset20in40m5gb0", "0x7BBD", 0 },
	{ "2:pdoffset20in40m5gb1", "0x7BBD", 0 },
	{ "2:pdoffset20in40m5gb2", "0x77BD", 0 },
	{ "2:pdoffset20in40m5gb3", "0x77BD", 0 },
	{ "2:pdoffset20in40m5gb4", "0x77BD", 0 },
	{ "2:pdoffset40in80m5gb0", "0x0000", 0 },
	{ "2:pdoffset40in80m5gb1", "0x0420", 0 },
	{ "2:pdoffset40in80m5gb2", "0x0440", 0 },
	{ "2:pdoffset40in80m5gb3", "0x0820", 0 },
	{ "2:pdoffset40in80m5gb4", "0x0841", 0 },
	{ "2:pdoffset20in80m5gb0", "0x7BDD", 0 },
	{ "2:pdoffset20in80m5gb1", "0x7FFF", 0 },
	{ "2:pdoffset20in80m5gb2", "0x7BDE", 0 },
	{ "2:pdoffset20in80m5gb3", "0x7BDE", 0 },
	{ "2:pdoffset20in80m5gb4", "0x7BDE", 0 },
	{ "2:swctrlmap4_cfg", "1", 0 },
	{ "2:swctrlmap4_TX2g_fem3to0", "0x0", 0 },
	{ "2:swctrlmap4_RX2g_fem3to0", "0x0", 0 },
	{ "2:swctrlmap4_RXByp2g_fem3to0", "0x0", 0 },
	{ "2:swctrlmap4_misc2g_fem3to0", "0x0", 0 },
	{ "2:swctrlmap4_TX5g_fem3to0", "0x6666", 0 },
	{ "2:swctrlmap4_RX5g_fem3to0", "0x1111", 0 },
	{ "2:swctrlmap4_RXByp5g_fem3to0", "0x3333", 0 },
	{ "2:swctrlmap4_misc5g_fem3to0", "0x0000", 0 },
	{ "2:swctrlmap4_TX2g_fem7to4", "0", 0 },
	{ "2:swctrlmap4_RX2g_fem7to4", "0", 0 },
	{ "2:swctrlmap4_RXByp2g_fem7to4", "0", 0 },
	{ "2:swctrlmap4_misc2g_fem7to4", "0", 0 },
	{ "2:swctrlmap4_TX5g_fem7to4", "0", 0 },
	{ "2:swctrlmap4_RX5g_fem7to4", "0", 0 },
	{ "2:swctrlmap4_RXByp5g_fem7to4", "0", 0 },
	{ "2:swctrlmap4_misc5g_fem7to4", "0", 0 },
	{ "2:srom13sig", "0x4D55", 0 },
#endif
	{ 0, 0, 0 }
};
#elif defined(RTAC1200G) || defined(RTAC1200GP)
struct nvram_tuple bcm4360ac_defaults[] = {
	{ "0:xtalfreq", "20000", 0 },
	{ "0:opo", "68", 0 },
	{ "0:aa2g", "0x3", 0 },
	{ "0:ag0", "0x2", 0 },
	{ "0:ag1", "0x2", 0 },
	{ "0:ag2", "0xff", 0 },
	{ "0:ag3", "0xff", 0 },
	{ "0:bxa2g", "0x0", 0 },
	{ "0:rssisav2g", "0x0", 0 },
	{ "0:rssismc2g", "0x0", 0 },
	{ "0:rssismf2g", "0x0", 0 },
	{ "0:tri2g", "0x0", 0 },
	{ "0:rxpo2g", "0x0", 0 },
	{ "0:txchain", "0x3", 0 },
	{ "0:rxchain", "0x3", 0 },
	{ "0:antswitch", "0x0", 0 },
	{ "0:tssipos2g", "0x1", 0 },
//	{ "0:extpagain2g", "0x0", 0 },
	{ "0:pdetrange2g", "0x3", 0 },
	{ "0:triso2g", "0x4", 0 },
	{ "0:antswctl2g", "0x0", 0 },
	{ "0:elna2g", "0x2", 0 },
	{ "0:itt2ga0", "0x20", 0 },
	{ "0:itt2ga1", "0x20", 0 },
	{ "0:ledbh0", "11", 0 },
	{ "0:ledbh1", "11", 0 },
	{ "0:ledbh2", "11", 0 },
	{ "0:ledbh3", "7", 0 },
	{ "0:leddc", "0xFFFF", 0 },
	{ "0:temps_period", "5", 0 },
	{ "0:tempthresh", "120", 0 },
	{ "0:temps_hysteresis", "5", 0 },
	{ "0:phycal_tempdelta", "0", 0 },
	{ "0:tempoffset", "0", 0 },
	{ "sb/1/aa2g", "0", 0 },
	{ "sb/1/aa5g", "3", 0 },
	{ "sb/1/aga0", "0", 0 },
	{ "sb/1/aga1", "0", 0 },
	{ "sb/1/aga2", "0", 0 },
	{ "sb/1/txchain", "3", 0 },
	{ "sb/1/rxchain", "3", 0 },
	{ "sb/1/antswitch", "0", 0 },
	{ "sb/1/femctrl", "15", 0 },
	{ "sb/1/subband5gver", "4", 0 },
	{ "sb/1/gainctrlsph", "0", 0 },
	{ "sb/1/papdcap5g", "0", 0 },
	{ "sb/1/tworangetssi5g", "0", 0 },
	{ "sb/1/pdgain5g", "0", 0 },
//	{ "sb/1/epagain5g", "0", 0 },
	{ "sb/1/tssiposslope5g", "1", 0 },
	{ "sb/1/AvVmid_c0", "2,135,2,135,2,135,2,135,2,135", 0 },
	{ "sb/1/AvVmid_c1", "2,145,2,145,2,145,2,145,2,145", 0 },
	{ "sb/1/paparambwver", "3", 0 },
	{ "paparambwver", "3", 0 },
	{ "sb/1/pdoffset5gsubbanda0", "0x0000", 0 },
	{ "sb/1/pdoffset5gsubbanda1", "0x0000", 0 },
	{ "sb/1/rxgains5gelnagaina0", "3", 0 },
	{ "sb/1/rxgains5gtrelnabypa0", "1", 0 },
	{ "sb/1/rxgains5gtrisoa0", "5", 0 },
	{ "sb/1/rxgains5gmelnagaina0", "3", 0 },
	{ "sb/1/rxgains5gmtrelnabypa0", "1", 0 },
	{ "sb/1/rxgains5gmtrisoa0", "5", 0 },
	{ "sb/1/rxgains5ghelnagaina0", "3", 0 },
	{ "sb/1/rxgains5ghtrelnabypa0", "1", 0 },
	{ "sb/1/rxgains5ghtrisoa0", "5", 0 },
	{ "sb/1/rxgains5gelnagaina1", "3", 0 },
	{"sb/1/rxgains5gtrelnabypa1", "1", 0 },
	{"sb/1/rxgains5gtrisoa1", "5", 0 },
	{"sb/1/rxgains5gmelnagaina1", "3", 0 },
	{"sb/1/rxgains5gmtrelnabypa1", "1", 0 },
	{"sb/1/rxgains5gmtrisoa1", "5", 0 },
	{"sb/1/rxgains5ghelnagaina1", "3", 0 },
	{"sb/1/rxgains5ghtrelnabypa1", "1", 0 },
	{"sb/1/rxgains5ghtrisoa1", "5", 0 },
	{"sb/1/rssi_delta_5gl_c0", "-3,0,-5,0,-5,0", 0 },
	{"sb/1/rssi_delta_5gml_c0", "-4,0,-5,0,-5,0", 0 },
	{"sb/1/rssi_delta_5gmu_c0", "-4,0,-5,0,-5,0", 0 },
	{"sb/1/rssi_delta_5gh_c0", "-4,0,-6,0,-5,0", 0 },
	{"sb/1/rssi_delta_5gl_c1", "-3,0,-5,0,-5,0", 0 },
	{"sb/1/rssi_delta_5gml_c1", "-4,0,-5,0,-5,0", 0 },
	{"sb/1/rssi_delta_5gmu_c1", "-4,0,-5,0,-5,0", 0 },
	{"sb/1/rssi_delta_5gh_c1", "-4,0,-6,0,-5,0", 0 },
	{"sb/1/rssi_delta_2g_c0", "4,5,4,5", 0 },
	{"sb/1/rssi_delta_2g_c1", "2,3,2,3", 0 },
	{"sb/1/ledbh0", "11", 0 },
	{"sb/1/ledbh1", "11", 0 },
	{"sb/1/ledbh2", "11", 0 },
	{"sb/1/ledbh3", "11", 0 },
	{"sb/1/ledbh11", "0x7", 0 },
	{"sb/1/leddc", "0xFFFF", 0 },
	{"sb/1/temps_period", "5", 0 },
	{"sb/1/tempthresh", "120", 0 },
	{"sb/1/temps_hysteresis", "5", 0 },
	{"sb/1/phycal_tempdelta", "15", 0 },
	{"sb/1/tempoffset", "0", 0 },
	{0, 0, 0 }
};
#else
struct nvram_tuple bcm4360ac_defaults[] = {
	{ "0:ledbh10", "7", 0 },
	{ "1:ledbh10", "7", 0 },
#ifdef RTCONFIG_LEDARRAY
	{ "0:ledbh0", "7", 0 },
	{ "0:ledbh9", "7", 0 },
	{ "1:ledbh0", "7", 0 },
	{ "1:ledbh9", "7", 0 },
#endif
	{ "0:temps_period", "5", 0 },
	{ "0:tempthresh", "120", 0 },
	{ "0:temps_hysteresis", "5", 0 },
	{ "0:phycal_tempdelta", "0", 0 },
	{ "0:tempoffset", "0", 0 },
	{ "1:temps_period", "5", 0 },
	{ "1:tempthresh", "120", 0 },
	{ "1:temps_hysteresis", "5", 0 },
	{ "1:phycal_tempdelta", "0", 0 },
	{ "1:tempoffset", "0", 0 },
	{ 0, 0, 0 }
};
#endif
#endif
#endif

/* nvram override default setting for Media Router */
struct nvram_tuple router_defaults_override_type1[] = {
	{ "router_disable", "1", 0 },		/* lan_proto=static lan_stp=0 wan_proto=disabled */
	{ "lan_stp", "0", 0 },			/* LAN spanning tree protocol */
	{ "wl_wmf_bss_enable", "1", 0 },	/* WMF Enable for IPTV Media or WiFi+PLC */
	{ "wl_reg_mode", "h", 0 },		/* Regulatory: 802.11H(h) */
#ifndef RTCONFIG_BCM7
	{ "wl_taf_enable", "1", 0 },		/* Enable TAF */
	{ "wl_taf_rule", "0x15", 0 },		/* Default TAF rule on SSID, RATE and AID */

	/* EBOS feature Media router default */
	{ "wl_ebos_enable", "0", 0 },		/* EBOS feature on */
	{ "wl_ebos_flags", "0x68", 0 },		/* 104(0x68) video links */
	{ "wl_ebos_transit", "-1", 0 },		/* transit limit for video links */
	{ "wl_ebos_prr_flags", "0xa41", 0 },	/* pseudo-round robin data links */
	{ "wl_ebos_prr_threshold", "0x0f000000", 0 },	/* pseudo-round robin threshold */
	{ "wl_ebos_prr_transit", "-1", 0 },	/* pseudo-round robin transit limit */

	/* Airtime fairness */
	{ "wl_atf", "1", 0 },			/* ATF feature on */
#else
	{ "wl_taf_enable", "0", 0 },		/* Disable TAF */
#endif
#ifdef __CONFIG_EMF__
	{ "emf_enable", "1", 0 },		/* Enable EMF by default */
	{ "wl_wmf_ucigmp_query", "1", 0 },	/* Enable Converting IGMP Query to ucast */
#ifdef RTCONFIG_BCMARM
	{ "wl_wmf_ucast_upnp", "1", 0 },	/* Enable upnp to ucast conversion */
	{ "wl_wmf_igmpq_filter", "1", 0 },	/* Enable igmp query filter */
#endif
#endif
	{ "wl_acs_fcs_mode", "1", 0 },		/* Enable acsd fcs mode */
#ifndef RTCONFIG_BCM7
	{ "wl_acs_dfs", "2", 0 },		/* Enable first DFS chan Selection */
#endif
	{ "wl_dcs_csa_unicast", "1", 0 },	/* Enable unicast CSA */
	/* Exclude ACSD to select 140l, 144u, 140/80, 144/80 to compatible with Ducati 11N */
	{ "wl_acs_excl_chans", "0xd98e,0xd88e,0xe28a,0xe38a", 0 },
	{ "wl_pspretend_retry_limit", "5", 0 }, /* Enable PsPretend */
#ifndef RTCONFIG_BCM7
	{ "wl_pspretend_threshold", "0", 0 },	/* Disable PsPretend Threshold */
	{ "wl_acs_chan_dwell_time", "70", 0 },	/* WAR for AP to stay on DFS chan */
	{ "wl_frameburst", "on", 0 },		/* BRCM Frambursting mode (off|on) */
#if defined (RTCONFIG_BCMARM) && !defined (RTCONFIG_BCM7)
	{ "frameburst_dyn", "0", 0 },		/* Frameburst controlled dynamically if on */
#endif
#endif
	{ "wl_amsdu", "off", 0 },		/* Default IPTV AMSDU setting */
#ifdef RTCONFIG_BCMARM && !defined (RTCONFIG_BCM_7114)
	{ "wl_rx_amsdu_in_ampdu", "off", 0 },	/* Media RX AMSDU In AMPDU setting */
#endif
	{ "wl_cal_period", "0", 0 },		/* Disable periodic cal */
#ifdef RTCONFIG_BCMARM
	{ "wl_psta_inact", "0", 0 },		/* PSTA inactivity timer */
	{ "wl_srl", "11", 0},			/* Short Retry Limit */
	{ "wl_lrl", "11", 0},			/* Long Retry Limit */
#endif
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
 * NOTE: This routine special-cases the variable wl_bss_enabled. It will
 * return the normal default value if asked for wl_ or wl0_. But it will
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
#ifdef RTCONFIG_BCMARM
#ifndef RTCONFIG_BCM7
#ifdef __CONFIG_HSPOT__
	if (strcmp(fixed_name, "wl_bss_hs2_enabled") == 0) {
		if (name[3] == '.' || name[4] == '.') { /* Virtual interface */
			return "0";
		}
	}
#endif /* __CONFIG_HSPOT__ */
#endif
	if (!strcmp(nvram_safe_get("devicemode"), "1")) {
		for (idx = 0; router_defaults_override_type1[idx].name != NULL; idx++) {
			if (strcmp(router_defaults_override_type1[idx].name, fixed_name) == 0) {
				return router_defaults_override_type1[idx].value;
			}
		}
	}
#endif

	/* check name wlx_xxx first */
	if ((strncmp(name, "wl", 2) == 0) && isdigit(name[2]) && (name[3] == '_'))
	for (idx = 0; router_defaults[idx].name != NULL; idx++) {
		if (strcmp(router_defaults[idx].name, name) == 0) {
			return router_defaults[idx].value;
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

	return;

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
