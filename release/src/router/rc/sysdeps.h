#ifndef _RC_SYSDEPS_H_
#define _RC_SYSDEPS_H_

/* sysdeps/init-PLATFORM.c */
extern void init_devs(void);
extern void generate_switch_para(void);
extern void init_switch();
extern void config_switch();
extern int switch_exist(void);
extern void init_wl(void);
extern void fini_wl(void);
extern void init_syspara(void);
extern void generate_wl_para(int unit, int subunit);
extern int is_module_loaded(const char *module);
extern void reinit_hwnat();
extern char *get_wlifname(int unit, int subunit, int subunit_x, char *buf);
extern int wl_exist(char *ifname, int band);
#endif
