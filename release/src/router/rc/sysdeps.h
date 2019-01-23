#ifndef _RC_SYSDEPS_H_
#define _RC_SYSDEPS_H_
#include <rtconfig.h>

/* sysdeps/init-PLATFORM.c */
extern void init_devs(void);
extern void generate_switch_para(void);
extern void init_switch();
extern void config_switch();
extern int switch_exist(void);
extern void init_wl(void);
#if defined(RTCONFIG_QCA)
extern void load_wifi_driver(void);
extern void load_testmode_wifi_driver(void);
extern char *__get_wlifname(int band, int subunit, char *buf);
extern int get_wlsubnet(int band, const char *ifname);
extern char *get_staifname(int band);
extern char *get_vphyifname(int band);
#endif
extern void fini_wl(void);
extern void init_syspara(void);
extern void post_syspara(void);
#ifndef CONFIG_BCMWL5
extern void generate_wl_para(int unit, int subunit);
#else
extern void generate_wl_para(char *ifname, int unit, int subunit);
#endif
#if defined(RTCONFIG_RALINK)
extern void reinit_hwnat(int unit);
#elif defined(RTCONFIG_QCA)

#if defined(RTCONFIG_SOC_QCA9557) || defined(RTCONFIG_QCA953X) || defined(RTCONFIG_QCA956X) || defined(RTCONFIG_SOC_IPQ40XX)
#define reinit_hwnat(unit) reinit_sfe(unit)
extern void reinit_sfe(int unit);
static inline void tweak_wifi_ps(const char *wif) { }
#elif defined(RTCONFIG_SOC_IPQ8064)
#define reinit_hwnat(unit) reinit_ecm(unit)
extern void reinit_ecm(int unit);
extern void tweak_wifi_ps(const char *wif);
#else
#error
#endif

#else
/* Broadcom */
static inline void reinit_hwnat(int unit) { }
#endif
extern char *get_wlifname(int unit, int subunit, int subunit_x, char *buf);
extern int wl_exist(char *ifname, int band);
extern void set_wan_tag(char *interface);

extern int wlcconnect_core(void);
extern int wlcscan_core(char *ofile, char *wif);
extern void wps_oob_both(void);
extern void start_wsc(void);
extern const char *get_wifname(int band);
extern int get_channel_list_via_driver(int unit, char *buffer, int len);
extern int get_channel_list_via_country(int unit, const char *country_code, char *buffer, int len);
extern char *wlc_nvname(char *keyword);

#if defined(RTCONFIG_RALINK)
extern int getWscStatus(int unit);
#elif defined(RTCONFIG_QCA)
extern char *getWscStatus(int unit);
#endif

#if defined(RTCONFIG_DSL)
extern void init_switch_dsl(void);
extern void config_switch_dsl(void);
extern void config_switch_dsl_set_lan(void);
#endif

#ifdef RTCONFIG_BCMFA
#include <etioctl.h>
#define	ARPHRD_ETHER	1	/* ARP Header */
#define	CTF_FA_DISABLED	0
#define	CTF_FA_BYPASS	1
#define	CTF_FA_NORMAL	2
#define	CTF_FA_SW_ACC	3
#define	FA_ON(mode)	(mode == CTF_FA_BYPASS || mode == CTF_FA_NORMAL)
int fa_mode;
void fa_mode_init();
#endif
#endif
