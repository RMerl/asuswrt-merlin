#include <termios.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <time.h>
#include <errno.h>
#include <paths.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <sys/klog.h>
#include <bcmnvram.h>
#include <rc.h>
#include <shutils.h>
#ifdef LINUX26
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#endif
#include <shared.h>

extern void get_country_code_from_rc(char* country_code);
extern struct nvram_tuple router_defaults[];

//find the wan setting from WAN List and convert to 
//wan_xxx for original rc flow.

void convert_dsl_config_num()
{
	int config_num = 0;
	if (nvram_match("dsl0_enable","1")) config_num++;
	if (nvram_match("dsl1_enable","1")) config_num++;
	if (nvram_match("dsl2_enable","1")) config_num++;
	if (nvram_match("dsl3_enable","1")) config_num++;
	if (nvram_match("dsl4_enable","1")) config_num++;
	if (nvram_match("dsl5_enable","1")) config_num++;
	if (nvram_match("dsl6_enable","1")) config_num++;
	if (nvram_match("dsl7_enable","1")) config_num++;
	nvram_set_int("dslx_config_num", config_num); 
}

void convert_dsl_wan()
{
	int conv_dsl_to_wan0 = 0;
	int conv_dsl_to_wan1 = 0;

#ifdef RTCONFIG_DUALWAN
	if (get_dualwan_primary()==WANS_DUALWAN_IF_DSL)
	{
		conv_dsl_to_wan0 = 1;
	}

	if (get_dualwan_secondary()==WANS_DUALWAN_IF_DSL)
	{
		conv_dsl_to_wan1 = 1;
	}	
#else
	conv_dsl_to_wan0 = 1;
#endif

	if (conv_dsl_to_wan0)
	{
		nvram_set("wan_nat_x",nvram_safe_get("dslx_nat"));
		nvram_set("wan_upnp_enable",nvram_safe_get("dslx_upnp_enable"));	
		nvram_set("wan_enable",nvram_safe_get("dslx_link_enable")); 
		nvram_set("wan_dhcpenable_x",nvram_safe_get("dslx_DHCPClient"));	
		nvram_set("wan_ipaddr_x",nvram_safe_get("dslx_ipaddr"));	
		nvram_set("wan_upnp_enable",nvram_safe_get("dslx_upnp_enable"));		
		nvram_set("wan_netmask_x",nvram_safe_get("dslx_netmask"));		
		nvram_set("wan_gateway_x",nvram_safe_get("dslx_gateway"));		
		nvram_set("wan_dnsenable_x",nvram_safe_get("dslx_dnsenable"));		
		nvram_set("wan_dns1_x",nvram_safe_get("dslx_dns1"));			
		nvram_set("wan_dns2_x",nvram_safe_get("dslx_dns2"));			
		nvram_set("wan_pppoe_username",nvram_safe_get("dslx_pppoe_username"));			
		nvram_set("wan_pppoe_passwd",nvram_safe_get("dslx_pppoe_passwd"));				
		nvram_set("wan_pppoe_idletime",nvram_safe_get("dslx_pppoe_idletime"));			
		nvram_set("wan_pppoe_mtu",nvram_safe_get("dslx_pppoe_mtu"));				
		nvram_set("wan_pppoe_mru",nvram_safe_get("dslx_pppoe_mtu"));			
		nvram_set("wan_pppoe_service",nvram_safe_get("dslx_pppoe_service"));				
		nvram_set("wan_pppoe_ac",nvram_safe_get("dslx_pppoe_ac"));				
		nvram_set("wan_pppoe_options_x",nvram_safe_get("dslx_pppoe_options"));			
//		nvram_set("wan_pppoe_relay",nvram_safe_get("dslx_pppoe_relay"));				
		nvram_set("wan_dns1_x",nvram_safe_get("dslx_dns1"));			
		nvram_set("wan_dns1_x",nvram_safe_get("dslx_dns1"));				
	}

	if (conv_dsl_to_wan0)
	{
		nvram_set("wan0_nat_x",nvram_safe_get("dslx_nat"));
		nvram_set("wan0_upnp_enable",nvram_safe_get("dslx_upnp_enable"));	
		nvram_set("wan0_enable",nvram_safe_get("dslx_link_enable"));	
		nvram_set("wan0_dhcpenable_x",nvram_safe_get("dslx_DHCPClient"));	
		nvram_set("wan0_ipaddr_x",nvram_safe_get("dslx_ipaddr"));	
		nvram_set("wan0_upnp_enable",nvram_safe_get("dslx_upnp_enable"));		
		nvram_set("wan0_netmask_x",nvram_safe_get("dslx_netmask")); 	
		nvram_set("wan0_gateway_x",nvram_safe_get("dslx_gateway")); 	
		nvram_set("wan0_dnsenable_x",nvram_safe_get("dslx_dnsenable")); 	
		nvram_set("wan0_dns1_x",nvram_safe_get("dslx_dns1"));			
		nvram_set("wan0_dns2_x",nvram_safe_get("dslx_dns2"));			
		nvram_set("wan0_pppoe_username",nvram_safe_get("dslx_pppoe_username")); 		
		nvram_set("wan0_pppoe_passwd",nvram_safe_get("dslx_pppoe_passwd")); 			
		nvram_set("wan0_pppoe_idletime",nvram_safe_get("dslx_pppoe_idletime")); 		
		nvram_set("wan0_pppoe_mtu",nvram_safe_get("dslx_pppoe_mtu"));				
		nvram_set("wan0_pppoe_mru",nvram_safe_get("dslx_pppoe_mtu"));			
		nvram_set("wan0_pppoe_service",nvram_safe_get("dslx_pppoe_service"));				
		nvram_set("wan0_pppoe_ac",nvram_safe_get("dslx_pppoe_ac")); 			
		nvram_set("wan0_pppoe_options_x",nvram_safe_get("dslx_pppoe_options")); 		
//		nvram_set("wan0_pppoe_relay",nvram_safe_get("dslx_pppoe_relay"));				
		nvram_set("wan0_dns1_x",nvram_safe_get("dslx_dns1"));			
		nvram_set("wan0_dns1_x",nvram_safe_get("dslx_dns1"));				
		nvram_set("wan0_hwaddr_x",nvram_safe_get("dslx_hwaddr"));					
	}

	if (conv_dsl_to_wan1)
	{
		nvram_set("wan1_nat_x",nvram_safe_get("dslx_nat"));
		nvram_set("wan1_upnp_enable",nvram_safe_get("dslx_upnp_enable"));	
		nvram_set("wan1_enable",nvram_safe_get("dslx_link_enable"));	
		nvram_set("wan1_dhcpenable_x",nvram_safe_get("dslx_DHCPClient"));	
		nvram_set("wan1_ipaddr_x",nvram_safe_get("dslx_ipaddr"));	
		nvram_set("wan1_upnp_enable",nvram_safe_get("dslx_upnp_enable"));		
		nvram_set("wan1_netmask_x",nvram_safe_get("dslx_netmask")); 	
		nvram_set("wan1_gateway_x",nvram_safe_get("dslx_gateway")); 	
		nvram_set("wan1_dnsenable_x",nvram_safe_get("dslx_dnsenable")); 	
		nvram_set("wan1_dns1_x",nvram_safe_get("dslx_dns1"));			
		nvram_set("wan1_dns2_x",nvram_safe_get("dslx_dns2"));			
		nvram_set("wan1_pppoe_username",nvram_safe_get("dslx_pppoe_username")); 		
		nvram_set("wan1_pppoe_passwd",nvram_safe_get("dslx_pppoe_passwd")); 			
		nvram_set("wan1_pppoe_idletime",nvram_safe_get("dslx_pppoe_idletime")); 		
		nvram_set("wan1_pppoe_mtu",nvram_safe_get("dslx_pppoe_mtu"));				
		nvram_set("wan1_pppoe_mru",nvram_safe_get("dslx_pppoe_mtu"));			
		nvram_set("wan1_pppoe_service",nvram_safe_get("dslx_pppoe_service"));				
		nvram_set("wan1_pppoe_ac",nvram_safe_get("dslx_pppoe_ac")); 			
		nvram_set("wan1_pppoe_options_x",nvram_safe_get("dslx_pppoe_options")); 		
//		nvram_set("wan1_pppoe_relay",nvram_safe_get("dslx_pppoe_relay"));				
		nvram_set("wan1_dns1_x",nvram_safe_get("dslx_dns1"));			
		nvram_set("wan1_dns1_x",nvram_safe_get("dslx_dns1"));				
		nvram_set("wan1_hwaddr_x",nvram_safe_get("dslx_hwaddr"));					
	}
	

	if (conv_dsl_to_wan0)
	{
		if (nvram_match("dsl0_proto","pppoe") || nvram_match("dsl0_proto","pppoa")) {
			nvram_set("wan0_proto","pppoe");
			/* Turn off DHCP on MAN interface */
			nvram_set_int("wan0_dhcpenable_x", 2);
		}
		else if (nvram_match("dsl0_proto","ipoa")) {
			nvram_set("wan0_proto","static");
		}
		else if (nvram_match("dsl0_proto","bridge")) {
			// disable nat
			nvram_set("wan0_nat_x","0");

			/* Paul add 2012/7/13, for Bridge connection type wan0_proto set to dhcp, and dsl_proto set as bridge */
			nvram_set("wan0_proto","dhcp");
			nvram_set("dsl_proto","bridge");
		}
		else if (nvram_match("dsl0_proto","mer")) {
			if (nvram_match("dslx_DHCPClient","1")) {
				nvram_set("wan0_proto","dhcp");
			}
			else {
				nvram_set("wan0_proto","static");		
			}
		}
	}

	if (conv_dsl_to_wan1)
	{
		if (nvram_match("dsl0_proto","pppoe") || nvram_match("dsl0_proto","pppoa")) {
			nvram_set("wan1_proto","pppoe");
			/* Turn off DHCP on MAN interface */
			nvram_set_int("wan1_dhcpenable_x", 2);
		}
		else if (nvram_match("dsl0_proto","ipoa")) {
			nvram_set("wan1_proto","static");
		}
		else if (nvram_match("dsl0_proto","bridge")) {
			// disable nat
			nvram_set("wan1_nat_x","0");		

			/* Paul add 2012/7/13, for Bridge connection type wan0_proto set to dhcp, and dsl_proto set as bridge */
			nvram_set("wan1_proto","dhcp");
			nvram_set("dsl_proto","bridge");
		}
		else if (nvram_match("dsl0_proto","mer")) {
			if (nvram_match("dslx_DHCPClient","1")) {
				nvram_set("wan1_proto","dhcp");
			}
			else {
				nvram_set("wan1_proto","static");		
			}
		}
	}
	

	if (conv_dsl_to_wan0)
	{
		nvram_set("wan_proto",nvram_safe_get("wan0_proto"));
	}	

}


void remove_dsl_autodet(void)
{
	int AdslReady = 0;
	int WaitAdslCnt;
	int x;
	char wan_if[9];
	char wan_num[2];	

	// not autodet , direct return
	if (nvram_match("dsltmp_autodet_state","")) return;

	// ask auto_det to quit
	nvram_set("dsltmp_adslatequit","1");
	
	for(x=2; x<=8; x++) {
		sprintf(wan_if, "eth2.1.%d", x);
		eval("ifconfig", wan_if, "down");				
	}

	nvram_set("dsltmp_autodet_state","");
}


void convert_dsl_wan_settings(int req)
{
	char buf[32];

	if (req == 0)
	{
		convert_dsl_config_num();
	}

	if (req == 1)
	{
		convert_dsl_wan();
	}

	if (req == 2)
	{
		convert_dsl_config_num();
		eval("req_dsl_drv", "reloadpvc");		
		convert_dsl_wan();		
	}

}

void
dsl_defaults(void)
{
	struct nvram_tuple *t;
	char prefix[]="dslXXXXXX_", tmp[100];
	char word[256], *next;
	int unit;

	for(unit=0;unit<8;unit++) {	
		snprintf(prefix, sizeof(prefix), "dsl%d_", unit);

		for (t = router_defaults; t->name; t++) {
			if(strncmp(t->name, "dsl_", 4)!=0) continue;

			if (!nvram_get(strcat_r(prefix, &t->name[4], tmp))) {
				//_dprintf("_set %s = %s\n", tmp, t->value);
				nvram_set(tmp, t->value);
			}
		}
		unit++;
	}

	// dump trx header
	// this is for upgrading check
	// if trx is same, the upgrade procedure will be skiped
	fprintf(stderr, "dump trx header..\n");
	eval("dd", "if=/dev/mtd3", "of=/tmp/trx_hdr.bin", "count=1");

	convert_dsl_wan_settings(0);
}

// from in.h
#define IN_CLASSB_NET           0xffff0000
#define IN_CLASSB_HOST          (0xffffffff & ~IN_CLASSB_NET)
// from net.h
#define LINKLOCAL_ADDR	0xa9fe0000
// from zcip.c and revised
// Pick a random link local IP address on 169.254/16, except that
// the first and last 256 addresses are reserved.
static void pick_a_random_ipv4(char* buf_ip)
{
        unsigned tmp;
		unsigned int tmp_ip;

        do {
                tmp = rand() & IN_CLASSB_HOST;
        } while (tmp > (IN_CLASSB_HOST - 0x0200));
        tmp_ip = (LINKLOCAL_ADDR + 0x0100) + tmp;
		sprintf(buf_ip,"%d.%d.%d.%d",tmp_ip>>24,(tmp_ip>>16)&0xff,(tmp_ip>>8)&0xff,tmp_ip&0xff);
}


void start_dsl()
{
	// todo: is it necessary? 
	init_dsl_before_start_wan();
	char *argv_tp_init[] = {"tp_init", NULL};	
	int pid;

	// if setting cfg file is from annex a, annex b will has invalid values
	if(nvram_match("productid", "DSL-N55U-B"))
	{
		if (nvram_get_int("dslx_annex") != 0)
		{
			nvram_set_int("dslx_annex", 0); //Paul add 2012/8/22, for Annex B model should always be 0
			nvram_set_int("dslx_config_num", 0);
		}
	}
	
	eval("mkdir", "/tmp/adsl");

	/* Paul comment 2012/7/25, the "never overcommit" policy would cause Ralink WiFi driver kernel panic when configure DUT through external registrar. *
	 * So let this value be the default which is 0, the kernel will estimate the amount of free memory left when userspace requests more memory. */
	//system("echo 2 > /proc/sys/vm/overcommit_memory");
	

#ifdef RTCONFIG_DUALWAN
	if (get_dualwan_secondary()==WANS_DUALWAN_IF_NONE)
	{
		if (get_dualwan_primary()!=WANS_DUALWAN_IF_DSL)
		{
			// it does not need to start dsl driver when using other modes
			// but it still need to run tp_init to have firmware version info
			printf("get modem info\n");
			eval("tp_init", "eth_wan_mode_only");			
			return;
		}
	}
#endif

	int config_num = nvram_get_int("dslx_config_num");
	_eval(argv_tp_init, NULL, 0, &pid);
	// IPTV
	if (nvram_match("x_Setting", "1") && config_num > 1)
	{
		int x;
		char wan_if[9];
		char wan_num[2];		
		char buf_ip[32];
		eval("brctl", "addbr", "br1");
		for(x=2; x<=config_num; x++) {
			sprintf(wan_if, "eth2.1.%d", x);
			sprintf(wan_num, "%d", x);
			eval("vconfig", "add", "eth2.1", wan_num);
			eval("ifconfig", wan_if, "up");				
			eval("brctl", "addif", "br1", wan_if);
		}	
		eval("brctl", "addif", "br1", "eth2.3");
		// it needs assign a IPv4 address to enable packet forwarding
		pick_a_random_ipv4(buf_ip);
		eval("ifconfig", "br1", "up");
		eval("ifconfig", "br1", buf_ip);		
	}

	// auto detection
	if (nvram_match("x_Setting", "0") && config_num == 0)
	{
		int x;
		char wan_if[9];
		char wan_num[2];		
		char country_value[8];
		char cmd_buf[64];
		char *argv_auto_det[] = {"auto_det", country_value, NULL};		
		int pid;		
		for(x=2; x<=8; x++) {
			sprintf(wan_if, "eth2.1.%d", x);
			sprintf(wan_num, "%d", x);
			eval("vconfig", "add", "eth2.1", wan_num);
			eval("ifconfig", wan_if, "up");				
		}
		//
		nvram_set("dsltmp_autodet_state", "Detecting");
		// call auto detection with country code
		get_country_code_from_rc(country_value);
		_eval(argv_auto_det, NULL, 0, &pid);
	}
}

void stop_dsl()
{
	// dsl service need not to stop
}

// a workaround handler, can be removed after bug found
void init_dsl_before_start_wan(void)
{
	// eth2 could not start up in original initialize routine
	// it must put eth2 start-up code here
	dbG("enable eth2 and power up all LAN ports\n");
	eval("ifconfig", "eth", "up");
	eval("8367r", "14");
	eval("brctl", "addif", "br0", "eth2.2");
}

