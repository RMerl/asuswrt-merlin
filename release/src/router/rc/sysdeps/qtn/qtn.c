#include <stdio.h>
#include <stdlib.h>

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
#define WIFINAME "wifi0"

static int s_c_rpc_use_udp = 0;

void inc_mac(char *mac, int plus);

int c_rpc_qcsapi_init()
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

int setMAC_5G_qtn(const char *mac)
{
	int ret;
	char cmd_l[64];
	char value[20] = {0};

	if( mac==NULL || !isValidMacAddr(mac) )
		return 0;

	ret = c_rpc_qcsapi_init();
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_bootcfg_update_parameter("ethaddr", mac);
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	inc_mac(mac, 1);
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

	ret = c_rpc_qcsapi_init();
	ret = qcsapi_bootcfg_get_parameter("ethaddr", value, sizeof(value));
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	puts(value);
	return 1;
}

