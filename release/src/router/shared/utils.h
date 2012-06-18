#ifndef __SHUTILS_H__
#define __SHUTILS_H__


#define PPP_PSEUDO_IP	"10.64.64.64"
#define PPP_PSEUDO_NM	"255.255.255.255"
#define PPP_PSEUDO_GW	"10.112.112.112"


//	extern int diag_led(int type, int act);
//	extern int C_led(int i);
//	extern int get_single_ip(char *ipaddr, int which);
//	extern char *get_mac_from_ip(char *ip);
//	extern struct dns_lists *get_dns_list(int no);
//	extern char *get_wan_face(void);
//	extern int check_wan_link(int num);
//	extern char *get_complete_lan_ip(char *ip);
//	extern int get_int_len(int num);
//	extern int file_to_buf(char *path, char *buf, int len);
//	int buf_to_file(char *path, char *buf);
//	extern pid_t* find_pid_by_name( char* pidName);
//	extern int find_pid_by_ps(char* pidName);
//	extern int *find_all_pid_by_ps(char* pidName);

//	extern long convert_ver(char *ver);
//	extern int check_flash(void);
//	extern int check_now_boot(void);
//	extern int check_hw_type(void);
//	extern int is_exist(char *filename);

//	extern void encode(char *buf, int len);
//	extern void decode(char *buf, int len);

//	extern int set_register_value(unsigned short port_addr, unsigned short option_content);
//	extern unsigned long get_register_value(unsigned short id, unsigned short num);

//	struct wl_assoc_mac * get_wl_assoc_mac(int *c);

/*
enum {
	WL = 0,
	DIAG = 1,
	SES_LED1 = 2,
	SES_LED2 = 3,
	SES_BUTTON = 4,
	DMZ = 7
};

enum {
	START_LED,
	STOP_LED,
	MALFUNCTION_LED
};
*/

//	enum { UNKNOWN_BOOT = -1, PMON_BOOT, CFE_BOOT };

//	enum { BCM4702_CHIP, BCM4712_CHIP, BCM5325E_CHIP, BCM4704_BCM5325F_CHIP, BCM5352E_CHIP, NO_DEFINE_CHIP };

//	enum { FIRST, SECOND };

//	enum { SYSLOG_LOG=1, SYSLOG_DEBUG, CONSOLE_ONLY, LOG_CONSOLE, DEBUG_CONSOLE };

/*
typedef enum {
	ACT_IDLE, 
	ACT_TFTP_UPGRADE, 
	ACT_WEB_UPGRADE, 
	ACT_WEBS_UPGRADE, 
	ACT_SW_RESTORE, 
	ACT_HW_RESTORE,
	ACT_ERASE_NVRAM,
	ACT_NVRAM_COMMIT,
	ACT_UPGRADE_DONE
} ACTION;
#define ACTION_FILE	"/tmp/action"
#define ACTION(cmd)	buf_to_file(ACTION_FILE, cmd)
extern int check_action(void);
*/

/*
struct dns_lists {
	int num_servers;
	char dns_server[4][16];
};

struct wl_assoc_mac {
	char mac[18];
};

#define	PROTO_DHCP	0
#define	PROTO_STATIC	1
#define	PROTO_PPPOE	2
#define	PROTO_PPTP	3
#define	PROTO_L2TP	4
#define	PROTO_HB	5
#define PROTO_EARTHLINK 6
#define	PROTO_ERROR	-1
*/



#define split(word, wordlist, next, delim) \
	for (next = wordlist, \
	     strncpy(word, next, sizeof(word)), \
	     word[(next=strstr(next, delim)) ? strstr(word, delim) - word : sizeof(word) - 1] = '\0', \
	     next = next ? next + sizeof(delim) - 1 : NULL ; \
	     strlen(word); \
	     next = next ? : "", \
	     strncpy(word, next, sizeof(word)), \
	     word[(next=strstr(next, delim)) ? strstr(word, delim) - word : sizeof(word) - 1] = '\0', \
	     next = next ? next + sizeof(delim) - 1 : NULL)

#define STRUCT_LEN(name)    sizeof(name)/sizeof(name[0])

#endif
