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

#define PLC_LOCK_FILE		"/tmp/plc_lock"
#if defined(PLN12)
#define BOOT_NVM_PATH		"/tmp/asus.nvm"
#else
#define BOOT_NVM_PATH		"/usr/local/bin/asus.nvm"
#endif
#define BOOT_PIB_PATH		"/tmp/asus.pib"

extern int default_plc_write_to_flash(void);
extern void set_plc_flag(int flag);
extern int load_plc_setting(void);
extern void save_plc_setting(void);

extern void turn_led_pwr_off(void);

struct remote_plc {
	char mac[18];
	char pwd[20];
	int status;	/*
			1: connect and known password
			2: disconnect and known password
			3: connect and unknown password
			*/
	int tx;
	int rx;
};

extern int apply_private_name(char *pnn, char *nv);
extern int get_connected_plc(struct remote_plc **rplc);
extern int get_known_plc(struct remote_plc **rplc);
extern int trigger_plc_pair(void);
extern int add_remote_plc(char *mac, char *pwd);

#endif /* _plc_utils_h_ */
