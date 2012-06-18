#ifndef CONFIG_H
#define CONFIG_H

typedef u8 macaddr[ETH_ALEN];

struct hostapd_radius_server {
	struct in_addr addr;
	int port;
	u8 *shared_secret;
	size_t shared_secret_len;
};

struct rtapd_config {
	char iface_name[IFNAMSIZ + 1];
	int SsidNum;
	
	int DefaultKeyID[MAX_MBSSID_NUM];
	int individual_wep_key_len[MAX_MBSSID_NUM];
	int	individual_wep_key_idx[MAX_MBSSID_NUM];
	u8 IEEE8021X_ikey[MAX_MBSSID_NUM][WEP8021X_KEY_LEN];
	
#define HOSTAPD_MODULE_IEEE80211 BIT(0)
#define HOSTAPD_MODULE_IEEE8021X BIT(1)
#define HOSTAPD_MODULE_RADIUS BIT(2)

	enum { HOSTAPD_DEBUG_NO = 0, HOSTAPD_DEBUG_MINIMAL = 1,
	       HOSTAPD_DEBUG_VERBOSE = 2,
	       HOSTAPD_DEBUG_MSGDUMPS = 3 } debug; /* debug verbosity level */
	int daemonize; /* fork into background */

	struct in_addr own_ip_addr;
	
	/* RADIUS Authentication and Accounting servers in priority order */
#if MULTIPLE_RADIUS
	struct hostapd_radius_server *mbss_auth_servers[MAX_MBSSID_NUM], *mbss_auth_server[MAX_MBSSID_NUM];
	int mbss_num_auth_servers[MAX_MBSSID_NUM];
#else
	struct hostapd_radius_server *auth_servers, *auth_server;
	int num_auth_servers;
#endif
	
	int	 num_eap_if;
	char eap_if_name[MAX_MBSSID_NUM][IFNAMSIZ];

	int	 num_preauth_if;
	char preauth_if_name[MAX_MBSSID_NUM][IFNAMSIZ];

	int radius_retry_primary_interval;

	int session_timeout_set;
	int session_timeout_interval;

	/* The initialization value used for the quietWhile timer. 
	   Its default value is 60 s; it can be set by management 
	   to any value in the range from 0 to 65535 s. 

	   NOTE 1 - The Authenticator may increase the value of quietPeriod 
	   per Port to ignore authorization failures for longer periods 
	   of time after a number of authorization failures have occurred.*/	
	int 	quiet_interval;

	u8		nasId[MAX_MBSSID_NUM][32];
	int		nasId_len[MAX_MBSSID_NUM];
};


struct rtapd_config * Config_read(int ioctl_sock, char *prefix_name);
void Config_free(struct rtapd_config *conf);


#endif /* CONFIG_H */
