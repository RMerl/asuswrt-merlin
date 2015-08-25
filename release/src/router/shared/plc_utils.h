#ifndef _plc_utils_h_
#define _plc_utils_h_

/*
 * The number of bytes in an PLC Key.
 */
#ifndef PLC_KEY_LEN
#define	PLC_KEY_LEN		16
#endif

extern int key_atoe(const char *a, unsigned char *e);
extern char *key_etoa(const unsigned char *e, char *a);

extern int getPLC_MAC(char *abuf);
extern int getPLC_NMK(char *abuf);
extern int getPLC_PWD(void);
extern int getPLC_para(int addr);
extern int setPLC_para(const char *abuf, int addr);
extern void ate_ctl_plc_led(void);
extern int set_plc_all_led_onoff(int on);

#define BOOT_NVM_PATH		"/tmp/asus.nvm"
#define BOOT_PIB_PATH		"/tmp/asus.pib"

extern void set_plc_flag(int flag);
extern int load_plc_setting(void);
extern void save_plc_setting(void);

extern void turn_led_pwr_off(void);

#endif /* _plc_utils_h_ */
