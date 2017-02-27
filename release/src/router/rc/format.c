#include <rc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void adjust_merlin_config(void)
{
#ifdef RTCONFIG_OPENVPN
	if(!nvram_is_empty("vpn_server_clientlist")) {
		nvram_set("vpn_serverx_clientlist", nvram_safe_get("vpn_server_clientlist"));
		nvram_unset("vpn_server_clientlist");
	}
#endif

/* migrate dhcpc_options to wanxxx_clientid */
	char *oldclientid = nvram_safe_get("wan0_dhcpc_options");
	if (*oldclientid) {
		nvram_set("wan0_clientid", oldclientid);
		nvram_unset("wan0_dhcpc_options");
	}

	oldclientid = nvram_safe_get("wan1_dhcpc_options");
	if (*oldclientid) {
		nvram_set("wan1_clientid", oldclientid);
		nvram_unset("wan1_dhcpc_options");
	}

/* Migrate to Asus's new tri-state sshd_enable to our dual nvram setup */
	if (nvram_match("sshd_enable", "1")) {
		if (nvram_match("sshd_wan", "0"))
			nvram_set("sshd_enable", "2");  // LAN-only
		// else stay WAN+LAN
		nvram_unset("sshd_wan");
	}
}

void adjust_url_urlelist(void)
{
	char *nv, *nvp, *b, *chk;
	char *url;
	char  replacebox[2048], rerule[256];
	int   cnt = 0;
	int   RESAVE = OFF;
	
	nv = nvp = strdup(nvram_safe_get("url_rulelist"));
	
	/* Change setting format 
	From:[<str1<str2<str3]
	To:  [<1>ALL>str1<1>ALL>str2<1>ALL>str3] 
	*/
	memset(replacebox, 0, sizeof(replacebox));
	while (nvp && (b = strsep(&nvp, "<")) != NULL) {
		chk = strdup(b);
		//dbg("[%s(%d)] %s\n", __FUNCTION__, __LINE__, chk);
		while( *chk != '\0') {
			if(*chk == '>') cnt++;
			chk++;
		}
		if (cnt != 2) {
			if (vstrsep(b, ">", &url) != 1) continue;
			if (*url) {
				memset(rerule, 0, sizeof(rerule));
				snprintf(rerule, sizeof(rerule), "<1>ALL>%s", url);
				//dbg("[%s(%d)] %s\n", __FUNCTION__, __LINE__, rerule);
				strcat(replacebox, rerule);
				RESAVE = ON;
			}
		}
		cnt = 0;
	}
	if (RESAVE) 
		nvram_set("url_rulelist", replacebox);
	free(nv);
}

void adjust_ddns_config(void)
{
	char *server;

	server = nvram_safe_get("ddns_server_x");
	if(!strcmp(server, "WWW.GOOGLE-DDNS.COM") || 
	   !strcmp(server, "WWW.GOOGLE-DOMAINS.COM")) {
		nvram_set("ddns_server_x", "DOMAINS.GOOGLE.COM");
	}
	
}
