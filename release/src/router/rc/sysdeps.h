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
extern char *get_staifname(int band);
extern char *get_vphyifname(int band);
#endif
extern void fini_wl(void);
extern void init_syspara(void);
extern void post_syspara(void);
extern void generate_wl_para(int unit, int subunit);
#if defined(RTCONFIG_RALINK)
extern void reinit_hwnat(int unit);
#elif defined(RTCONFIG_QCA)

#if defined(RTCONFIG_SOC_QCA9557) || defined(RTCONFIG_QCA953X) || defined(RTCONFIG_QCA956X)
#define reinit_hwnat(unit) reinit_sfe(unit)
extern void reinit_sfe(int unit);
static inline void tweak_wifi_ps(const char *wif) { }
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

extern char *get_lan_hwaddr(void);

#endif
