#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <bcmnvram.h>
#include <shutils.h>

#include "qcsapi_output.h"
#include "qcsapi_rpc_common/client/find_host_addr.h"

#include "qcsapi.h"
#include "qcsapi_rpc/client/qcsapi_rpc_client.h"
#include "qcsapi_rpc/generated/qcsapi_rpc.h"
#include "qcsapi_driver.h"
#include "call_qcsapi.h"

#define MAX_RETRY_TIMES 30

static int s_c_rpc_use_udp = 0;

char cmd[32];
#if 0
void inc_mac(char *mac, int plus);
#endif
int rpc_qcsapi_init()
{
	const char *host;
	CLIENT *clnt;
	int retry = 0;

	/* setup RPC based on udp protocol */
	do {
		// remove due to ATE command output format
		// fprintf(stderr, "#%d attempt to create RPC connection\n", retry + 1);

		host = client_qcsapi_find_host_addr(0, NULL);
		if (!host) {
			fprintf(stderr, "Cannot find the host\n");
			sleep(1);
			continue;
		}

		if (!s_c_rpc_use_udp) {
			clnt = clnt_create(host, QCSAPI_PROG, QCSAPI_VERS, "tcp");
		} else {
			clnt = clnt_create(host, QCSAPI_PROG, QCSAPI_VERS, "udp");
		}

		if (clnt == NULL) {
			clnt_pcreateerror(host);
			sleep(1);
			continue;
		} else {
			client_qcsapi_set_rpcclient(clnt);
			return 0;
		}
	} while (retry++ < MAX_RETRY_TIMES);

	return -1;
}

int
setCountryCode_5G_qtn(const char *cc)
{
	int ret;
	char value[20] = {0};

	if( cc==NULL || !isValidCountryCode(cc) )
		return 0;

	ret = rpc_qcsapi_init();
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_bootcfg_update_parameter("ccode_5g", cc);
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_bootcfg_get_parameter("ccode_5g", value, sizeof(value));
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}

	sprintf(cmd, "asuscfe1:ccode=%s", cc);
	eval("nvram", "set", cmd );
	puts(nvram_safe_get("1:ccode"));

	return 1;
}

int
getCountryCode_5G_qtn(void)
{
	puts(nvram_safe_get("1:ccode"));

	return 0;
}

int setRegrev_5G_qtn(const char *regrev)
{
	int ret;
	char value[20] = {0};

	if( regrev==NULL || !isValidRegrev(regrev) )
		return 0;

	ret = rpc_qcsapi_init();
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_bootcfg_update_parameter("regrev_5g", regrev);
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_bootcfg_get_parameter("regrev_5g", value, sizeof(value));
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}

	memset(cmd, 0, 32);
	sprintf(cmd, "asuscfe1:regrev=%s", regrev);
	eval("nvram", "set", cmd );
	puts(nvram_safe_get("1:regrev"));
	return 1;
}

int
getRegrev_5G_qtn(void)
{
	puts(nvram_safe_get("1:regrev"));
	return 0;
}

int setMAC_5G_qtn(const char *mac)
{
	int ret;
	char cmd_l[64];
	char value[20] = {0};

	if( mac==NULL || !isValidMacAddr(mac) )
		return 0;

	ret = rpc_qcsapi_init();
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_bootcfg_update_parameter("ethaddr", mac);
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
#if 0
	inc_mac(mac, 1);
#endif
	ret = qcsapi_bootcfg_update_parameter("wifiaddr", mac);
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_bootcfg_get_parameter("ethaddr", value, sizeof(value));
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}

	memset(cmd_l, 0, 64);
	sprintf(cmd_l, "asuscfe1:macaddr=%s", mac);
	eval("nvram", "set", cmd_l );
	// puts(nvram_safe_get("1:macaddr"));

	puts(value);
	return 1;
}

int getMAC_5G_qtn(void)
{
	int ret;
	char value[20] = {0};

	ret = rpc_qcsapi_init();
	ret = qcsapi_bootcfg_get_parameter("ethaddr", value, sizeof(value));
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	puts(value);
	return 1;
}

//int start_wireless_qtn(void)
//{
//	int ret;
//
//	ret = rpc_qcsapi_init();
//
//	ret = qcsapi_wifi_set_SSID(WIFINAME,nvram_safe_get("wl1_ssid"));
//	if(!nvram_match("wl1_auth_mode_x", "open")){
//		rpc_qcsapi_set_beacon_type(nvram_safe_get("wl1_auth_mode_x"));
//		rpc_qcsapi_set_WPA_encryption_modes(nvram_safe_get("wl1_crypto"));
//		rpc_qcsapi_set_key_passphrase(nvram_safe_get("wl1_wpa_psk"));
//	}
//	return 1;
//}

int setAllLedOn_qtn(void)
{
	int ret;

	ret = rpc_qcsapi_init();
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_led_set(1, 1);
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_eth_phy_power_control(0, "eth1_1");
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	return 0;
}

int setAllLedOff_qtn(void)
{
	int ret;

	ret = rpc_qcsapi_init();
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_led_set(1, 0);
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}

	ret = qcsapi_eth_phy_power_control(1, "eth1_1");
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	return 0;
}

