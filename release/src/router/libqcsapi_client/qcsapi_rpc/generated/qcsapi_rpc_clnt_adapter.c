
/*
*  ########## DO NOT EDIT ###########

Automatically generated on Sat Mar 15 01:27:34 PDT 2014

*
* Adapter from qcsapi.h functions
* to RPC client functions.
*/

#include <stdio.h>
#include <errno.h>
#include <inttypes.h>

#include <qcsapi.h>
#include "qcsapi_rpc.h"
#include <qcsapi_rpc/client/qcsapi_rpc_client.h>

static int retries_limit = 3;

/* Default timeout can be changed using clnt_control() */
static struct timeval __timeout = { 25, 0 };

static CLIENT *__clnt = NULL;

static const int debug = 0;

static CLIENT *qcsapi_adapter_get_client(void)
{
	if (__clnt == NULL) {
		fprintf(stderr, "%s: client is null!\n", __FUNCTION__);
		exit (1);
	}

	return __clnt;
}

void client_qcsapi_set_rpcclient(CLIENT * clnt)
{
	__clnt = clnt;
}

static client_qcsapi_callback_pre_t __pre_callback = NULL;
static client_qcsapi_callback_post_t __post_callback = NULL;
static client_qcsapi_callback_reconnect_t __reconnect_callback = NULL;

void client_qcsapi_set_callbacks(client_qcsapi_callback_pre_t pre,
		client_qcsapi_callback_post_t post,
		client_qcsapi_callback_reconnect_t reconnect)
{
	__pre_callback = pre;
	__post_callback = post;
	__reconnect_callback = reconnect;
}

#define client_qcsapi_pre() __client_qcsapi_pre(__FUNCTION__)
static void __client_qcsapi_pre(const char *func)
{
	if (__pre_callback) {
		__pre_callback(func);
	}
}

#define client_qcsapi_post(x) __client_qcsapi_post(__FUNCTION__, (x))
static void __client_qcsapi_post(const char *func, int was_error)
{
	if (__post_callback) {
		__post_callback(func, was_error);
	}
}

#define client_qcsapi_reconnect() __client_qcsapi_reconnect(__FUNCTION__)
static void __client_qcsapi_reconnect(const char *func)
{
	if (__reconnect_callback) {
		__reconnect_callback(func);
	}
}

int qcsapi_bootcfg_get_parameter(const char * param_name, char * param_value, const size_t max_param_len)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_bootcfg_get_parameter_request __req;
	struct qcsapi_bootcfg_get_parameter_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (param_value == NULL) {
		return -EFAULT;
	}
	__req.param_name = (str)param_name;
	if (param_value) {
		param_value[0] = 0;
	}
	__req.max_param_len = (unsigned long)max_param_len;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_BOOTCFG_GET_PARAMETER_REMOTE,
				(xdrproc_t)xdr_qcsapi_bootcfg_get_parameter_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_bootcfg_get_parameter_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_bootcfg_get_parameter call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (param_value && __resp.param_value)
		strcpy(param_value,
			__resp.param_value);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_bootcfg_update_parameter(const char * param_name, const char * param_value)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_bootcfg_update_parameter_request __req;
	struct qcsapi_bootcfg_update_parameter_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.param_name = (str)param_name;
	__req.param_value = (str)param_value;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_BOOTCFG_UPDATE_PARAMETER_REMOTE,
				(xdrproc_t)xdr_qcsapi_bootcfg_update_parameter_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_bootcfg_update_parameter_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_bootcfg_update_parameter call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_bootcfg_commit()
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_bootcfg_commit_request __req;
	struct qcsapi_bootcfg_commit_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_BOOTCFG_COMMIT_REMOTE,
				(xdrproc_t)xdr_qcsapi_bootcfg_commit_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_bootcfg_commit_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_bootcfg_commit call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_telnet_enable(const qcsapi_unsigned_int onoff)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_telnet_enable_request __req;
	struct qcsapi_telnet_enable_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.onoff = (unsigned int)onoff;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_TELNET_ENABLE_REMOTE,
				(xdrproc_t)xdr_qcsapi_telnet_enable_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_telnet_enable_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_telnet_enable call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_scs_cce_channels(const char * ifname, qcsapi_unsigned_int * p_prev_channel, qcsapi_unsigned_int * p_cur_channel)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_scs_cce_channels_request __req;
	struct qcsapi_wifi_get_scs_cce_channels_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_prev_channel == NULL || p_cur_channel == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_SCS_CCE_CHANNELS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_scs_cce_channels_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_scs_cce_channels_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_scs_cce_channels call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_prev_channel)
		*p_prev_channel = __resp.p_prev_channel;
	if (p_cur_channel)
		*p_cur_channel = __resp.p_cur_channel;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_scs_enable(const char * ifname, uint16_t enable_val)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_scs_enable_request __req;
	struct qcsapi_wifi_scs_enable_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.enable_val = (unsigned int)enable_val;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SCS_ENABLE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_scs_enable_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_scs_enable_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_scs_enable call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_scs_switch_channel(const char * ifname)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_scs_switch_channel_request __req;
	struct qcsapi_wifi_scs_switch_channel_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SCS_SWITCH_CHANNEL_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_scs_switch_channel_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_scs_switch_channel_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_scs_switch_channel call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_scs_verbose(const char * ifname, uint16_t enable_val)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_scs_verbose_request __req;
	struct qcsapi_wifi_set_scs_verbose_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.enable_val = (unsigned int)enable_val;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_SCS_VERBOSE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_scs_verbose_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_scs_verbose_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_scs_verbose call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_scs_status(const char * ifname, qcsapi_unsigned_int * p_scs_status)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_scs_status_request __req;
	struct qcsapi_wifi_get_scs_status_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_scs_status == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_SCS_STATUS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_scs_status_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_scs_status_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_scs_status call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_scs_status)
		*p_scs_status = __resp.p_scs_status;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_scs_smpl_enable(const char * ifname, uint16_t enable_val)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_scs_smpl_enable_request __req;
	struct qcsapi_wifi_set_scs_smpl_enable_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.enable_val = (unsigned int)enable_val;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_SCS_SMPL_ENABLE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_scs_smpl_enable_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_scs_smpl_enable_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_scs_smpl_enable call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_scs_smpl_dwell_time(const char * ifname, uint16_t scs_sample_time)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_scs_smpl_dwell_time_request __req;
	struct qcsapi_wifi_set_scs_smpl_dwell_time_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.scs_sample_time = (unsigned int)scs_sample_time;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_SCS_SMPL_DWELL_TIME_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_scs_smpl_dwell_time_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_scs_smpl_dwell_time_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_scs_smpl_dwell_time call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_scs_sample_intv(const char * ifname, uint16_t scs_sample_intv)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_scs_sample_intv_request __req;
	struct qcsapi_wifi_set_scs_sample_intv_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.scs_sample_intv = (unsigned int)scs_sample_intv;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_SCS_SAMPLE_INTV_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_scs_sample_intv_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_scs_sample_intv_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_scs_sample_intv call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_scs_intf_detect_intv(const char * ifname, uint16_t scs_intf_detect_intv)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_scs_intf_detect_intv_request __req;
	struct qcsapi_wifi_set_scs_intf_detect_intv_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.scs_intf_detect_intv = (unsigned int)scs_intf_detect_intv;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_SCS_INTF_DETECT_INTV_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_scs_intf_detect_intv_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_scs_intf_detect_intv_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_scs_intf_detect_intv call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_scs_thrshld(const char * ifname, const char * scs_param_name, uint16_t scs_threshold)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_scs_thrshld_request __req;
	struct qcsapi_wifi_set_scs_thrshld_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.scs_param_name = (str)scs_param_name;
	__req.scs_threshold = (unsigned int)scs_threshold;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_SCS_THRSHLD_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_scs_thrshld_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_scs_thrshld_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_scs_thrshld call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_scs_report_only(const char * ifname, uint16_t scs_report_only)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_scs_report_only_request __req;
	struct qcsapi_wifi_set_scs_report_only_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.scs_report_only = (unsigned int)scs_report_only;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_SCS_REPORT_ONLY_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_scs_report_only_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_scs_report_only_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_scs_report_only call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_scs_stat_report(const char * ifname, struct qcsapi_scs_ranking_rpt * scs_rpt)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_scs_stat_report_request __req;
	struct qcsapi_wifi_get_scs_stat_report_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (scs_rpt == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_SCS_STAT_REPORT_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_scs_stat_report_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_scs_stat_report_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_scs_stat_report call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	scs_rpt->num = __resp.scs_rpt_num;
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(scs_rpt->chan); __i_0++) {
			scs_rpt->chan[__i_0] = __resp.scs_rpt_chan[__i_0];
		}
	}
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(scs_rpt->dfs); __i_0++) {
			scs_rpt->dfs[__i_0] = __resp.scs_rpt_dfs[__i_0];
		}
	}
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(scs_rpt->txpwr); __i_0++) {
			scs_rpt->txpwr[__i_0] = __resp.scs_rpt_txpwr[__i_0];
		}
	}
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(scs_rpt->metric); __i_0++) {
			scs_rpt->metric[__i_0] = __resp.scs_rpt_metric[__i_0];
		}
	}
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(scs_rpt->cca_intf); __i_0++) {
			scs_rpt->cca_intf[__i_0] = __resp.scs_rpt_cca_intf[__i_0];
		}
	}
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(scs_rpt->pmbl_ap); __i_0++) {
			scs_rpt->pmbl_ap[__i_0] = __resp.scs_rpt_pmbl_ap[__i_0];
		}
	}
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(scs_rpt->pmbl_sta); __i_0++) {
			scs_rpt->pmbl_sta[__i_0] = __resp.scs_rpt_pmbl_sta[__i_0];
		}
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_scs_currchan_report(const char * ifname, struct qcsapi_scs_currchan_rpt * scs_currchan_rpt)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_scs_currchan_report_request __req;
	struct qcsapi_wifi_get_scs_currchan_report_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (scs_currchan_rpt == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_SCS_CURRCHAN_REPORT_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_scs_currchan_report_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_scs_currchan_report_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_scs_currchan_report call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	scs_currchan_rpt->chan = __resp.scs_currchan_rpt_chan;
	scs_currchan_rpt->cca_try = __resp.scs_currchan_rpt_cca_try;
	scs_currchan_rpt->cca_idle = __resp.scs_currchan_rpt_cca_idle;
	scs_currchan_rpt->cca_busy = __resp.scs_currchan_rpt_cca_busy;
	scs_currchan_rpt->cca_intf = __resp.scs_currchan_rpt_cca_intf;
	scs_currchan_rpt->cca_tx = __resp.scs_currchan_rpt_cca_tx;
	scs_currchan_rpt->tx_ms = __resp.scs_currchan_rpt_tx_ms;
	scs_currchan_rpt->rx_ms = __resp.scs_currchan_rpt_rx_ms;
	scs_currchan_rpt->pmbl = __resp.scs_currchan_rpt_pmbl;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_autochan_report(const char * ifname, struct qcsapi_autochan_rpt * autochan_rpt)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_autochan_report_request __req;
	struct qcsapi_wifi_get_autochan_report_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (autochan_rpt == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_AUTOCHAN_REPORT_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_autochan_report_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_autochan_report_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_autochan_report call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	autochan_rpt->num = __resp.autochan_rpt_num;
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(autochan_rpt->chan); __i_0++) {
			autochan_rpt->chan[__i_0] = __resp.autochan_rpt_chan[__i_0];
		}
	}
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(autochan_rpt->dfs); __i_0++) {
			autochan_rpt->dfs[__i_0] = __resp.autochan_rpt_dfs[__i_0];
		}
	}
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(autochan_rpt->txpwr); __i_0++) {
			autochan_rpt->txpwr[__i_0] = __resp.autochan_rpt_txpwr[__i_0];
		}
	}
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(autochan_rpt->metric); __i_0++) {
			autochan_rpt->metric[__i_0] = __resp.autochan_rpt_metric[__i_0];
		}
	}
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(autochan_rpt->numbeacons); __i_0++) {
			autochan_rpt->numbeacons[__i_0] = __resp.autochan_rpt_numbeacons[__i_0];
		}
	}
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(autochan_rpt->cci); __i_0++) {
			autochan_rpt->cci[__i_0] = __resp.autochan_rpt_cci[__i_0];
		}
	}
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(autochan_rpt->aci); __i_0++) {
			autochan_rpt->aci[__i_0] = __resp.autochan_rpt_aci[__i_0];
		}
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_scs_cca_intf_smth_fctr(const char * ifname, uint8_t smth_fctr_noxp, uint8_t smth_fctr_xped)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_scs_cca_intf_smth_fctr_request __req;
	struct qcsapi_wifi_set_scs_cca_intf_smth_fctr_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.smth_fctr_noxp = (unsigned int)smth_fctr_noxp;
	__req.smth_fctr_xped = (unsigned int)smth_fctr_xped;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_SCS_CCA_INTF_SMTH_FCTR_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_scs_cca_intf_smth_fctr_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_scs_cca_intf_smth_fctr_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_scs_cca_intf_smth_fctr call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_scs_chan_mtrc_mrgn(const char * ifname, uint8_t chan_mtrc_mrgn)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_scs_chan_mtrc_mrgn_request __req;
	struct qcsapi_wifi_set_scs_chan_mtrc_mrgn_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.chan_mtrc_mrgn = (unsigned int)chan_mtrc_mrgn;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_SCS_CHAN_MTRC_MRGN_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_scs_chan_mtrc_mrgn_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_scs_chan_mtrc_mrgn_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_scs_chan_mtrc_mrgn call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_scs_cca_intf(const char * ifname, const qcsapi_unsigned_int the_channel, int * p_cca_intf)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_scs_cca_intf_request __req;
	struct qcsapi_wifi_get_scs_cca_intf_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_cca_intf == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.the_channel = (unsigned int)the_channel;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_SCS_CCA_INTF_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_scs_cca_intf_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_scs_cca_intf_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_scs_cca_intf call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_cca_intf)
		*p_cca_intf = __resp.p_cca_intf;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_scs_param_report(const char * ifname, struct qcsapi_scs_param_rpt * p_scs_param_rpt, uint32_t param_num)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_scs_param_report_request __req;
	struct qcsapi_wifi_get_scs_param_report_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_scs_param_rpt == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.param_num = (unsigned long)param_num;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_SCS_PARAM_REPORT_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_scs_param_report_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_scs_param_report_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_scs_param_report call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	p_scs_param_rpt->scs_cfg_param = __resp.p_scs_param_rpt_scs_cfg_param;
	p_scs_param_rpt->scs_signed_param_flag = __resp.p_scs_param_rpt_scs_signed_param_flag;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_scs_dfs_reentry_request(const char * ifname, qcsapi_unsigned_int * p_scs_dfs_reentry_request)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_scs_dfs_reentry_request_request __req;
	struct qcsapi_wifi_get_scs_dfs_reentry_request_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_scs_dfs_reentry_request == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_SCS_DFS_REENTRY_REQUEST_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_scs_dfs_reentry_request_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_scs_dfs_reentry_request_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_scs_dfs_reentry_request call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_scs_dfs_reentry_request)
		*p_scs_dfs_reentry_request = __resp.p_scs_dfs_reentry_request;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_start_ocac(const char * ifname, uint16_t channel)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_start_ocac_request __req;
	struct qcsapi_wifi_start_ocac_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.channel = (unsigned int)channel;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_START_OCAC_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_start_ocac_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_start_ocac_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_start_ocac call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_stop_ocac(const char * ifname)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_stop_ocac_request __req;
	struct qcsapi_wifi_stop_ocac_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_STOP_OCAC_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_stop_ocac_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_stop_ocac_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_stop_ocac call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_ocac_status(const char * ifname, qcsapi_unsigned_int * status)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_ocac_status_request __req;
	struct qcsapi_wifi_get_ocac_status_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (status == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_OCAC_STATUS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_ocac_status_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_ocac_status_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_ocac_status call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (status)
		*status = __resp.status;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_ocac_dwell_time(const char * ifname, uint16_t dwell_time)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_ocac_dwell_time_request __req;
	struct qcsapi_wifi_set_ocac_dwell_time_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.dwell_time = (unsigned int)dwell_time;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_OCAC_DWELL_TIME_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_ocac_dwell_time_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_ocac_dwell_time_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_ocac_dwell_time call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_ocac_duration(const char * ifname, uint16_t duration)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_ocac_duration_request __req;
	struct qcsapi_wifi_set_ocac_duration_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.duration = (unsigned int)duration;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_OCAC_DURATION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_ocac_duration_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_ocac_duration_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_ocac_duration call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_ocac_cac_time(const char * ifname, uint16_t cac_time)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_ocac_cac_time_request __req;
	struct qcsapi_wifi_set_ocac_cac_time_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.cac_time = (unsigned int)cac_time;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_OCAC_CAC_TIME_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_ocac_cac_time_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_ocac_cac_time_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_ocac_cac_time call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_ocac_report_only(const char * ifname, uint16_t enable)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_ocac_report_only_request __req;
	struct qcsapi_wifi_set_ocac_report_only_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.enable = (unsigned int)enable;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_OCAC_REPORT_ONLY_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_ocac_report_only_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_ocac_report_only_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_ocac_report_only call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_ocac_thrshld(const char * ifname, const char * param_name, uint16_t threshold)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_ocac_thrshld_request __req;
	struct qcsapi_wifi_set_ocac_thrshld_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.param_name = (str)param_name;
	__req.threshold = (unsigned int)threshold;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_OCAC_THRSHLD_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_ocac_thrshld_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_ocac_thrshld_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_ocac_thrshld call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_init()
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_init_request __req;
	struct qcsapi_init_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_INIT_REMOTE,
				(xdrproc_t)xdr_qcsapi_init_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_init_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_init call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_console_disconnect()
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_console_disconnect_request __req;
	struct qcsapi_console_disconnect_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_CONSOLE_DISCONNECT_REMOTE,
				(xdrproc_t)xdr_qcsapi_console_disconnect_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_console_disconnect_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_console_disconnect call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_system_get_time_since_start(qcsapi_unsigned_int * p_elapsed_time)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_system_get_time_since_start_request __req;
	struct qcsapi_system_get_time_since_start_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_elapsed_time == NULL) {
		return -EFAULT;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SYSTEM_GET_TIME_SINCE_START_REMOTE,
				(xdrproc_t)xdr_qcsapi_system_get_time_since_start_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_system_get_time_since_start_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_system_get_time_since_start call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_elapsed_time)
		*p_elapsed_time = __resp.p_elapsed_time;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_get_system_status(qcsapi_unsigned_int * p_status)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_get_system_status_request __req;
	struct qcsapi_get_system_status_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_status == NULL) {
		return -EFAULT;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_GET_SYSTEM_STATUS_REMOTE,
				(xdrproc_t)xdr_qcsapi_get_system_status_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_get_system_status_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_get_system_status call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_status)
		*p_status = __resp.p_status;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_config_get_parameter(const char * ifname, const char * param_name, char * param_value, const size_t max_param_len)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_config_get_parameter_request __req;
	struct qcsapi_config_get_parameter_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (param_value == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.param_name = (str)param_name;
	if (param_value) {
		param_value[0] = 0;
	}
	__req.max_param_len = (unsigned long)max_param_len;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_CONFIG_GET_PARAMETER_REMOTE,
				(xdrproc_t)xdr_qcsapi_config_get_parameter_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_config_get_parameter_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_config_get_parameter call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (param_value && __resp.param_value)
		strcpy(param_value,
			__resp.param_value);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_config_update_parameter(const char * ifname, const char * param_name, const char * param_value)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_config_update_parameter_request __req;
	struct qcsapi_config_update_parameter_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.param_name = (str)param_name;
	__req.param_value = (str)param_value;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_CONFIG_UPDATE_PARAMETER_REMOTE,
				(xdrproc_t)xdr_qcsapi_config_update_parameter_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_config_update_parameter_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_config_update_parameter call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_file_path_get_config(const qcsapi_file_path_config e_file_path, char * file_path, qcsapi_unsigned_int path_size)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_file_path_get_config_request __req;
	struct qcsapi_file_path_get_config_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (file_path == NULL) {
		return -EFAULT;
	}
	__req.e_file_path = (int)e_file_path;
	if (file_path) {
		file_path[0] = 0;
	}
	__req.path_size = (unsigned int)path_size;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_FILE_PATH_GET_CONFIG_REMOTE,
				(xdrproc_t)xdr_qcsapi_file_path_get_config_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_file_path_get_config_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_file_path_get_config call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (file_path && __resp.file_path)
		strcpy(file_path,
			__resp.file_path);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_file_path_set_config(const qcsapi_file_path_config e_file_path, const char * new_path)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_file_path_set_config_request __req;
	struct qcsapi_file_path_set_config_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.e_file_path = (int)e_file_path;
	__req.new_path = (str)new_path;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_FILE_PATH_SET_CONFIG_REMOTE,
				(xdrproc_t)xdr_qcsapi_file_path_set_config_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_file_path_set_config_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_file_path_set_config call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_restore_default_config(int flag)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_restore_default_config_request __req;
	struct qcsapi_restore_default_config_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.flag = (int)flag;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_RESTORE_DEFAULT_CONFIG_REMOTE,
				(xdrproc_t)xdr_qcsapi_restore_default_config_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_restore_default_config_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_restore_default_config call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_interface_enable(const char * ifname, const int enable_flag)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_interface_enable_request __req;
	struct qcsapi_interface_enable_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.enable_flag = (int)enable_flag;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_INTERFACE_ENABLE_REMOTE,
				(xdrproc_t)xdr_qcsapi_interface_enable_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_interface_enable_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_interface_enable call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_interface_get_status(const char * ifname, char * interface_status)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_interface_get_status_request __req;
	struct qcsapi_interface_get_status_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (interface_status == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (interface_status) {
		interface_status[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_INTERFACE_GET_STATUS_REMOTE,
				(xdrproc_t)xdr_qcsapi_interface_get_status_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_interface_get_status_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_interface_get_status call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (interface_status && __resp.interface_status)
		strcpy(interface_status,
			__resp.interface_status);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_interface_get_counter(const char * ifname, qcsapi_counter_type qcsapi_counter, qcsapi_unsigned_int * p_counter_value)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_interface_get_counter_request __req;
	struct qcsapi_interface_get_counter_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_counter_value == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.qcsapi_counter = (int)qcsapi_counter;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_INTERFACE_GET_COUNTER_REMOTE,
				(xdrproc_t)xdr_qcsapi_interface_get_counter_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_interface_get_counter_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_interface_get_counter call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_counter_value)
		*p_counter_value = __resp.p_counter_value;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_interface_get_mac_addr(const char * ifname, qcsapi_mac_addr current_mac_addr)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_interface_get_mac_addr_request __req;
	struct qcsapi_interface_get_mac_addr_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (current_mac_addr == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_INTERFACE_GET_MAC_ADDR_REMOTE,
				(xdrproc_t)xdr_qcsapi_interface_get_mac_addr_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_interface_get_mac_addr_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_interface_get_mac_addr call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (current_mac_addr && __resp.current_mac_addr)
		memcpy(current_mac_addr,
			__resp.current_mac_addr,
			sizeof(__resp.current_mac_addr));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_interface_set_mac_addr(const char * ifname, const qcsapi_mac_addr interface_mac_addr)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_interface_set_mac_addr_request __req;
	struct qcsapi_interface_set_mac_addr_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (interface_mac_addr == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	memcpy(__req.interface_mac_addr, interface_mac_addr, sizeof(const qcsapi_mac_addr));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_INTERFACE_SET_MAC_ADDR_REMOTE,
				(xdrproc_t)xdr_qcsapi_interface_set_mac_addr_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_interface_set_mac_addr_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_interface_set_mac_addr call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_pm_get_counter(const char * ifname, qcsapi_counter_type qcsapi_counter, const char * pm_interval, qcsapi_unsigned_int * p_counter_value)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_pm_get_counter_request __req;
	struct qcsapi_pm_get_counter_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_counter_value == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.qcsapi_counter = (int)qcsapi_counter;
	__req.pm_interval = (str)pm_interval;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_PM_GET_COUNTER_REMOTE,
				(xdrproc_t)xdr_qcsapi_pm_get_counter_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_pm_get_counter_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_pm_get_counter call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_counter_value)
		*p_counter_value = __resp.p_counter_value;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_pm_get_elapsed_time(const char * pm_interval, qcsapi_unsigned_int * p_elapsed_time)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_pm_get_elapsed_time_request __req;
	struct qcsapi_pm_get_elapsed_time_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_elapsed_time == NULL) {
		return -EFAULT;
	}
	__req.pm_interval = (str)pm_interval;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_PM_GET_ELAPSED_TIME_REMOTE,
				(xdrproc_t)xdr_qcsapi_pm_get_elapsed_time_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_pm_get_elapsed_time_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_pm_get_elapsed_time call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_elapsed_time)
		*p_elapsed_time = __resp.p_elapsed_time;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_eth_phy_power_control(int on_off, const char * interface)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_eth_phy_power_control_request __req;
	struct qcsapi_eth_phy_power_control_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.on_off = (int)on_off;
	__req.interface = (str)interface;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_ETH_PHY_POWER_CONTROL_REMOTE,
				(xdrproc_t)xdr_qcsapi_eth_phy_power_control_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_eth_phy_power_control_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_eth_phy_power_control call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_mode(const char * ifname, qcsapi_wifi_mode * p_wifi_mode)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_mode_request __req;
	struct qcsapi_wifi_get_mode_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_wifi_mode == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_MODE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_mode_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_mode_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_mode call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_wifi_mode)
		*p_wifi_mode = __resp.p_wifi_mode;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_mode(const char * ifname, const qcsapi_wifi_mode new_wifi_mode)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_mode_request __req;
	struct qcsapi_wifi_set_mode_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.new_wifi_mode = (int)new_wifi_mode;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_MODE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_mode_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_mode_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_mode call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_reload_in_mode(const char * ifname, const qcsapi_wifi_mode new_wifi_mode)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_reload_in_mode_request __req;
	struct qcsapi_wifi_reload_in_mode_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.new_wifi_mode = (int)new_wifi_mode;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_RELOAD_IN_MODE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_reload_in_mode_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_reload_in_mode_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_reload_in_mode call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_rfenable(const char * ifname, const qcsapi_unsigned_int onoff)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_rfenable_request __req;
	struct qcsapi_wifi_rfenable_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.onoff = (unsigned int)onoff;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_RFENABLE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_rfenable_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_rfenable_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_rfenable call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_rfstatus(const char * ifname, qcsapi_unsigned_int * rfstatus)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_rfstatus_request __req;
	struct qcsapi_wifi_rfstatus_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (rfstatus == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_RFSTATUS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_rfstatus_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_rfstatus_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_rfstatus call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (rfstatus)
		*rfstatus = __resp.rfstatus;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_bw(const char * ifname, qcsapi_unsigned_int * p_bw)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_bw_request __req;
	struct qcsapi_wifi_get_bw_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_bw == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_BW_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_bw_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_bw_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_bw call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_bw)
		*p_bw = __resp.p_bw;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_bw(const char * ifname, const qcsapi_unsigned_int bw)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_bw_request __req;
	struct qcsapi_wifi_set_bw_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.bw = (unsigned int)bw;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_BW_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_bw_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_bw_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_bw call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_channel(const char * ifname, qcsapi_unsigned_int * p_current_channel)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_channel_request __req;
	struct qcsapi_wifi_get_channel_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_current_channel == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_CHANNEL_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_channel_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_channel_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_channel call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_current_channel)
		*p_current_channel = __resp.p_current_channel;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_channel(const char * ifname, const qcsapi_unsigned_int new_channel)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_channel_request __req;
	struct qcsapi_wifi_set_channel_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.new_channel = (unsigned int)new_channel;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_CHANNEL_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_channel_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_channel_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_channel call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_chan_pri_inactive(const char * ifname, const qcsapi_unsigned_int channel, const qcsapi_unsigned_int inactive)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_chan_pri_inactive_request __req;
	struct qcsapi_wifi_set_chan_pri_inactive_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.channel = (unsigned int)channel;
	__req.inactive = (unsigned int)inactive;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_CHAN_PRI_INACTIVE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_chan_pri_inactive_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_chan_pri_inactive_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_chan_pri_inactive call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_beacon_interval(const char * ifname, qcsapi_unsigned_int * p_current_intval)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_beacon_interval_request __req;
	struct qcsapi_wifi_get_beacon_interval_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_current_intval == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_BEACON_INTERVAL_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_beacon_interval_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_beacon_interval_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_beacon_interval call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_current_intval)
		*p_current_intval = __resp.p_current_intval;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_beacon_interval(const char * ifname, const qcsapi_unsigned_int new_intval)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_beacon_interval_request __req;
	struct qcsapi_wifi_set_beacon_interval_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.new_intval = (unsigned int)new_intval;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_BEACON_INTERVAL_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_beacon_interval_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_beacon_interval_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_beacon_interval call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_dtim(const char * ifname, qcsapi_unsigned_int * p_dtim)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_dtim_request __req;
	struct qcsapi_wifi_get_dtim_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_dtim == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_DTIM_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_dtim_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_dtim_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_dtim call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_dtim)
		*p_dtim = __resp.p_dtim;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_dtim(const char * ifname, const qcsapi_unsigned_int new_dtim)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_dtim_request __req;
	struct qcsapi_wifi_set_dtim_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.new_dtim = (unsigned int)new_dtim;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_DTIM_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_dtim_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_dtim_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_dtim call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_assoc_limit(const char * ifname, qcsapi_unsigned_int * p_assoc_limit)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_assoc_limit_request __req;
	struct qcsapi_wifi_get_assoc_limit_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_assoc_limit == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_ASSOC_LIMIT_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_assoc_limit_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_assoc_limit_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_assoc_limit call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_assoc_limit)
		*p_assoc_limit = __resp.p_assoc_limit;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_assoc_limit(const char * ifname, const qcsapi_unsigned_int new_assoc_limit)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_assoc_limit_request __req;
	struct qcsapi_wifi_set_assoc_limit_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.new_assoc_limit = (unsigned int)new_assoc_limit;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_ASSOC_LIMIT_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_assoc_limit_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_assoc_limit_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_assoc_limit call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_BSSID(const char * ifname, qcsapi_mac_addr current_BSSID)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_BSSID_request __req;
	struct qcsapi_wifi_get_BSSID_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (current_BSSID == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_BSSID_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_BSSID_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_BSSID_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_BSSID call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (current_BSSID && __resp.current_BSSID)
		memcpy(current_BSSID,
			__resp.current_BSSID,
			sizeof(__resp.current_BSSID));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_SSID(const char * ifname, qcsapi_SSID SSID_str)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_SSID_request __req;
	struct qcsapi_wifi_get_SSID_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (SSID_str == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (SSID_str) {
		SSID_str[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_SSID_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_SSID_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_SSID_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_SSID call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (SSID_str && __resp.SSID_str)
		strcpy(SSID_str,
			__resp.SSID_str);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_SSID(const char * ifname, const qcsapi_SSID SSID_str)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_SSID_request __req;
	struct qcsapi_wifi_set_SSID_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.SSID_str = (str)SSID_str;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_SSID_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_SSID_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_SSID_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_SSID call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_IEEE_802_11_standard(const char * ifname, char * IEEE_802_11_standard)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_IEEE_802_11_standard_request __req;
	struct qcsapi_wifi_get_IEEE_802_11_standard_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (IEEE_802_11_standard == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (IEEE_802_11_standard) {
		IEEE_802_11_standard[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_IEEE_802_11_STANDARD_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_IEEE_802_11_standard_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_IEEE_802_11_standard_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_IEEE_802_11_standard call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (IEEE_802_11_standard && __resp.IEEE_802_11_standard)
		strcpy(IEEE_802_11_standard,
			__resp.IEEE_802_11_standard);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_list_channels(const char * ifname, string_1024 list_of_channels)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_list_channels_request __req;
	struct qcsapi_wifi_get_list_channels_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (list_of_channels == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (list_of_channels) {
		list_of_channels[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_LIST_CHANNELS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_list_channels_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_list_channels_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_list_channels call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (list_of_channels && __resp.list_of_channels)
		strcpy(list_of_channels,
			__resp.list_of_channels);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_mode_switch(uint8_t * p_wifi_mode_switch_setting)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_mode_switch_request __req;
	struct qcsapi_wifi_get_mode_switch_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_wifi_mode_switch_setting == NULL) {
		return -EFAULT;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_MODE_SWITCH_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_mode_switch_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_mode_switch_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_mode_switch call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_wifi_mode_switch_setting)
		*p_wifi_mode_switch_setting = __resp.p_wifi_mode_switch_setting;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_disassociate(const char * ifname)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_disassociate_request __req;
	struct qcsapi_wifi_disassociate_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_DISASSOCIATE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_disassociate_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_disassociate_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_disassociate call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_disconn_info(const char * ifname, qcsapi_disconn_info * disconn_info)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_disconn_info_request __req;
	struct qcsapi_wifi_get_disconn_info_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (disconn_info == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_DISCONN_INFO_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_disconn_info_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_disconn_info_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_disconn_info call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	disconn_info->asso_sta_count = __resp.disconn_info_asso_sta_count;
	disconn_info->disconn_count = __resp.disconn_info_disconn_count;
	disconn_info->sequence = __resp.disconn_info_sequence;
	disconn_info->up_time = __resp.disconn_info_up_time;
	disconn_info->resetflag = __resp.disconn_info_resetflag;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_disable_wps(const char * ifname, int disable_wps)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_disable_wps_request __req;
	struct qcsapi_wifi_disable_wps_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.disable_wps = (int)disable_wps;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_DISABLE_WPS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_disable_wps_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_disable_wps_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_disable_wps call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_associate(const char * ifname, const qcsapi_SSID join_ssid)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_associate_request __req;
	struct qcsapi_wifi_associate_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.join_ssid = (str)join_ssid;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_ASSOCIATE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_associate_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_associate_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_associate call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_start_cca(const char * ifname, int channel, int duration)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_start_cca_request __req;
	struct qcsapi_wifi_start_cca_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.channel = (int)channel;
	__req.duration = (int)duration;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_START_CCA_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_start_cca_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_start_cca_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_start_cca call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_noise(const char * ifname, int * p_noise)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_noise_request __req;
	struct qcsapi_wifi_get_noise_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_noise == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_NOISE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_noise_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_noise_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_noise call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_noise)
		*p_noise = __resp.p_noise;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_rssi_by_chain(const char * ifname, int rf_chain, int * p_rssi)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_rssi_by_chain_request __req;
	struct qcsapi_wifi_get_rssi_by_chain_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_rssi == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.rf_chain = (int)rf_chain;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_RSSI_BY_CHAIN_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_rssi_by_chain_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_rssi_by_chain_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_rssi_by_chain call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_rssi)
		*p_rssi = __resp.p_rssi;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_avg_snr(const char * ifname, int * p_snr)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_avg_snr_request __req;
	struct qcsapi_wifi_get_avg_snr_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_snr == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_AVG_SNR_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_avg_snr_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_avg_snr_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_avg_snr call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_snr)
		*p_snr = __resp.p_snr;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_get_primary_interface(char * ifname, size_t maxlen)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_get_primary_interface_request __req;
	struct qcsapi_get_primary_interface_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (ifname == NULL) {
		return -EFAULT;
	}
	if (ifname) {
		ifname[0] = 0;
	}
	__req.maxlen = (unsigned long)maxlen;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_GET_PRIMARY_INTERFACE_REMOTE,
				(xdrproc_t)xdr_qcsapi_get_primary_interface_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_get_primary_interface_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_get_primary_interface call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (ifname && __resp.ifname)
		strcpy(ifname,
			__resp.ifname);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_get_interface_by_index(unsigned int if_index, char * ifname, size_t maxlen)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_get_interface_by_index_request __req;
	struct qcsapi_get_interface_by_index_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (ifname == NULL) {
		return -EFAULT;
	}
	__req.if_index = (unsigned int)if_index;
	if (ifname) {
		ifname[0] = 0;
	}
	__req.maxlen = (unsigned long)maxlen;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_GET_INTERFACE_BY_INDEX_REMOTE,
				(xdrproc_t)xdr_qcsapi_get_interface_by_index_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_get_interface_by_index_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_get_interface_by_index call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (ifname && __resp.ifname)
		strcpy(ifname,
			__resp.ifname);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_wifi_macaddr(const qcsapi_mac_addr new_mac_addr)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_wifi_macaddr_request __req;
	struct qcsapi_wifi_set_wifi_macaddr_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (new_mac_addr == NULL) {
		return -EFAULT;
	}
	memcpy(__req.new_mac_addr, new_mac_addr, sizeof(const qcsapi_mac_addr));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_WIFI_MACADDR_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_wifi_macaddr_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_wifi_macaddr_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_wifi_macaddr call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_interface_get_BSSID(const char * ifname, qcsapi_mac_addr current_BSSID)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_interface_get_BSSID_request __req;
	struct qcsapi_interface_get_BSSID_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (current_BSSID == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_INTERFACE_GET_BSSID_REMOTE,
				(xdrproc_t)xdr_qcsapi_interface_get_BSSID_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_interface_get_BSSID_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_interface_get_BSSID call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (current_BSSID && __resp.current_BSSID)
		memcpy(current_BSSID,
			__resp.current_BSSID,
			sizeof(__resp.current_BSSID));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_rates(const char * ifname, qcsapi_rate_type rate_type, string_1024 supported_rates)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_rates_request __req;
	struct qcsapi_wifi_get_rates_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (supported_rates == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.rate_type = (int)rate_type;
	if (supported_rates) {
		supported_rates[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_RATES_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_rates_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_rates_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_rates call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (supported_rates && __resp.supported_rates)
		strcpy(supported_rates,
			__resp.supported_rates);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_rates(const char * ifname, qcsapi_rate_type rate_type, const string_256 current_rates)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_rates_request __req;
	struct qcsapi_wifi_set_rates_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.rate_type = (int)rate_type;
	__req.current_rates = (str)current_rates;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_RATES_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_rates_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_rates_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_rates call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_get_max_bitrate(const char * ifname, char * max_bitrate, const int max_str_len)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_get_max_bitrate_request __req;
	struct qcsapi_get_max_bitrate_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (max_bitrate == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (max_bitrate) {
		max_bitrate[0] = 0;
	}
	__req.max_str_len = (int)max_str_len;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_GET_MAX_BITRATE_REMOTE,
				(xdrproc_t)xdr_qcsapi_get_max_bitrate_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_get_max_bitrate_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_get_max_bitrate call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (max_bitrate && __resp.max_bitrate)
		strcpy(max_bitrate,
			__resp.max_bitrate);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_set_max_bitrate(const char * ifname, const char * max_bitrate)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_set_max_bitrate_request __req;
	struct qcsapi_set_max_bitrate_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.max_bitrate = (str)max_bitrate;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SET_MAX_BITRATE_REMOTE,
				(xdrproc_t)xdr_qcsapi_set_max_bitrate_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_set_max_bitrate_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_set_max_bitrate call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_qos_get_param(const char * ifname, int the_queue, int the_param, int ap_bss_flag, int * p_value)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_qos_get_param_request __req;
	struct qcsapi_wifi_qos_get_param_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_value == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.the_queue = (int)the_queue;
	__req.the_param = (int)the_param;
	__req.ap_bss_flag = (int)ap_bss_flag;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_QOS_GET_PARAM_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_qos_get_param_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_qos_get_param_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_qos_get_param call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_value)
		*p_value = __resp.p_value;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_qos_set_param(const char * ifname, int the_queue, int the_param, int ap_bss_flag, int value)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_qos_set_param_request __req;
	struct qcsapi_wifi_qos_set_param_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.the_queue = (int)the_queue;
	__req.the_param = (int)the_param;
	__req.ap_bss_flag = (int)ap_bss_flag;
	__req.value = (int)value;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_QOS_SET_PARAM_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_qos_set_param_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_qos_set_param_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_qos_set_param call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_wmm_ac_map(const char * ifname, string_64 mapping_table)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_wmm_ac_map_request __req;
	struct qcsapi_wifi_get_wmm_ac_map_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (mapping_table == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (mapping_table) {
		mapping_table[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_WMM_AC_MAP_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_wmm_ac_map_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_wmm_ac_map_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_wmm_ac_map call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (mapping_table && __resp.mapping_table)
		strcpy(mapping_table,
			__resp.mapping_table);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_wmm_ac_map(const char * ifname, int user_prio, int ac_index)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_wmm_ac_map_request __req;
	struct qcsapi_wifi_set_wmm_ac_map_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.user_prio = (int)user_prio;
	__req.ac_index = (int)ac_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_WMM_AC_MAP_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_wmm_ac_map_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_wmm_ac_map_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_wmm_ac_map call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_dscp_8021p_map(const char * ifname, string_64 mapping_table)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_dscp_8021p_map_request __req;
	struct qcsapi_wifi_get_dscp_8021p_map_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (mapping_table == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (mapping_table) {
		mapping_table[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_DSCP_8021P_MAP_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_dscp_8021p_map_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_dscp_8021p_map_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_dscp_8021p_map call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (mapping_table && __resp.mapping_table)
		strcpy(mapping_table,
			__resp.mapping_table);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_dscp_ac_map(const char * ifname, u8_array_64 mapping_table)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_dscp_ac_map_request __req;
	struct qcsapi_wifi_get_dscp_ac_map_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (mapping_table == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_DSCP_AC_MAP_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_dscp_ac_map_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_dscp_ac_map_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_dscp_ac_map call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (mapping_table && __resp.mapping_table)
		memcpy(mapping_table,
			__resp.mapping_table,
			sizeof(__resp.mapping_table));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_dscp_8021p_map(const char * ifname, const char * ip_dscp_list, uint8_t dot1p_up)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_dscp_8021p_map_request __req;
	struct qcsapi_wifi_set_dscp_8021p_map_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.ip_dscp_list = (str)ip_dscp_list;
	__req.dot1p_up = (unsigned int)dot1p_up;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_DSCP_8021P_MAP_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_dscp_8021p_map_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_dscp_8021p_map_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_dscp_8021p_map call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_dscp_ac_map(const char * ifname, const u8_array_64 dscp_list, uint8_t dscp_list_len, uint8_t ac)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_dscp_ac_map_request __req;
	struct qcsapi_wifi_set_dscp_ac_map_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (dscp_list == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	memcpy(__req.dscp_list, dscp_list, sizeof(const u8_array_64));
	__req.dscp_list_len = (unsigned int)dscp_list_len;
	__req.ac = (unsigned int)ac;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_DSCP_AC_MAP_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_dscp_ac_map_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_dscp_ac_map_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_dscp_ac_map call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_priority(const char * ifname, uint8_t * p_priority)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_priority_request __req;
	struct qcsapi_wifi_get_priority_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_priority == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_PRIORITY_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_priority_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_priority_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_priority call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_priority)
		*p_priority = __resp.p_priority;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_priority(const char * ifname, uint8_t priority)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_priority_request __req;
	struct qcsapi_wifi_set_priority_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.priority = (unsigned int)priority;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_PRIORITY_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_priority_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_priority_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_priority call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_dwell_times(const char * ifname, const unsigned int max_dwell_time_active_chan, const unsigned int min_dwell_time_active_chan, const unsigned int max_dwell_time_passive_chan, const unsigned int min_dwell_time_passive_chan)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_dwell_times_request __req;
	struct qcsapi_wifi_set_dwell_times_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.max_dwell_time_active_chan = (unsigned int)max_dwell_time_active_chan;
	__req.min_dwell_time_active_chan = (unsigned int)min_dwell_time_active_chan;
	__req.max_dwell_time_passive_chan = (unsigned int)max_dwell_time_passive_chan;
	__req.min_dwell_time_passive_chan = (unsigned int)min_dwell_time_passive_chan;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_DWELL_TIMES_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_dwell_times_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_dwell_times_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_dwell_times call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_dwell_times(const char * ifname, unsigned int * p_max_dwell_time_active_chan, unsigned int * p_min_dwell_time_active_chan, unsigned int * p_max_dwell_time_passive_chan, unsigned int * p_min_dwell_time_passive_chan)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_dwell_times_request __req;
	struct qcsapi_wifi_get_dwell_times_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_max_dwell_time_active_chan == NULL || p_min_dwell_time_active_chan == NULL || p_max_dwell_time_passive_chan == NULL || p_min_dwell_time_passive_chan == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_DWELL_TIMES_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_dwell_times_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_dwell_times_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_dwell_times call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_max_dwell_time_active_chan)
		*p_max_dwell_time_active_chan = __resp.p_max_dwell_time_active_chan;
	if (p_min_dwell_time_active_chan)
		*p_min_dwell_time_active_chan = __resp.p_min_dwell_time_active_chan;
	if (p_max_dwell_time_passive_chan)
		*p_max_dwell_time_passive_chan = __resp.p_max_dwell_time_passive_chan;
	if (p_min_dwell_time_passive_chan)
		*p_min_dwell_time_passive_chan = __resp.p_min_dwell_time_passive_chan;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_bgscan_dwell_times(const char * ifname, const unsigned int dwell_time_active_chan, const unsigned int dwell_time_passive_chan)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_bgscan_dwell_times_request __req;
	struct qcsapi_wifi_set_bgscan_dwell_times_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.dwell_time_active_chan = (unsigned int)dwell_time_active_chan;
	__req.dwell_time_passive_chan = (unsigned int)dwell_time_passive_chan;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_BGSCAN_DWELL_TIMES_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_bgscan_dwell_times_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_bgscan_dwell_times_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_bgscan_dwell_times call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_bgscan_dwell_times(const char * ifname, unsigned int * p_dwell_time_active_chan, unsigned int * p_dwell_time_passive_chan)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_bgscan_dwell_times_request __req;
	struct qcsapi_wifi_get_bgscan_dwell_times_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_dwell_time_active_chan == NULL || p_dwell_time_passive_chan == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_BGSCAN_DWELL_TIMES_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_bgscan_dwell_times_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_bgscan_dwell_times_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_bgscan_dwell_times call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_dwell_time_active_chan)
		*p_dwell_time_active_chan = __resp.p_dwell_time_active_chan;
	if (p_dwell_time_passive_chan)
		*p_dwell_time_passive_chan = __resp.p_dwell_time_passive_chan;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_start_scan(const char * ifname)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_start_scan_request __req;
	struct qcsapi_wifi_start_scan_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_START_SCAN_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_start_scan_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_start_scan_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_start_scan call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_cancel_scan(const char * ifname, int force)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_cancel_scan_request __req;
	struct qcsapi_wifi_cancel_scan_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.force = (int)force;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_CANCEL_SCAN_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_cancel_scan_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_cancel_scan_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_cancel_scan call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_scan_status(const char * ifname, int * scanstatus)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_scan_status_request __req;
	struct qcsapi_wifi_get_scan_status_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (scanstatus == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_SCAN_STATUS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_scan_status_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_scan_status_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_scan_status call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (scanstatus)
		*scanstatus = __resp.scanstatus;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_cac_status(const char * ifname, int * cacstatus)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_cac_status_request __req;
	struct qcsapi_wifi_get_cac_status_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (cacstatus == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_CAC_STATUS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_cac_status_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_cac_status_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_cac_status call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (cacstatus)
		*cacstatus = __resp.cacstatus;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_wait_scan_completes(const char * ifname, time_t timeout)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_wait_scan_completes_request __req;
	struct qcsapi_wifi_wait_scan_completes_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.timeout = (unsigned long)timeout;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_WAIT_SCAN_COMPLETES_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_wait_scan_completes_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_wait_scan_completes_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_wait_scan_completes call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_tx_power(const char * ifname, const qcsapi_unsigned_int the_channel, int * p_tx_power)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_tx_power_request __req;
	struct qcsapi_wifi_get_tx_power_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_tx_power == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.the_channel = (unsigned int)the_channel;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_TX_POWER_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_tx_power_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_tx_power_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_tx_power call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_tx_power)
		*p_tx_power = __resp.p_tx_power;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_tx_power(const char * ifname, const qcsapi_unsigned_int the_channel, const int tx_power)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_tx_power_request __req;
	struct qcsapi_wifi_set_tx_power_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.the_channel = (unsigned int)the_channel;
	__req.tx_power = (int)tx_power;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_TX_POWER_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_tx_power_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_tx_power_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_tx_power call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_bw_power(const char * ifname, const qcsapi_unsigned_int the_channel, int * p_power_20M, int * p_power_40M, int * p_power_80M)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_bw_power_request __req;
	struct qcsapi_wifi_get_bw_power_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_power_20M == NULL || p_power_40M == NULL || p_power_80M == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.the_channel = (unsigned int)the_channel;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_BW_POWER_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_bw_power_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_bw_power_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_bw_power call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_power_20M)
		*p_power_20M = __resp.p_power_20M;
	if (p_power_40M)
		*p_power_40M = __resp.p_power_40M;
	if (p_power_80M)
		*p_power_80M = __resp.p_power_80M;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_bw_power(const char * ifname, const qcsapi_unsigned_int the_channel, const int power_20M, const int power_40M, const int power_80M)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_bw_power_request __req;
	struct qcsapi_wifi_set_bw_power_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.the_channel = (unsigned int)the_channel;
	__req.power_20M = (int)power_20M;
	__req.power_40M = (int)power_40M;
	__req.power_80M = (int)power_80M;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_BW_POWER_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_bw_power_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_bw_power_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_bw_power call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_regulatory_set_bw_power(const char * ifname, const qcsapi_unsigned_int the_channel, const int power_20M, const int power_40M, const int power_80M)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_regulatory_set_bw_power_request __req;
	struct qcsapi_regulatory_set_bw_power_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.the_channel = (unsigned int)the_channel;
	__req.power_20M = (int)power_20M;
	__req.power_40M = (int)power_40M;
	__req.power_80M = (int)power_80M;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_REGULATORY_SET_BW_POWER_REMOTE,
				(xdrproc_t)xdr_qcsapi_regulatory_set_bw_power_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_regulatory_set_bw_power_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_regulatory_set_bw_power call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_carrier_interference(const char * ifname, int * ci)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_carrier_interference_request __req;
	struct qcsapi_wifi_get_carrier_interference_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (ci == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_CARRIER_INTERFERENCE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_carrier_interference_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_carrier_interference_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_carrier_interference call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (ci)
		*ci = __resp.ci;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_congestion_index(const char * ifname, int * ci)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_congestion_index_request __req;
	struct qcsapi_wifi_get_congestion_index_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (ci == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_CONGESTION_INDEX_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_congestion_index_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_congestion_index_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_congestion_index call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (ci)
		*ci = __resp.ci;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_supported_tx_power_levels(const char * ifname, string_128 available_percentages)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_supported_tx_power_levels_request __req;
	struct qcsapi_wifi_get_supported_tx_power_levels_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (available_percentages == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (available_percentages) {
		available_percentages[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_SUPPORTED_TX_POWER_LEVELS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_supported_tx_power_levels_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_supported_tx_power_levels_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_supported_tx_power_levels call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (available_percentages && __resp.available_percentages)
		strcpy(available_percentages,
			__resp.available_percentages);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_current_tx_power_level(const char * ifname, uint32_t * p_current_percentage)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_current_tx_power_level_request __req;
	struct qcsapi_wifi_get_current_tx_power_level_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_current_percentage == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_CURRENT_TX_POWER_LEVEL_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_current_tx_power_level_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_current_tx_power_level_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_current_tx_power_level call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_current_percentage)
		*p_current_percentage = __resp.p_current_percentage;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_power_constraint(const char * ifname, uint32_t pwr_constraint)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_power_constraint_request __req;
	struct qcsapi_wifi_set_power_constraint_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.pwr_constraint = (unsigned long)pwr_constraint;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_POWER_CONSTRAINT_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_power_constraint_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_power_constraint_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_power_constraint call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_power_constraint(const char * ifname, uint32_t * p_pwr_constraint)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_power_constraint_request __req;
	struct qcsapi_wifi_get_power_constraint_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_pwr_constraint == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_POWER_CONSTRAINT_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_power_constraint_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_power_constraint_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_power_constraint call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_pwr_constraint)
		*p_pwr_constraint = __resp.p_pwr_constraint;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_tpc_interval(const char * ifname, int32_t tpc_interval)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_tpc_interval_request __req;
	struct qcsapi_wifi_set_tpc_interval_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.tpc_interval = (long)tpc_interval;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_TPC_INTERVAL_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_tpc_interval_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_tpc_interval_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_tpc_interval call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_tpc_interval(const char * ifname, uint32_t * p_tpc_interval)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_tpc_interval_request __req;
	struct qcsapi_wifi_get_tpc_interval_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_tpc_interval == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_TPC_INTERVAL_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_tpc_interval_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_tpc_interval_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_tpc_interval call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_tpc_interval)
		*p_tpc_interval = __resp.p_tpc_interval;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_assoc_records(const char * ifname, int reset, qcsapi_assoc_records * records)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_assoc_records_request __req;
	struct qcsapi_wifi_get_assoc_records_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (records == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.reset = (int)reset;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_ASSOC_RECORDS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_assoc_records_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_assoc_records_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_assoc_records call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(records->addr); __i_0++) {
			if (records->addr[__i_0] && __resp.records_addr[__i_0])
		memcpy(records->addr[__i_0],
			__resp.records_addr[__i_0],
			sizeof(__resp.records_addr[__i_0]));
		}
	}
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(records->timestamp); __i_0++) {
			records->timestamp[__i_0] = __resp.records_timestamp[__i_0];
		}
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_ap_isolate(const char * ifname, int * p_ap_isolate)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_ap_isolate_request __req;
	struct qcsapi_wifi_get_ap_isolate_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_ap_isolate == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_AP_ISOLATE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_ap_isolate_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_ap_isolate_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_ap_isolate call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_ap_isolate)
		*p_ap_isolate = __resp.p_ap_isolate;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_ap_isolate(const char * ifname, const int new_ap_isolate)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_ap_isolate_request __req;
	struct qcsapi_wifi_set_ap_isolate_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.new_ap_isolate = (int)new_ap_isolate;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_AP_ISOLATE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_ap_isolate_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_ap_isolate_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_ap_isolate call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_create_restricted_bss(const char * ifname, const qcsapi_mac_addr mac_addr)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_create_restricted_bss_request __req;
	struct qcsapi_wifi_create_restricted_bss_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (mac_addr == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	memcpy(__req.mac_addr, mac_addr, sizeof(const qcsapi_mac_addr));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_CREATE_RESTRICTED_BSS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_create_restricted_bss_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_create_restricted_bss_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_create_restricted_bss call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_create_bss(const char * ifname, const qcsapi_mac_addr mac_addr)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_create_bss_request __req;
	struct qcsapi_wifi_create_bss_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (mac_addr == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	memcpy(__req.mac_addr, mac_addr, sizeof(const qcsapi_mac_addr));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_CREATE_BSS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_create_bss_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_create_bss_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_create_bss call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_remove_bss(const char * ifname)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_remove_bss_request __req;
	struct qcsapi_wifi_remove_bss_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_REMOVE_BSS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_remove_bss_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_remove_bss_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_remove_bss call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wds_add_peer(const char * ifname, const qcsapi_mac_addr peer_address)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wds_add_peer_request __req;
	struct qcsapi_wds_add_peer_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (peer_address == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	memcpy(__req.peer_address, peer_address, sizeof(const qcsapi_mac_addr));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WDS_ADD_PEER_REMOTE,
				(xdrproc_t)xdr_qcsapi_wds_add_peer_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wds_add_peer_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wds_add_peer call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wds_remove_peer(const char * ifname, const qcsapi_mac_addr peer_address)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wds_remove_peer_request __req;
	struct qcsapi_wds_remove_peer_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (peer_address == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	memcpy(__req.peer_address, peer_address, sizeof(const qcsapi_mac_addr));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WDS_REMOVE_PEER_REMOTE,
				(xdrproc_t)xdr_qcsapi_wds_remove_peer_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wds_remove_peer_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wds_remove_peer call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wds_get_peer_address(const char * ifname, const int index, qcsapi_mac_addr peer_address)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wds_get_peer_address_request __req;
	struct qcsapi_wds_get_peer_address_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (peer_address == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.index = (int)index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WDS_GET_PEER_ADDRESS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wds_get_peer_address_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wds_get_peer_address_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wds_get_peer_address call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (peer_address && __resp.peer_address)
		memcpy(peer_address,
			__resp.peer_address,
			sizeof(__resp.peer_address));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wds_set_psk(const char * ifname, const qcsapi_mac_addr peer_address, const string_64 pre_shared_key)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wds_set_psk_request __req;
	struct qcsapi_wds_set_psk_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (peer_address == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	memcpy(__req.peer_address, peer_address, sizeof(const qcsapi_mac_addr));
	__req.pre_shared_key = (str)pre_shared_key;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WDS_SET_PSK_REMOTE,
				(xdrproc_t)xdr_qcsapi_wds_set_psk_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wds_set_psk_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wds_set_psk call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_beacon_type(const char * ifname, char * p_current_beacon)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_beacon_type_request __req;
	struct qcsapi_wifi_get_beacon_type_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_current_beacon == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (p_current_beacon) {
		p_current_beacon[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_BEACON_TYPE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_beacon_type_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_beacon_type_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_beacon_type call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_current_beacon && __resp.p_current_beacon)
		strcpy(p_current_beacon,
			__resp.p_current_beacon);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_beacon_type(const char * ifname, const char * p_new_beacon)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_beacon_type_request __req;
	struct qcsapi_wifi_set_beacon_type_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.p_new_beacon = (str)p_new_beacon;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_BEACON_TYPE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_beacon_type_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_beacon_type_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_beacon_type call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_WEP_key_index(const char * ifname, qcsapi_unsigned_int * p_key_index)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_WEP_key_index_request __req;
	struct qcsapi_wifi_get_WEP_key_index_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_key_index == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_WEP_KEY_INDEX_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_WEP_key_index_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_WEP_key_index_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_WEP_key_index call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_key_index)
		*p_key_index = __resp.p_key_index;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_WEP_key_index(const char * ifname, const qcsapi_unsigned_int key_index)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_WEP_key_index_request __req;
	struct qcsapi_wifi_set_WEP_key_index_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.key_index = (unsigned int)key_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_WEP_KEY_INDEX_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_WEP_key_index_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_WEP_key_index_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_WEP_key_index call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_WEP_key_passphrase(const char * ifname, string_64 current_passphrase)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_WEP_key_passphrase_request __req;
	struct qcsapi_wifi_get_WEP_key_passphrase_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (current_passphrase == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (current_passphrase) {
		current_passphrase[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_WEP_KEY_PASSPHRASE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_WEP_key_passphrase_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_WEP_key_passphrase_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_WEP_key_passphrase call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (current_passphrase && __resp.current_passphrase)
		strcpy(current_passphrase,
			__resp.current_passphrase);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_WEP_key_passphrase(const char * ifname, const string_64 new_passphrase)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_WEP_key_passphrase_request __req;
	struct qcsapi_wifi_set_WEP_key_passphrase_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.new_passphrase = (str)new_passphrase;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_WEP_KEY_PASSPHRASE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_WEP_key_passphrase_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_WEP_key_passphrase_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_WEP_key_passphrase call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_WEP_encryption_level(const char * ifname, string_64 current_encryption_level)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_WEP_encryption_level_request __req;
	struct qcsapi_wifi_get_WEP_encryption_level_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (current_encryption_level == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (current_encryption_level) {
		current_encryption_level[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_WEP_ENCRYPTION_LEVEL_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_WEP_encryption_level_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_WEP_encryption_level_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_WEP_encryption_level call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (current_encryption_level && __resp.current_encryption_level)
		strcpy(current_encryption_level,
			__resp.current_encryption_level);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_basic_encryption_modes(const char * ifname, string_32 encryption_modes)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_basic_encryption_modes_request __req;
	struct qcsapi_wifi_get_basic_encryption_modes_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (encryption_modes == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (encryption_modes) {
		encryption_modes[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_BASIC_ENCRYPTION_MODES_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_basic_encryption_modes_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_basic_encryption_modes_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_basic_encryption_modes call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (encryption_modes && __resp.encryption_modes)
		strcpy(encryption_modes,
			__resp.encryption_modes);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_basic_encryption_modes(const char * ifname, const string_32 encryption_modes)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_basic_encryption_modes_request __req;
	struct qcsapi_wifi_set_basic_encryption_modes_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.encryption_modes = (str)encryption_modes;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_BASIC_ENCRYPTION_MODES_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_basic_encryption_modes_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_basic_encryption_modes_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_basic_encryption_modes call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_basic_authentication_mode(const char * ifname, string_32 authentication_mode)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_basic_authentication_mode_request __req;
	struct qcsapi_wifi_get_basic_authentication_mode_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (authentication_mode == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (authentication_mode) {
		authentication_mode[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_BASIC_AUTHENTICATION_MODE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_basic_authentication_mode_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_basic_authentication_mode_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_basic_authentication_mode call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (authentication_mode && __resp.authentication_mode)
		strcpy(authentication_mode,
			__resp.authentication_mode);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_basic_authentication_mode(const char * ifname, const string_32 authentication_mode)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_basic_authentication_mode_request __req;
	struct qcsapi_wifi_set_basic_authentication_mode_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.authentication_mode = (str)authentication_mode;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_BASIC_AUTHENTICATION_MODE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_basic_authentication_mode_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_basic_authentication_mode_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_basic_authentication_mode call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_WEP_key(const char * ifname, qcsapi_unsigned_int key_index, string_64 current_passphrase)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_WEP_key_request __req;
	struct qcsapi_wifi_get_WEP_key_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (current_passphrase == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.key_index = (unsigned int)key_index;
	if (current_passphrase) {
		current_passphrase[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_WEP_KEY_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_WEP_key_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_WEP_key_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_WEP_key call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (current_passphrase && __resp.current_passphrase)
		strcpy(current_passphrase,
			__resp.current_passphrase);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_WEP_key(const char * ifname, qcsapi_unsigned_int key_index, const string_64 new_passphrase)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_WEP_key_request __req;
	struct qcsapi_wifi_set_WEP_key_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.key_index = (unsigned int)key_index;
	__req.new_passphrase = (str)new_passphrase;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_WEP_KEY_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_WEP_key_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_WEP_key_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_WEP_key call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_WPA_encryption_modes(const char * ifname, string_32 encryption_modes)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_WPA_encryption_modes_request __req;
	struct qcsapi_wifi_get_WPA_encryption_modes_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (encryption_modes == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (encryption_modes) {
		encryption_modes[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_WPA_ENCRYPTION_MODES_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_WPA_encryption_modes_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_WPA_encryption_modes_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_WPA_encryption_modes call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (encryption_modes && __resp.encryption_modes)
		strcpy(encryption_modes,
			__resp.encryption_modes);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_WPA_encryption_modes(const char * ifname, const string_32 encryption_modes)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_WPA_encryption_modes_request __req;
	struct qcsapi_wifi_set_WPA_encryption_modes_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.encryption_modes = (str)encryption_modes;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_WPA_ENCRYPTION_MODES_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_WPA_encryption_modes_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_WPA_encryption_modes_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_WPA_encryption_modes call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_WPA_authentication_mode(const char * ifname, string_32 authentication_mode)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_WPA_authentication_mode_request __req;
	struct qcsapi_wifi_get_WPA_authentication_mode_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (authentication_mode == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (authentication_mode) {
		authentication_mode[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_WPA_AUTHENTICATION_MODE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_WPA_authentication_mode_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_WPA_authentication_mode_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_WPA_authentication_mode call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (authentication_mode && __resp.authentication_mode)
		strcpy(authentication_mode,
			__resp.authentication_mode);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_WPA_authentication_mode(const char * ifname, const string_32 authentication_mode)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_WPA_authentication_mode_request __req;
	struct qcsapi_wifi_set_WPA_authentication_mode_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.authentication_mode = (str)authentication_mode;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_WPA_AUTHENTICATION_MODE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_WPA_authentication_mode_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_WPA_authentication_mode_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_WPA_authentication_mode call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_IEEE11i_encryption_modes(const char * ifname, string_32 encryption_modes)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_IEEE11i_encryption_modes_request __req;
	struct qcsapi_wifi_get_IEEE11i_encryption_modes_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (encryption_modes == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (encryption_modes) {
		encryption_modes[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_IEEE11I_ENCRYPTION_MODES_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_IEEE11i_encryption_modes_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_IEEE11i_encryption_modes_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_IEEE11i_encryption_modes call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (encryption_modes && __resp.encryption_modes)
		strcpy(encryption_modes,
			__resp.encryption_modes);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_IEEE11i_encryption_modes(const char * ifname, const string_32 encryption_modes)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_IEEE11i_encryption_modes_request __req;
	struct qcsapi_wifi_set_IEEE11i_encryption_modes_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.encryption_modes = (str)encryption_modes;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_IEEE11I_ENCRYPTION_MODES_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_IEEE11i_encryption_modes_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_IEEE11i_encryption_modes_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_IEEE11i_encryption_modes call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_IEEE11i_authentication_mode(const char * ifname, string_32 authentication_mode)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_IEEE11i_authentication_mode_request __req;
	struct qcsapi_wifi_get_IEEE11i_authentication_mode_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (authentication_mode == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (authentication_mode) {
		authentication_mode[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_IEEE11I_AUTHENTICATION_MODE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_IEEE11i_authentication_mode_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_IEEE11i_authentication_mode_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_IEEE11i_authentication_mode call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (authentication_mode && __resp.authentication_mode)
		strcpy(authentication_mode,
			__resp.authentication_mode);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_IEEE11i_authentication_mode(const char * ifname, const string_32 authentication_mode)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_IEEE11i_authentication_mode_request __req;
	struct qcsapi_wifi_set_IEEE11i_authentication_mode_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.authentication_mode = (str)authentication_mode;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_IEEE11I_AUTHENTICATION_MODE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_IEEE11i_authentication_mode_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_IEEE11i_authentication_mode_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_IEEE11i_authentication_mode call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_michael_errcnt(const char * ifname, uint32_t * errcount)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_michael_errcnt_request __req;
	struct qcsapi_wifi_get_michael_errcnt_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (errcount == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_MICHAEL_ERRCNT_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_michael_errcnt_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_michael_errcnt_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_michael_errcnt call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (errcount)
		*errcount = __resp.errcount;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_pre_shared_key(const char * ifname, const qcsapi_unsigned_int key_index, string_64 pre_shared_key)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_pre_shared_key_request __req;
	struct qcsapi_wifi_get_pre_shared_key_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (pre_shared_key == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.key_index = (unsigned int)key_index;
	if (pre_shared_key) {
		pre_shared_key[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_PRE_SHARED_KEY_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_pre_shared_key_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_pre_shared_key_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_pre_shared_key call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (pre_shared_key && __resp.pre_shared_key)
		strcpy(pre_shared_key,
			__resp.pre_shared_key);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_pre_shared_key(const char * ifname, const qcsapi_unsigned_int key_index, const string_64 pre_shared_key)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_pre_shared_key_request __req;
	struct qcsapi_wifi_set_pre_shared_key_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.key_index = (unsigned int)key_index;
	__req.pre_shared_key = (str)pre_shared_key;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_PRE_SHARED_KEY_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_pre_shared_key_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_pre_shared_key_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_pre_shared_key call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_key_passphrase(const char * ifname, const qcsapi_unsigned_int key_index, string_64 passphrase)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_key_passphrase_request __req;
	struct qcsapi_wifi_get_key_passphrase_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (passphrase == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.key_index = (unsigned int)key_index;
	if (passphrase) {
		passphrase[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_KEY_PASSPHRASE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_key_passphrase_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_key_passphrase_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_key_passphrase call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (passphrase && __resp.passphrase)
		strcpy(passphrase,
			__resp.passphrase);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_key_passphrase(const char * ifname, const qcsapi_unsigned_int key_index, const string_64 passphrase)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_key_passphrase_request __req;
	struct qcsapi_wifi_set_key_passphrase_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.key_index = (unsigned int)key_index;
	__req.passphrase = (str)passphrase;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_KEY_PASSPHRASE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_key_passphrase_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_key_passphrase_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_key_passphrase call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_wpa_status(const char * ifname, char * wpa_status, char * mac_addr, const qcsapi_unsigned_int max_len)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_wpa_status_request __req;
	struct qcsapi_wifi_get_wpa_status_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (wpa_status == NULL || mac_addr == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (wpa_status) {
		wpa_status[0] = 0;
	}
	if (mac_addr) {
		mac_addr[0] = 0;
	}
	__req.max_len = (unsigned int)max_len;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_WPA_STATUS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_wpa_status_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_wpa_status_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_wpa_status call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (wpa_status && __resp.wpa_status)
		strcpy(wpa_status,
			__resp.wpa_status);
	if (mac_addr && __resp.mac_addr)
		strcpy(mac_addr,
			__resp.mac_addr);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_psk_auth_failures(const char * ifname, qcsapi_unsigned_int * count)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_psk_auth_failures_request __req;
	struct qcsapi_wifi_get_psk_auth_failures_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (count == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_PSK_AUTH_FAILURES_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_psk_auth_failures_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_psk_auth_failures_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_psk_auth_failures call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (count)
		*count = __resp.count;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_auth_state(const char * ifname, char * mac_addr, int * auth_state)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_auth_state_request __req;
	struct qcsapi_wifi_get_auth_state_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (mac_addr == NULL || auth_state == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (mac_addr) {
		mac_addr[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_AUTH_STATE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_auth_state_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_auth_state_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_auth_state call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (mac_addr && __resp.mac_addr)
		strcpy(mac_addr,
			__resp.mac_addr);
	if (auth_state)
		*auth_state = __resp.auth_state;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_mac_address_filtering(const char * ifname, const qcsapi_mac_address_filtering new_mac_address_filtering)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_mac_address_filtering_request __req;
	struct qcsapi_wifi_set_mac_address_filtering_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.new_mac_address_filtering = (int)new_mac_address_filtering;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_MAC_ADDRESS_FILTERING_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_mac_address_filtering_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_mac_address_filtering_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_mac_address_filtering call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_mac_address_filtering(const char * ifname, qcsapi_mac_address_filtering * current_mac_address_filtering)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_mac_address_filtering_request __req;
	struct qcsapi_wifi_get_mac_address_filtering_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (current_mac_address_filtering == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_MAC_ADDRESS_FILTERING_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_mac_address_filtering_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_mac_address_filtering_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_mac_address_filtering call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (current_mac_address_filtering)
		*current_mac_address_filtering = __resp.current_mac_address_filtering;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_authorize_mac_address(const char * ifname, const qcsapi_mac_addr address_to_authorize)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_authorize_mac_address_request __req;
	struct qcsapi_wifi_authorize_mac_address_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (address_to_authorize == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	memcpy(__req.address_to_authorize, address_to_authorize, sizeof(const qcsapi_mac_addr));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_AUTHORIZE_MAC_ADDRESS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_authorize_mac_address_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_authorize_mac_address_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_authorize_mac_address call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_deny_mac_address(const char * ifname, const qcsapi_mac_addr address_to_deny)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_deny_mac_address_request __req;
	struct qcsapi_wifi_deny_mac_address_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (address_to_deny == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	memcpy(__req.address_to_deny, address_to_deny, sizeof(const qcsapi_mac_addr));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_DENY_MAC_ADDRESS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_deny_mac_address_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_deny_mac_address_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_deny_mac_address call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_remove_mac_address(const char * ifname, const qcsapi_mac_addr address_to_remove)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_remove_mac_address_request __req;
	struct qcsapi_wifi_remove_mac_address_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (address_to_remove == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	memcpy(__req.address_to_remove, address_to_remove, sizeof(const qcsapi_mac_addr));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_REMOVE_MAC_ADDRESS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_remove_mac_address_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_remove_mac_address_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_remove_mac_address call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_is_mac_address_authorized(const char * ifname, const qcsapi_mac_addr address_to_verify, int * p_mac_address_authorized)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_is_mac_address_authorized_request __req;
	struct qcsapi_wifi_is_mac_address_authorized_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (address_to_verify == NULL || p_mac_address_authorized == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	memcpy(__req.address_to_verify, address_to_verify, sizeof(const qcsapi_mac_addr));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_IS_MAC_ADDRESS_AUTHORIZED_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_is_mac_address_authorized_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_is_mac_address_authorized_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_is_mac_address_authorized call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_mac_address_authorized)
		*p_mac_address_authorized = __resp.p_mac_address_authorized;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_authorized_mac_addresses(const char * ifname, char * list_mac_addresses, const unsigned int sizeof_list)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_authorized_mac_addresses_request __req;
	struct qcsapi_wifi_get_authorized_mac_addresses_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (list_mac_addresses == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (list_mac_addresses) {
		list_mac_addresses[0] = 0;
	}
	__req.sizeof_list = (unsigned int)sizeof_list;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_AUTHORIZED_MAC_ADDRESSES_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_authorized_mac_addresses_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_authorized_mac_addresses_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_authorized_mac_addresses call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (list_mac_addresses && __resp.list_mac_addresses)
		strcpy(list_mac_addresses,
			__resp.list_mac_addresses);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_denied_mac_addresses(const char * ifname, char * list_mac_addresses, const unsigned int sizeof_list)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_denied_mac_addresses_request __req;
	struct qcsapi_wifi_get_denied_mac_addresses_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (list_mac_addresses == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (list_mac_addresses) {
		list_mac_addresses[0] = 0;
	}
	__req.sizeof_list = (unsigned int)sizeof_list;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_DENIED_MAC_ADDRESSES_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_denied_mac_addresses_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_denied_mac_addresses_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_denied_mac_addresses call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (list_mac_addresses && __resp.list_mac_addresses)
		strcpy(list_mac_addresses,
			__resp.list_mac_addresses);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_accept_oui_filter(const char * ifname, const qcsapi_mac_addr oui, int flag)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_accept_oui_filter_request __req;
	struct qcsapi_wifi_set_accept_oui_filter_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (oui == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	memcpy(__req.oui, oui, sizeof(const qcsapi_mac_addr));
	__req.flag = (int)flag;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_ACCEPT_OUI_FILTER_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_accept_oui_filter_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_accept_oui_filter_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_accept_oui_filter call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_accept_oui_filter(const char * ifname, char * oui_list, const unsigned int sizeof_list)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_accept_oui_filter_request __req;
	struct qcsapi_wifi_get_accept_oui_filter_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (oui_list == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (oui_list) {
		oui_list[0] = 0;
	}
	__req.sizeof_list = (unsigned int)sizeof_list;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_ACCEPT_OUI_FILTER_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_accept_oui_filter_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_accept_oui_filter_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_accept_oui_filter call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (oui_list && __resp.oui_list)
		strcpy(oui_list,
			__resp.oui_list);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_clear_mac_address_filters(const char * ifname)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_clear_mac_address_filters_request __req;
	struct qcsapi_wifi_clear_mac_address_filters_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_CLEAR_MAC_ADDRESS_FILTERS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_clear_mac_address_filters_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_clear_mac_address_filters_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_clear_mac_address_filters call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_option(const char * ifname, qcsapi_option_type qcsapi_option, int * p_current_option)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_option_request __req;
	struct qcsapi_wifi_get_option_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_current_option == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.qcsapi_option = (int)qcsapi_option;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_OPTION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_option_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_option_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_option call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_current_option)
		*p_current_option = __resp.p_current_option;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_option(const char * ifname, qcsapi_option_type qcsapi_option, int new_option)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_option_request __req;
	struct qcsapi_wifi_set_option_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.qcsapi_option = (int)qcsapi_option;
	__req.new_option = (int)new_option;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_OPTION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_option_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_option_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_option call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_SSID_create_SSID(const char * ifname, const qcsapi_SSID new_SSID)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_SSID_create_SSID_request __req;
	struct qcsapi_SSID_create_SSID_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.new_SSID = (str)new_SSID;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SSID_CREATE_SSID_REMOTE,
				(xdrproc_t)xdr_qcsapi_SSID_create_SSID_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_SSID_create_SSID_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_SSID_create_SSID call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_SSID_remove_SSID(const char * ifname, const qcsapi_SSID del_SSID)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_SSID_remove_SSID_request __req;
	struct qcsapi_SSID_remove_SSID_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.del_SSID = (str)del_SSID;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SSID_REMOVE_SSID_REMOTE,
				(xdrproc_t)xdr_qcsapi_SSID_remove_SSID_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_SSID_remove_SSID_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_SSID_remove_SSID call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_SSID_verify_SSID(const char * ifname, const qcsapi_SSID current_SSID)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_SSID_verify_SSID_request __req;
	struct qcsapi_SSID_verify_SSID_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.current_SSID = (str)current_SSID;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SSID_VERIFY_SSID_REMOTE,
				(xdrproc_t)xdr_qcsapi_SSID_verify_SSID_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_SSID_verify_SSID_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_SSID_verify_SSID call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_SSID_rename_SSID(const char * ifname, const qcsapi_SSID current_SSID, const qcsapi_SSID new_SSID)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_SSID_rename_SSID_request __req;
	struct qcsapi_SSID_rename_SSID_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.current_SSID = (str)current_SSID;
	__req.new_SSID = (str)new_SSID;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SSID_RENAME_SSID_REMOTE,
				(xdrproc_t)xdr_qcsapi_SSID_rename_SSID_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_SSID_rename_SSID_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_SSID_rename_SSID call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_SSID_get_SSID_list(const char * ifname, const unsigned int arrayc, char ** list_SSID)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_SSID_get_SSID_list_request __req;
	struct qcsapi_SSID_get_SSID_list_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (list_SSID == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.arrayc = (unsigned int)arrayc;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SSID_GET_SSID_LIST_REMOTE,
				(xdrproc_t)xdr_qcsapi_SSID_get_SSID_list_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_SSID_get_SSID_list_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_SSID_get_SSID_list call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	{
		unsigned int i;
		for (i = 0; i < __resp.list_SSID.list_SSID_len; i++) {
			strcpy(list_SSID[i], __resp.list_SSID.list_SSID_val[i]);
		}
		list_SSID[i] = NULL;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_SSID_set_protocol(const char * ifname, const qcsapi_SSID current_SSID, const char * new_protocol)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_SSID_set_protocol_request __req;
	struct qcsapi_SSID_set_protocol_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.current_SSID = (str)current_SSID;
	__req.new_protocol = (str)new_protocol;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SSID_SET_PROTOCOL_REMOTE,
				(xdrproc_t)xdr_qcsapi_SSID_set_protocol_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_SSID_set_protocol_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_SSID_set_protocol call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_SSID_get_protocol(const char * ifname, const qcsapi_SSID current_SSID, string_16 current_protocol)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_SSID_get_protocol_request __req;
	struct qcsapi_SSID_get_protocol_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (current_protocol == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.current_SSID = (str)current_SSID;
	if (current_protocol) {
		current_protocol[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SSID_GET_PROTOCOL_REMOTE,
				(xdrproc_t)xdr_qcsapi_SSID_get_protocol_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_SSID_get_protocol_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_SSID_get_protocol call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (current_protocol && __resp.current_protocol)
		strcpy(current_protocol,
			__resp.current_protocol);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_SSID_get_encryption_modes(const char * ifname, const qcsapi_SSID current_SSID, string_32 encryption_modes)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_SSID_get_encryption_modes_request __req;
	struct qcsapi_SSID_get_encryption_modes_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (encryption_modes == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.current_SSID = (str)current_SSID;
	if (encryption_modes) {
		encryption_modes[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SSID_GET_ENCRYPTION_MODES_REMOTE,
				(xdrproc_t)xdr_qcsapi_SSID_get_encryption_modes_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_SSID_get_encryption_modes_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_SSID_get_encryption_modes call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (encryption_modes && __resp.encryption_modes)
		strcpy(encryption_modes,
			__resp.encryption_modes);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_SSID_set_encryption_modes(const char * ifname, const qcsapi_SSID current_SSID, const string_32 encryption_modes)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_SSID_set_encryption_modes_request __req;
	struct qcsapi_SSID_set_encryption_modes_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.current_SSID = (str)current_SSID;
	__req.encryption_modes = (str)encryption_modes;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SSID_SET_ENCRYPTION_MODES_REMOTE,
				(xdrproc_t)xdr_qcsapi_SSID_set_encryption_modes_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_SSID_set_encryption_modes_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_SSID_set_encryption_modes call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_SSID_get_group_encryption(const char * ifname, const qcsapi_SSID current_SSID, string_32 encryption_mode)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_SSID_get_group_encryption_request __req;
	struct qcsapi_SSID_get_group_encryption_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (encryption_mode == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.current_SSID = (str)current_SSID;
	if (encryption_mode) {
		encryption_mode[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SSID_GET_GROUP_ENCRYPTION_REMOTE,
				(xdrproc_t)xdr_qcsapi_SSID_get_group_encryption_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_SSID_get_group_encryption_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_SSID_get_group_encryption call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (encryption_mode && __resp.encryption_mode)
		strcpy(encryption_mode,
			__resp.encryption_mode);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_SSID_set_group_encryption(const char * ifname, const qcsapi_SSID current_SSID, const string_32 encryption_mode)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_SSID_set_group_encryption_request __req;
	struct qcsapi_SSID_set_group_encryption_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.current_SSID = (str)current_SSID;
	__req.encryption_mode = (str)encryption_mode;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SSID_SET_GROUP_ENCRYPTION_REMOTE,
				(xdrproc_t)xdr_qcsapi_SSID_set_group_encryption_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_SSID_set_group_encryption_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_SSID_set_group_encryption call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_SSID_get_authentication_mode(const char * ifname, const qcsapi_SSID current_SSID, string_32 authentication_mode)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_SSID_get_authentication_mode_request __req;
	struct qcsapi_SSID_get_authentication_mode_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (authentication_mode == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.current_SSID = (str)current_SSID;
	if (authentication_mode) {
		authentication_mode[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SSID_GET_AUTHENTICATION_MODE_REMOTE,
				(xdrproc_t)xdr_qcsapi_SSID_get_authentication_mode_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_SSID_get_authentication_mode_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_SSID_get_authentication_mode call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (authentication_mode && __resp.authentication_mode)
		strcpy(authentication_mode,
			__resp.authentication_mode);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_SSID_set_authentication_mode(const char * ifname, const qcsapi_SSID current_SSID, const string_32 authentication_mode)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_SSID_set_authentication_mode_request __req;
	struct qcsapi_SSID_set_authentication_mode_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.current_SSID = (str)current_SSID;
	__req.authentication_mode = (str)authentication_mode;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SSID_SET_AUTHENTICATION_MODE_REMOTE,
				(xdrproc_t)xdr_qcsapi_SSID_set_authentication_mode_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_SSID_set_authentication_mode_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_SSID_set_authentication_mode call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_SSID_get_pre_shared_key(const char * ifname, const qcsapi_SSID current_SSID, const qcsapi_unsigned_int key_index, string_64 pre_shared_key)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_SSID_get_pre_shared_key_request __req;
	struct qcsapi_SSID_get_pre_shared_key_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (pre_shared_key == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.current_SSID = (str)current_SSID;
	__req.key_index = (unsigned int)key_index;
	if (pre_shared_key) {
		pre_shared_key[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SSID_GET_PRE_SHARED_KEY_REMOTE,
				(xdrproc_t)xdr_qcsapi_SSID_get_pre_shared_key_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_SSID_get_pre_shared_key_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_SSID_get_pre_shared_key call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (pre_shared_key && __resp.pre_shared_key)
		strcpy(pre_shared_key,
			__resp.pre_shared_key);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_SSID_set_pre_shared_key(const char * ifname, const qcsapi_SSID current_SSID, const qcsapi_unsigned_int key_index, const string_64 pre_shared_key)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_SSID_set_pre_shared_key_request __req;
	struct qcsapi_SSID_set_pre_shared_key_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.current_SSID = (str)current_SSID;
	__req.key_index = (unsigned int)key_index;
	__req.pre_shared_key = (str)pre_shared_key;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SSID_SET_PRE_SHARED_KEY_REMOTE,
				(xdrproc_t)xdr_qcsapi_SSID_set_pre_shared_key_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_SSID_set_pre_shared_key_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_SSID_set_pre_shared_key call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_SSID_get_key_passphrase(const char * ifname, const qcsapi_SSID current_SSID, const qcsapi_unsigned_int key_index, string_64 passphrase)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_SSID_get_key_passphrase_request __req;
	struct qcsapi_SSID_get_key_passphrase_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (passphrase == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.current_SSID = (str)current_SSID;
	__req.key_index = (unsigned int)key_index;
	if (passphrase) {
		passphrase[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SSID_GET_KEY_PASSPHRASE_REMOTE,
				(xdrproc_t)xdr_qcsapi_SSID_get_key_passphrase_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_SSID_get_key_passphrase_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_SSID_get_key_passphrase call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (passphrase && __resp.passphrase)
		strcpy(passphrase,
			__resp.passphrase);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_SSID_set_key_passphrase(const char * ifname, const qcsapi_SSID current_SSID, const qcsapi_unsigned_int key_index, const string_64 passphrase)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_SSID_set_key_passphrase_request __req;
	struct qcsapi_SSID_set_key_passphrase_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.current_SSID = (str)current_SSID;
	__req.key_index = (unsigned int)key_index;
	__req.passphrase = (str)passphrase;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SSID_SET_KEY_PASSPHRASE_REMOTE,
				(xdrproc_t)xdr_qcsapi_SSID_set_key_passphrase_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_SSID_set_key_passphrase_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_SSID_set_key_passphrase call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_SSID_get_wps_SSID(const char * ifname, qcsapi_SSID wps_SSID)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_SSID_get_wps_SSID_request __req;
	struct qcsapi_SSID_get_wps_SSID_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (wps_SSID == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (wps_SSID) {
		wps_SSID[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SSID_GET_WPS_SSID_REMOTE,
				(xdrproc_t)xdr_qcsapi_SSID_get_wps_SSID_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_SSID_get_wps_SSID_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_SSID_get_wps_SSID call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (wps_SSID && __resp.wps_SSID)
		strcpy(wps_SSID,
			__resp.wps_SSID);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_vlan_config(const char * ifname, qcsapi_vlan_cmd cmd, uint32_t vlanid, uint32_t flags)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_vlan_config_request __req;
	struct qcsapi_wifi_vlan_config_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.cmd = (int)cmd;
	__req.vlanid = (unsigned long)vlanid;
	__req.flags = (unsigned long)flags;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_VLAN_CONFIG_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_vlan_config_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_vlan_config_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_vlan_config call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_show_vlan_config(string_4096 vtable)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_show_vlan_config_request __req;
	struct qcsapi_wifi_show_vlan_config_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (vtable == NULL) {
		return -EFAULT;
	}
	if (vtable) {
		vtable[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SHOW_VLAN_CONFIG_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_show_vlan_config_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_show_vlan_config_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_show_vlan_config call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (vtable && __resp.vtable)
		strcpy(vtable,
			__resp.vtable);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_enable_vlan_pass_through(int * enabled)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_enable_vlan_pass_through_request __req;
	struct qcsapi_enable_vlan_pass_through_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (enabled == NULL) {
		return -EFAULT;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_ENABLE_VLAN_PASS_THROUGH_REMOTE,
				(xdrproc_t)xdr_qcsapi_enable_vlan_pass_through_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_enable_vlan_pass_through_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_enable_vlan_pass_through call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (enabled)
		*enabled = __resp.enabled;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_vlan_promisc(int * enable)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_vlan_promisc_request __req;
	struct qcsapi_wifi_set_vlan_promisc_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (enable == NULL) {
		return -EFAULT;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_VLAN_PROMISC_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_vlan_promisc_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_vlan_promisc_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_vlan_promisc call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (enable)
		*enable = __resp.enable;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_registrar_report_button_press(const char * ifname)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_registrar_report_button_press_request __req;
	struct qcsapi_wps_registrar_report_button_press_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_REGISTRAR_REPORT_BUTTON_PRESS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_registrar_report_button_press_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_registrar_report_button_press_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_registrar_report_button_press call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_registrar_report_pin(const char * ifname, const char * wps_pin)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_registrar_report_pin_request __req;
	struct qcsapi_wps_registrar_report_pin_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.wps_pin = (str)wps_pin;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_REGISTRAR_REPORT_PIN_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_registrar_report_pin_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_registrar_report_pin_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_registrar_report_pin call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_registrar_get_pp_devname(const char * ifname, int blacklist, string_128 pp_devname)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_registrar_get_pp_devname_request __req;
	struct qcsapi_wps_registrar_get_pp_devname_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (pp_devname == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.blacklist = (int)blacklist;
	if (pp_devname) {
		pp_devname[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_REGISTRAR_GET_PP_DEVNAME_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_registrar_get_pp_devname_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_registrar_get_pp_devname_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_registrar_get_pp_devname call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (pp_devname && __resp.pp_devname)
		strcpy(pp_devname,
			__resp.pp_devname);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_registrar_set_pp_devname(const char * ifname, int update_blacklist, const string_256 pp_devname)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_registrar_set_pp_devname_request __req;
	struct qcsapi_wps_registrar_set_pp_devname_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.update_blacklist = (int)update_blacklist;
	__req.pp_devname = (str)pp_devname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_REGISTRAR_SET_PP_DEVNAME_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_registrar_set_pp_devname_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_registrar_set_pp_devname_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_registrar_set_pp_devname call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_enrollee_report_button_press(const char * ifname, const qcsapi_mac_addr bssid)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_enrollee_report_button_press_request __req;
	struct qcsapi_wps_enrollee_report_button_press_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (bssid == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	memcpy(__req.bssid, bssid, sizeof(const qcsapi_mac_addr));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_ENROLLEE_REPORT_BUTTON_PRESS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_enrollee_report_button_press_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_enrollee_report_button_press_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_enrollee_report_button_press call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_enrollee_report_pin(const char * ifname, const qcsapi_mac_addr bssid, const char * wps_pin)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_enrollee_report_pin_request __req;
	struct qcsapi_wps_enrollee_report_pin_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (bssid == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	memcpy(__req.bssid, bssid, sizeof(const qcsapi_mac_addr));
	__req.wps_pin = (str)wps_pin;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_ENROLLEE_REPORT_PIN_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_enrollee_report_pin_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_enrollee_report_pin_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_enrollee_report_pin call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_enrollee_generate_pin(const char * ifname, const qcsapi_mac_addr bssid, char * wps_pin)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_enrollee_generate_pin_request __req;
	struct qcsapi_wps_enrollee_generate_pin_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (bssid == NULL || wps_pin == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	memcpy(__req.bssid, bssid, sizeof(const qcsapi_mac_addr));
	if (wps_pin) {
		wps_pin[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_ENROLLEE_GENERATE_PIN_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_enrollee_generate_pin_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_enrollee_generate_pin_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_enrollee_generate_pin call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (wps_pin && __resp.wps_pin)
		strcpy(wps_pin,
			__resp.wps_pin);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_get_ap_pin(const char * ifname, char * wps_pin, int force_regenerate)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_get_ap_pin_request __req;
	struct qcsapi_wps_get_ap_pin_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (wps_pin == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (wps_pin) {
		wps_pin[0] = 0;
	}
	__req.force_regenerate = (int)force_regenerate;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_GET_AP_PIN_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_get_ap_pin_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_get_ap_pin_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_get_ap_pin call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (wps_pin && __resp.wps_pin)
		strcpy(wps_pin,
			__resp.wps_pin);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_set_ap_pin(const char * ifname, const char * wps_pin)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_set_ap_pin_request __req;
	struct qcsapi_wps_set_ap_pin_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.wps_pin = (str)wps_pin;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_SET_AP_PIN_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_set_ap_pin_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_set_ap_pin_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_set_ap_pin call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_save_ap_pin(const char * ifname)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_save_ap_pin_request __req;
	struct qcsapi_wps_save_ap_pin_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_SAVE_AP_PIN_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_save_ap_pin_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_save_ap_pin_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_save_ap_pin call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_get_sta_pin(const char * ifname, char * wps_pin)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_get_sta_pin_request __req;
	struct qcsapi_wps_get_sta_pin_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (wps_pin == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (wps_pin) {
		wps_pin[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_GET_STA_PIN_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_get_sta_pin_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_get_sta_pin_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_get_sta_pin call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (wps_pin && __resp.wps_pin)
		strcpy(wps_pin,
			__resp.wps_pin);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_get_state(const char * ifname, char * wps_state, const qcsapi_unsigned_int max_len)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_get_state_request __req;
	struct qcsapi_wps_get_state_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (wps_state == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (wps_state) {
		wps_state[0] = 0;
	}
	__req.max_len = (unsigned int)max_len;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_GET_STATE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_get_state_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_get_state_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_get_state call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (wps_state && __resp.wps_state)
		strcpy(wps_state,
			__resp.wps_state);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_get_configured_state(const char * ifname, char * wps_state, const qcsapi_unsigned_int max_len)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_get_configured_state_request __req;
	struct qcsapi_wps_get_configured_state_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (wps_state == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (wps_state) {
		wps_state[0] = 0;
	}
	__req.max_len = (unsigned int)max_len;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_GET_CONFIGURED_STATE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_get_configured_state_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_get_configured_state_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_get_configured_state call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (wps_state && __resp.wps_state)
		strcpy(wps_state,
			__resp.wps_state);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_get_runtime_state(const char * ifname, char * state, int max_len)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_get_runtime_state_request __req;
	struct qcsapi_wps_get_runtime_state_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (state == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (state) {
		state[0] = 0;
	}
	__req.max_len = (int)max_len;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_GET_RUNTIME_STATE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_get_runtime_state_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_get_runtime_state_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_get_runtime_state call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (state && __resp.state)
		strcpy(state,
			__resp.state);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_set_configured_state(const char * ifname, const qcsapi_unsigned_int state)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_set_configured_state_request __req;
	struct qcsapi_wps_set_configured_state_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.state = (unsigned int)state;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_SET_CONFIGURED_STATE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_set_configured_state_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_set_configured_state_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_set_configured_state call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_get_param(const char * ifname, qcsapi_wps_param_type wps_type, char * wps_str, const qcsapi_unsigned_int max_len)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_get_param_request __req;
	struct qcsapi_wps_get_param_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (wps_str == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.wps_type = (int)wps_type;
	if (wps_str) {
		wps_str[0] = 0;
	}
	__req.max_len = (unsigned int)max_len;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_GET_PARAM_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_get_param_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_get_param_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_get_param call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (wps_str && __resp.wps_str)
		strcpy(wps_str,
			__resp.wps_str);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_set_timeout(const char * ifname, const int value)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_set_timeout_request __req;
	struct qcsapi_wps_set_timeout_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.value = (int)value;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_SET_TIMEOUT_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_set_timeout_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_set_timeout_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_set_timeout call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_on_hidden_ssid(const char * ifname, const int value)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_on_hidden_ssid_request __req;
	struct qcsapi_wps_on_hidden_ssid_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.value = (int)value;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_ON_HIDDEN_SSID_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_on_hidden_ssid_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_on_hidden_ssid_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_on_hidden_ssid call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_on_hidden_ssid_status(const char * ifname, char * state, int max_len)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_on_hidden_ssid_status_request __req;
	struct qcsapi_wps_on_hidden_ssid_status_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (state == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (state) {
		state[0] = 0;
	}
	__req.max_len = (int)max_len;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_ON_HIDDEN_SSID_STATUS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_on_hidden_ssid_status_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_on_hidden_ssid_status_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_on_hidden_ssid_status call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (state && __resp.state)
		strcpy(state,
			__resp.state);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_upnp_enable(const char * ifname, const int value)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_upnp_enable_request __req;
	struct qcsapi_wps_upnp_enable_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.value = (int)value;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_UPNP_ENABLE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_upnp_enable_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_upnp_enable_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_upnp_enable call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_upnp_status(const char * ifname, char * reply, int reply_len)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_upnp_status_request __req;
	struct qcsapi_wps_upnp_status_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (reply == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (reply) {
		reply[0] = 0;
	}
	__req.reply_len = (int)reply_len;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_UPNP_STATUS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_upnp_status_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_upnp_status_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_upnp_status call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (reply && __resp.reply)
		strcpy(reply,
			__resp.reply);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_allow_pbc_overlap(const char * ifname, const qcsapi_unsigned_int allow)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_allow_pbc_overlap_request __req;
	struct qcsapi_wps_allow_pbc_overlap_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.allow = (unsigned int)allow;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_ALLOW_PBC_OVERLAP_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_allow_pbc_overlap_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_allow_pbc_overlap_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_allow_pbc_overlap call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_get_allow_pbc_overlap_status(const char * ifname, int * status)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_get_allow_pbc_overlap_status_request __req;
	struct qcsapi_wps_get_allow_pbc_overlap_status_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (status == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_GET_ALLOW_PBC_OVERLAP_STATUS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_get_allow_pbc_overlap_status_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_get_allow_pbc_overlap_status_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_get_allow_pbc_overlap_status call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (status)
		*status = __resp.status;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_set_access_control(const char * ifname, uint32_t ctrl_state)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_set_access_control_request __req;
	struct qcsapi_wps_set_access_control_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.ctrl_state = (unsigned long)ctrl_state;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_SET_ACCESS_CONTROL_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_set_access_control_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_set_access_control_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_set_access_control call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_get_access_control(const char * ifname, uint32_t * ctrl_state)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_get_access_control_request __req;
	struct qcsapi_wps_get_access_control_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (ctrl_state == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_GET_ACCESS_CONTROL_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_get_access_control_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_get_access_control_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_get_access_control call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (ctrl_state)
		*ctrl_state = __resp.ctrl_state;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_set_param(const char * ifname, const qcsapi_wps_param_type param_type, const char * param_value)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_set_param_request __req;
	struct qcsapi_wps_set_param_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.param_type = (int)param_type;
	__req.param_value = (str)param_value;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_SET_PARAM_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_set_param_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_set_param_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_set_param call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_cancel(const char * ifname)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_cancel_request __req;
	struct qcsapi_wps_cancel_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_CANCEL_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_cancel_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_cancel_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_cancel call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_set_pbc_in_srcm(const char * ifname, const qcsapi_unsigned_int enabled)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_set_pbc_in_srcm_request __req;
	struct qcsapi_wps_set_pbc_in_srcm_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.enabled = (unsigned int)enabled;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_SET_PBC_IN_SRCM_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_set_pbc_in_srcm_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_set_pbc_in_srcm_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_set_pbc_in_srcm call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wps_get_pbc_in_srcm(const char * ifname, qcsapi_unsigned_int * p_enabled)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wps_get_pbc_in_srcm_request __req;
	struct qcsapi_wps_get_pbc_in_srcm_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_enabled == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WPS_GET_PBC_IN_SRCM_REMOTE,
				(xdrproc_t)xdr_qcsapi_wps_get_pbc_in_srcm_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wps_get_pbc_in_srcm_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wps_get_pbc_in_srcm call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_enabled)
		*p_enabled = __resp.p_enabled;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_registrar_set_default_pbc_bss(const char * ifname)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_registrar_set_default_pbc_bss_request __req;
	struct qcsapi_registrar_set_default_pbc_bss_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_REGISTRAR_SET_DEFAULT_PBC_BSS_REMOTE,
				(xdrproc_t)xdr_qcsapi_registrar_set_default_pbc_bss_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_registrar_set_default_pbc_bss_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_registrar_set_default_pbc_bss call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_registrar_get_default_pbc_bss(char * default_bss, int len)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_registrar_get_default_pbc_bss_request __req;
	struct qcsapi_registrar_get_default_pbc_bss_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (default_bss == NULL) {
		return -EFAULT;
	}
	if (default_bss) {
		default_bss[0] = 0;
	}
	__req.len = (int)len;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_REGISTRAR_GET_DEFAULT_PBC_BSS_REMOTE,
				(xdrproc_t)xdr_qcsapi_registrar_get_default_pbc_bss_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_registrar_get_default_pbc_bss_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_registrar_get_default_pbc_bss call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (default_bss && __resp.default_bss)
		strcpy(default_bss,
			__resp.default_bss);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_gpio_set_config(const uint8_t gpio_pin, const qcsapi_gpio_config new_gpio_config)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_gpio_set_config_request __req;
	struct qcsapi_gpio_set_config_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.gpio_pin = (unsigned int)gpio_pin;
	__req.new_gpio_config = (int)new_gpio_config;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_GPIO_SET_CONFIG_REMOTE,
				(xdrproc_t)xdr_qcsapi_gpio_set_config_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_gpio_set_config_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_gpio_set_config call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_gpio_get_config(const uint8_t gpio_pin, qcsapi_gpio_config * p_gpio_config)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_gpio_get_config_request __req;
	struct qcsapi_gpio_get_config_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_gpio_config == NULL) {
		return -EFAULT;
	}
	__req.gpio_pin = (unsigned int)gpio_pin;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_GPIO_GET_CONFIG_REMOTE,
				(xdrproc_t)xdr_qcsapi_gpio_get_config_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_gpio_get_config_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_gpio_get_config call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_gpio_config)
		*p_gpio_config = __resp.p_gpio_config;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_led_get(const uint8_t led_ident, uint8_t * p_led_setting)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_led_get_request __req;
	struct qcsapi_led_get_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_led_setting == NULL) {
		return -EFAULT;
	}
	__req.led_ident = (unsigned int)led_ident;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_LED_GET_REMOTE,
				(xdrproc_t)xdr_qcsapi_led_get_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_led_get_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_led_get call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_led_setting)
		*p_led_setting = __resp.p_led_setting;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_led_set(const uint8_t led_ident, const uint8_t new_led_setting)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_led_set_request __req;
	struct qcsapi_led_set_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.led_ident = (unsigned int)led_ident;
	__req.new_led_setting = (unsigned int)new_led_setting;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_LED_SET_REMOTE,
				(xdrproc_t)xdr_qcsapi_led_set_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_led_set_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_led_set call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_gpio_monitor_reset_device(const uint8_t reset_device_pin, const uint8_t active_logic, const int blocking_flag, reset_device_callback respond_reset_device)
{
	/* stubbed, not implemented */
	fprintf(stderr, "%s not implemented\n", "qcsapi_gpio_monitor_reset_device");
	return -qcsapi_programming_error;
}
int qcsapi_gpio_enable_wps_push_button(const uint8_t wps_push_button, const uint8_t active_logic, const uint8_t use_interrupt_flag)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_gpio_enable_wps_push_button_request __req;
	struct qcsapi_gpio_enable_wps_push_button_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.wps_push_button = (unsigned int)wps_push_button;
	__req.active_logic = (unsigned int)active_logic;
	__req.use_interrupt_flag = (unsigned int)use_interrupt_flag;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_GPIO_ENABLE_WPS_PUSH_BUTTON_REMOTE,
				(xdrproc_t)xdr_qcsapi_gpio_enable_wps_push_button_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_gpio_enable_wps_push_button_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_gpio_enable_wps_push_button call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_count_associations(const char * ifname, qcsapi_unsigned_int * p_association_count)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_count_associations_request __req;
	struct qcsapi_wifi_get_count_associations_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_association_count == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_COUNT_ASSOCIATIONS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_count_associations_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_count_associations_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_count_associations call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_association_count)
		*p_association_count = __resp.p_association_count;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_associated_device_mac_addr(const char * ifname, const qcsapi_unsigned_int device_index, qcsapi_mac_addr device_mac_addr)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_associated_device_mac_addr_request __req;
	struct qcsapi_wifi_get_associated_device_mac_addr_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (device_mac_addr == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.device_index = (unsigned int)device_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_ASSOCIATED_DEVICE_MAC_ADDR_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_associated_device_mac_addr_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_associated_device_mac_addr_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_associated_device_mac_addr call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (device_mac_addr && __resp.device_mac_addr)
		memcpy(device_mac_addr,
			__resp.device_mac_addr,
			sizeof(__resp.device_mac_addr));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_associated_device_ip_addr(const char * ifname, const qcsapi_unsigned_int device_index, unsigned int * ip_addr)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_associated_device_ip_addr_request __req;
	struct qcsapi_wifi_get_associated_device_ip_addr_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (ip_addr == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.device_index = (unsigned int)device_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_ASSOCIATED_DEVICE_IP_ADDR_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_associated_device_ip_addr_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_associated_device_ip_addr_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_associated_device_ip_addr call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (ip_addr)
		*ip_addr = __resp.ip_addr;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_link_quality(const char * ifname, const qcsapi_unsigned_int association_index, qcsapi_unsigned_int * p_link_quality)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_link_quality_request __req;
	struct qcsapi_wifi_get_link_quality_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_link_quality == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.association_index = (unsigned int)association_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_LINK_QUALITY_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_link_quality_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_link_quality_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_link_quality call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_link_quality)
		*p_link_quality = __resp.p_link_quality;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_rx_bytes_per_association(const char * ifname, const qcsapi_unsigned_int association_index, u_int64_t * p_rx_bytes)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_rx_bytes_per_association_request __req;
	struct qcsapi_wifi_get_rx_bytes_per_association_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_rx_bytes == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.association_index = (unsigned int)association_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_RX_BYTES_PER_ASSOCIATION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_rx_bytes_per_association_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_rx_bytes_per_association_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_rx_bytes_per_association call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_rx_bytes)
		sscanf(__resp.p_rx_bytes, "%" SCNu64, p_rx_bytes);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_tx_bytes_per_association(const char * ifname, const qcsapi_unsigned_int association_index, u_int64_t * p_tx_bytes)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_tx_bytes_per_association_request __req;
	struct qcsapi_wifi_get_tx_bytes_per_association_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_tx_bytes == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.association_index = (unsigned int)association_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_TX_BYTES_PER_ASSOCIATION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_tx_bytes_per_association_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_tx_bytes_per_association_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_tx_bytes_per_association call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_tx_bytes)
		sscanf(__resp.p_tx_bytes, "%" SCNu64, p_tx_bytes);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_rx_packets_per_association(const char * ifname, const qcsapi_unsigned_int association_index, qcsapi_unsigned_int * p_rx_packets)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_rx_packets_per_association_request __req;
	struct qcsapi_wifi_get_rx_packets_per_association_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_rx_packets == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.association_index = (unsigned int)association_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_RX_PACKETS_PER_ASSOCIATION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_rx_packets_per_association_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_rx_packets_per_association_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_rx_packets_per_association call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_rx_packets)
		*p_rx_packets = __resp.p_rx_packets;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_tx_packets_per_association(const char * ifname, const qcsapi_unsigned_int association_index, qcsapi_unsigned_int * p_tx_packets)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_tx_packets_per_association_request __req;
	struct qcsapi_wifi_get_tx_packets_per_association_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_tx_packets == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.association_index = (unsigned int)association_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_TX_PACKETS_PER_ASSOCIATION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_tx_packets_per_association_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_tx_packets_per_association_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_tx_packets_per_association call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_tx_packets)
		*p_tx_packets = __resp.p_tx_packets;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_tx_err_packets_per_association(const char * ifname, const qcsapi_unsigned_int association_index, qcsapi_unsigned_int * p_tx_err_packets)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_tx_err_packets_per_association_request __req;
	struct qcsapi_wifi_get_tx_err_packets_per_association_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_tx_err_packets == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.association_index = (unsigned int)association_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_TX_ERR_PACKETS_PER_ASSOCIATION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_tx_err_packets_per_association_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_tx_err_packets_per_association_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_tx_err_packets_per_association call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_tx_err_packets)
		*p_tx_err_packets = __resp.p_tx_err_packets;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_rssi_per_association(const char * ifname, const qcsapi_unsigned_int association_index, qcsapi_unsigned_int * p_rssi)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_rssi_per_association_request __req;
	struct qcsapi_wifi_get_rssi_per_association_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_rssi == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.association_index = (unsigned int)association_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_RSSI_PER_ASSOCIATION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_rssi_per_association_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_rssi_per_association_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_rssi_per_association call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_rssi)
		*p_rssi = __resp.p_rssi;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_rssi_in_dbm_per_association(const char * ifname, const qcsapi_unsigned_int association_index, int * p_rssi)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_rssi_in_dbm_per_association_request __req;
	struct qcsapi_wifi_get_rssi_in_dbm_per_association_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_rssi == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.association_index = (unsigned int)association_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_RSSI_IN_DBM_PER_ASSOCIATION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_rssi_in_dbm_per_association_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_rssi_in_dbm_per_association_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_rssi_in_dbm_per_association call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_rssi)
		*p_rssi = __resp.p_rssi;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_bw_per_association(const char * ifname, const qcsapi_unsigned_int association_index, qcsapi_unsigned_int * p_bw)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_bw_per_association_request __req;
	struct qcsapi_wifi_get_bw_per_association_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_bw == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.association_index = (unsigned int)association_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_BW_PER_ASSOCIATION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_bw_per_association_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_bw_per_association_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_bw_per_association call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_bw)
		*p_bw = __resp.p_bw;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_tx_phy_rate_per_association(const char * ifname, const qcsapi_unsigned_int association_index, qcsapi_unsigned_int * p_tx_phy_rate)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_tx_phy_rate_per_association_request __req;
	struct qcsapi_wifi_get_tx_phy_rate_per_association_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_tx_phy_rate == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.association_index = (unsigned int)association_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_TX_PHY_RATE_PER_ASSOCIATION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_tx_phy_rate_per_association_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_tx_phy_rate_per_association_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_tx_phy_rate_per_association call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_tx_phy_rate)
		*p_tx_phy_rate = __resp.p_tx_phy_rate;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_rx_phy_rate_per_association(const char * ifname, const qcsapi_unsigned_int association_index, qcsapi_unsigned_int * p_rx_phy_rate)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_rx_phy_rate_per_association_request __req;
	struct qcsapi_wifi_get_rx_phy_rate_per_association_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_rx_phy_rate == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.association_index = (unsigned int)association_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_RX_PHY_RATE_PER_ASSOCIATION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_rx_phy_rate_per_association_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_rx_phy_rate_per_association_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_rx_phy_rate_per_association call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_rx_phy_rate)
		*p_rx_phy_rate = __resp.p_rx_phy_rate;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_tx_mcs_per_association(const char * ifname, const qcsapi_unsigned_int association_index, qcsapi_unsigned_int * p_mcs)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_tx_mcs_per_association_request __req;
	struct qcsapi_wifi_get_tx_mcs_per_association_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_mcs == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.association_index = (unsigned int)association_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_TX_MCS_PER_ASSOCIATION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_tx_mcs_per_association_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_tx_mcs_per_association_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_tx_mcs_per_association call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_mcs)
		*p_mcs = __resp.p_mcs;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_rx_mcs_per_association(const char * ifname, const qcsapi_unsigned_int association_index, qcsapi_unsigned_int * p_mcs)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_rx_mcs_per_association_request __req;
	struct qcsapi_wifi_get_rx_mcs_per_association_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_mcs == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.association_index = (unsigned int)association_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_RX_MCS_PER_ASSOCIATION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_rx_mcs_per_association_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_rx_mcs_per_association_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_rx_mcs_per_association call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_mcs)
		*p_mcs = __resp.p_mcs;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_achievable_tx_phy_rate_per_association(const char * ifname, const qcsapi_unsigned_int association_index, qcsapi_unsigned_int * p_achievable_tx_phy_rate)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_achievable_tx_phy_rate_per_association_request __req;
	struct qcsapi_wifi_get_achievable_tx_phy_rate_per_association_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_achievable_tx_phy_rate == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.association_index = (unsigned int)association_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_ACHIEVABLE_TX_PHY_RATE_PER_ASSOCIATION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_achievable_tx_phy_rate_per_association_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_achievable_tx_phy_rate_per_association_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_achievable_tx_phy_rate_per_association call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_achievable_tx_phy_rate)
		*p_achievable_tx_phy_rate = __resp.p_achievable_tx_phy_rate;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_achievable_rx_phy_rate_per_association(const char * ifname, const qcsapi_unsigned_int association_index, qcsapi_unsigned_int * p_achievable_rx_phy_rate)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_achievable_rx_phy_rate_per_association_request __req;
	struct qcsapi_wifi_get_achievable_rx_phy_rate_per_association_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_achievable_rx_phy_rate == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.association_index = (unsigned int)association_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_ACHIEVABLE_RX_PHY_RATE_PER_ASSOCIATION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_achievable_rx_phy_rate_per_association_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_achievable_rx_phy_rate_per_association_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_achievable_rx_phy_rate_per_association call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_achievable_rx_phy_rate)
		*p_achievable_rx_phy_rate = __resp.p_achievable_rx_phy_rate;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_snr_per_association(const char * ifname, const qcsapi_unsigned_int association_index, int * p_snr)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_snr_per_association_request __req;
	struct qcsapi_wifi_get_snr_per_association_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_snr == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.association_index = (unsigned int)association_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_SNR_PER_ASSOCIATION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_snr_per_association_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_snr_per_association_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_snr_per_association call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_snr)
		*p_snr = __resp.p_snr;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_time_associated_per_association(const char * ifname, const qcsapi_unsigned_int association_index, qcsapi_unsigned_int * time_associated)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_time_associated_per_association_request __req;
	struct qcsapi_wifi_get_time_associated_per_association_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (time_associated == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.association_index = (unsigned int)association_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_TIME_ASSOCIATED_PER_ASSOCIATION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_time_associated_per_association_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_time_associated_per_association_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_time_associated_per_association call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (time_associated)
		*time_associated = __resp.time_associated;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_node_param(const char * ifname, const uint32_t node_index, qcsapi_per_assoc_param param_type, int local_remote_flag, string_128 input_param_str, qcsapi_measure_report_result * report_result)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_node_param_request __req;
	struct qcsapi_wifi_get_node_param_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (input_param_str == NULL || report_result == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.node_index = (unsigned long)node_index;
	__req.param_type = (int)param_type;
	__req.local_remote_flag = (int)local_remote_flag;
	if (input_param_str) {
		input_param_str[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_NODE_PARAM_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_node_param_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_node_param_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_node_param call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (input_param_str && __resp.input_param_str)
		strcpy(input_param_str,
			__resp.input_param_str);
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(report_result->common); __i_0++) {
			report_result->common[__i_0] = __resp.report_result_common[__i_0];
		}
	}
	report_result->basic = __resp.report_result_basic;
	report_result->cca = __resp.report_result_cca;
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(report_result->rpi); __i_0++) {
			report_result->rpi[__i_0] = __resp.report_result_rpi[__i_0];
		}
	}
	report_result->channel_load = __resp.report_result_channel_load;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_node_counter(const char * ifname, const uint32_t node_index, qcsapi_counter_type counter_type, int local_remote_flag, uint64_t * p_value)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_node_counter_request __req;
	struct qcsapi_wifi_get_node_counter_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_value == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.node_index = (unsigned long)node_index;
	__req.counter_type = (int)counter_type;
	__req.local_remote_flag = (int)local_remote_flag;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_NODE_COUNTER_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_node_counter_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_node_counter_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_node_counter call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_value)
		sscanf(__resp.p_value, "%" SCNu64, p_value);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_node_stats(const char * ifname, const uint32_t node_index, int local_remote_flag, struct qcsapi_node_stats * p_stats)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_node_stats_request __req;
	struct qcsapi_wifi_get_node_stats_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_stats == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.node_index = (unsigned long)node_index;
	__req.local_remote_flag = (int)local_remote_flag;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_NODE_STATS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_node_stats_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_node_stats_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_node_stats call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	sscanf(__resp.p_stats_tx_bytes, "%" SCNu64, &p_stats->tx_bytes);
	p_stats->tx_pkts = __resp.p_stats_tx_pkts;
	p_stats->tx_discard = __resp.p_stats_tx_discard;
	p_stats->tx_err = __resp.p_stats_tx_err;
	p_stats->tx_unicast = __resp.p_stats_tx_unicast;
	p_stats->tx_multicast = __resp.p_stats_tx_multicast;
	p_stats->tx_broadcast = __resp.p_stats_tx_broadcast;
	sscanf(__resp.p_stats_rx_bytes, "%" SCNu64, &p_stats->rx_bytes);
	p_stats->rx_pkts = __resp.p_stats_rx_pkts;
	p_stats->rx_discard = __resp.p_stats_rx_discard;
	p_stats->rx_err = __resp.p_stats_rx_err;
	p_stats->rx_unicast = __resp.p_stats_rx_unicast;
	p_stats->rx_multicast = __resp.p_stats_rx_multicast;
	p_stats->rx_broadcast = __resp.p_stats_rx_broadcast;
	p_stats->rx_unknown = __resp.p_stats_rx_unknown;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_max_queued(const char * ifname, const uint32_t node_index, int local_remote_flag, int reset_flag, uint32_t * max_queued)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_max_queued_request __req;
	struct qcsapi_wifi_get_max_queued_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (max_queued == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.node_index = (unsigned long)node_index;
	__req.local_remote_flag = (int)local_remote_flag;
	__req.reset_flag = (int)reset_flag;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_MAX_QUEUED_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_max_queued_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_max_queued_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_max_queued call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (max_queued)
		*max_queued = __resp.max_queued;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_hw_noise_per_association(const char * ifname, const qcsapi_unsigned_int association_index, int * p_hw_noise)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_hw_noise_per_association_request __req;
	struct qcsapi_wifi_get_hw_noise_per_association_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_hw_noise == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.association_index = (unsigned int)association_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_HW_NOISE_PER_ASSOCIATION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_hw_noise_per_association_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_hw_noise_per_association_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_hw_noise_per_association call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_hw_noise)
		*p_hw_noise = __resp.p_hw_noise;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_list_regulatory_regions(string_128 list_regulatory_regions)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_list_regulatory_regions_request __req;
	struct qcsapi_wifi_get_list_regulatory_regions_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (list_regulatory_regions == NULL) {
		return -EFAULT;
	}
	if (list_regulatory_regions) {
		list_regulatory_regions[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_LIST_REGULATORY_REGIONS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_list_regulatory_regions_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_list_regulatory_regions_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_list_regulatory_regions call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (list_regulatory_regions && __resp.list_regulatory_regions)
		strcpy(list_regulatory_regions,
			__resp.list_regulatory_regions);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_regulatory_get_list_regulatory_regions(string_128 list_regulatory_regions)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_regulatory_get_list_regulatory_regions_request __req;
	struct qcsapi_regulatory_get_list_regulatory_regions_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (list_regulatory_regions == NULL) {
		return -EFAULT;
	}
	if (list_regulatory_regions) {
		list_regulatory_regions[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_REGULATORY_GET_LIST_REGULATORY_REGIONS_REMOTE,
				(xdrproc_t)xdr_qcsapi_regulatory_get_list_regulatory_regions_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_regulatory_get_list_regulatory_regions_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_regulatory_get_list_regulatory_regions call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (list_regulatory_regions && __resp.list_regulatory_regions)
		strcpy(list_regulatory_regions,
			__resp.list_regulatory_regions);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_list_regulatory_channels(const char * region_by_name, const qcsapi_unsigned_int bw, string_1024 list_of_channels)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_list_regulatory_channels_request __req;
	struct qcsapi_wifi_get_list_regulatory_channels_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (list_of_channels == NULL) {
		return -EFAULT;
	}
	__req.region_by_name = (str)region_by_name;
	__req.bw = (unsigned int)bw;
	if (list_of_channels) {
		list_of_channels[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_LIST_REGULATORY_CHANNELS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_list_regulatory_channels_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_list_regulatory_channels_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_list_regulatory_channels call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (list_of_channels && __resp.list_of_channels)
		strcpy(list_of_channels,
			__resp.list_of_channels);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_regulatory_get_list_regulatory_channels(const char * region_by_name, const qcsapi_unsigned_int bw, string_1024 list_of_channels)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_regulatory_get_list_regulatory_channels_request __req;
	struct qcsapi_regulatory_get_list_regulatory_channels_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (list_of_channels == NULL) {
		return -EFAULT;
	}
	__req.region_by_name = (str)region_by_name;
	__req.bw = (unsigned int)bw;
	if (list_of_channels) {
		list_of_channels[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_REGULATORY_GET_LIST_REGULATORY_CHANNELS_REMOTE,
				(xdrproc_t)xdr_qcsapi_regulatory_get_list_regulatory_channels_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_regulatory_get_list_regulatory_channels_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_regulatory_get_list_regulatory_channels call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (list_of_channels && __resp.list_of_channels)
		strcpy(list_of_channels,
			__resp.list_of_channels);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_regulatory_get_list_regulatory_bands(const char * region_by_name, string_128 list_of_channels)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_regulatory_get_list_regulatory_bands_request __req;
	struct qcsapi_regulatory_get_list_regulatory_bands_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (list_of_channels == NULL) {
		return -EFAULT;
	}
	__req.region_by_name = (str)region_by_name;
	if (list_of_channels) {
		list_of_channels[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_REGULATORY_GET_LIST_REGULATORY_BANDS_REMOTE,
				(xdrproc_t)xdr_qcsapi_regulatory_get_list_regulatory_bands_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_regulatory_get_list_regulatory_bands_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_regulatory_get_list_regulatory_bands call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (list_of_channels && __resp.list_of_channels)
		strcpy(list_of_channels,
			__resp.list_of_channels);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_regulatory_tx_power(const char * ifname, const qcsapi_unsigned_int the_channel, const char * region_by_name, int * p_tx_power)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_regulatory_tx_power_request __req;
	struct qcsapi_wifi_get_regulatory_tx_power_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_tx_power == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.the_channel = (unsigned int)the_channel;
	__req.region_by_name = (str)region_by_name;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_REGULATORY_TX_POWER_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_regulatory_tx_power_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_regulatory_tx_power_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_regulatory_tx_power call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_tx_power)
		*p_tx_power = __resp.p_tx_power;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_regulatory_get_regulatory_tx_power(const char * ifname, const qcsapi_unsigned_int the_channel, const char * region_by_name, int * p_tx_power)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_regulatory_get_regulatory_tx_power_request __req;
	struct qcsapi_regulatory_get_regulatory_tx_power_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_tx_power == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.the_channel = (unsigned int)the_channel;
	__req.region_by_name = (str)region_by_name;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_REGULATORY_GET_REGULATORY_TX_POWER_REMOTE,
				(xdrproc_t)xdr_qcsapi_regulatory_get_regulatory_tx_power_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_regulatory_get_regulatory_tx_power_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_regulatory_get_regulatory_tx_power call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_tx_power)
		*p_tx_power = __resp.p_tx_power;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_configured_tx_power(const char * ifname, const qcsapi_unsigned_int the_channel, const char * region_by_name, const qcsapi_unsigned_int bw, int * p_tx_power)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_configured_tx_power_request __req;
	struct qcsapi_wifi_get_configured_tx_power_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_tx_power == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.the_channel = (unsigned int)the_channel;
	__req.region_by_name = (str)region_by_name;
	__req.bw = (unsigned int)bw;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_CONFIGURED_TX_POWER_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_configured_tx_power_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_configured_tx_power_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_configured_tx_power call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_tx_power)
		*p_tx_power = __resp.p_tx_power;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_regulatory_get_configured_tx_power(const char * ifname, const qcsapi_unsigned_int the_channel, const char * region_by_name, const qcsapi_unsigned_int bw, int * p_tx_power)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_regulatory_get_configured_tx_power_request __req;
	struct qcsapi_regulatory_get_configured_tx_power_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_tx_power == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.the_channel = (unsigned int)the_channel;
	__req.region_by_name = (str)region_by_name;
	__req.bw = (unsigned int)bw;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_REGULATORY_GET_CONFIGURED_TX_POWER_REMOTE,
				(xdrproc_t)xdr_qcsapi_regulatory_get_configured_tx_power_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_regulatory_get_configured_tx_power_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_regulatory_get_configured_tx_power call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_tx_power)
		*p_tx_power = __resp.p_tx_power;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_regulatory_region(const char * ifname, const char * region_by_name)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_regulatory_region_request __req;
	struct qcsapi_wifi_set_regulatory_region_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.region_by_name = (str)region_by_name;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_REGULATORY_REGION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_regulatory_region_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_regulatory_region_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_regulatory_region call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_regulatory_set_regulatory_region(const char * ifname, const char * region_by_name)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_regulatory_set_regulatory_region_request __req;
	struct qcsapi_regulatory_set_regulatory_region_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.region_by_name = (str)region_by_name;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_REGULATORY_SET_REGULATORY_REGION_REMOTE,
				(xdrproc_t)xdr_qcsapi_regulatory_set_regulatory_region_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_regulatory_set_regulatory_region_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_regulatory_set_regulatory_region call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_regulatory_region(const char * ifname, char * region_by_name)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_regulatory_region_request __req;
	struct qcsapi_wifi_get_regulatory_region_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (region_by_name == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (region_by_name) {
		region_by_name[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_REGULATORY_REGION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_regulatory_region_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_regulatory_region_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_regulatory_region call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (region_by_name && __resp.region_by_name)
		strcpy(region_by_name,
			__resp.region_by_name);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_regulatory_overwrite_country_code(const char * ifname, const char * curr_country_name, const char * new_country_name)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_regulatory_overwrite_country_code_request __req;
	struct qcsapi_regulatory_overwrite_country_code_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.curr_country_name = (str)curr_country_name;
	__req.new_country_name = (str)new_country_name;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_REGULATORY_OVERWRITE_COUNTRY_CODE_REMOTE,
				(xdrproc_t)xdr_qcsapi_regulatory_overwrite_country_code_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_regulatory_overwrite_country_code_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_regulatory_overwrite_country_code call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_regulatory_channel(const char * ifname, const qcsapi_unsigned_int the_channel, const char * region_by_name, const qcsapi_unsigned_int tx_power_offset)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_regulatory_channel_request __req;
	struct qcsapi_wifi_set_regulatory_channel_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.the_channel = (unsigned int)the_channel;
	__req.region_by_name = (str)region_by_name;
	__req.tx_power_offset = (unsigned int)tx_power_offset;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_REGULATORY_CHANNEL_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_regulatory_channel_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_regulatory_channel_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_regulatory_channel call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_regulatory_set_regulatory_channel(const char * ifname, const qcsapi_unsigned_int the_channel, const char * region_by_name, const qcsapi_unsigned_int tx_power_offset)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_regulatory_set_regulatory_channel_request __req;
	struct qcsapi_regulatory_set_regulatory_channel_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.the_channel = (unsigned int)the_channel;
	__req.region_by_name = (str)region_by_name;
	__req.tx_power_offset = (unsigned int)tx_power_offset;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_REGULATORY_SET_REGULATORY_CHANNEL_REMOTE,
				(xdrproc_t)xdr_qcsapi_regulatory_set_regulatory_channel_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_regulatory_set_regulatory_channel_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_regulatory_set_regulatory_channel call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_regulatory_get_db_version(int * p_version, const int index)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_regulatory_get_db_version_request __req;
	struct qcsapi_regulatory_get_db_version_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_version == NULL) {
		return -EFAULT;
	}
	__req.index = (int)index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_REGULATORY_GET_DB_VERSION_REMOTE,
				(xdrproc_t)xdr_qcsapi_regulatory_get_db_version_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_regulatory_get_db_version_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_regulatory_get_db_version call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_version)
		*p_version = __resp.p_version;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_list_DFS_channels(const char * region_by_name, const int DFS_flag, const qcsapi_unsigned_int bw, string_1024 list_of_channels)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_list_DFS_channels_request __req;
	struct qcsapi_wifi_get_list_DFS_channels_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (list_of_channels == NULL) {
		return -EFAULT;
	}
	__req.region_by_name = (str)region_by_name;
	__req.DFS_flag = (int)DFS_flag;
	__req.bw = (unsigned int)bw;
	if (list_of_channels) {
		list_of_channels[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_LIST_DFS_CHANNELS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_list_DFS_channels_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_list_DFS_channels_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_list_DFS_channels call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (list_of_channels && __resp.list_of_channels)
		strcpy(list_of_channels,
			__resp.list_of_channels);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_regulatory_get_list_DFS_channels(const char * region_by_name, const int DFS_flag, const qcsapi_unsigned_int bw, string_1024 list_of_channels)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_regulatory_get_list_DFS_channels_request __req;
	struct qcsapi_regulatory_get_list_DFS_channels_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (list_of_channels == NULL) {
		return -EFAULT;
	}
	__req.region_by_name = (str)region_by_name;
	__req.DFS_flag = (int)DFS_flag;
	__req.bw = (unsigned int)bw;
	if (list_of_channels) {
		list_of_channels[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_REGULATORY_GET_LIST_DFS_CHANNELS_REMOTE,
				(xdrproc_t)xdr_qcsapi_regulatory_get_list_DFS_channels_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_regulatory_get_list_DFS_channels_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_regulatory_get_list_DFS_channels call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (list_of_channels && __resp.list_of_channels)
		strcpy(list_of_channels,
			__resp.list_of_channels);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_is_channel_DFS(const char * region_by_name, const qcsapi_unsigned_int the_channel, int * p_channel_is_DFS)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_is_channel_DFS_request __req;
	struct qcsapi_wifi_is_channel_DFS_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_channel_is_DFS == NULL) {
		return -EFAULT;
	}
	__req.region_by_name = (str)region_by_name;
	__req.the_channel = (unsigned int)the_channel;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_IS_CHANNEL_DFS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_is_channel_DFS_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_is_channel_DFS_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_is_channel_DFS call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_channel_is_DFS)
		*p_channel_is_DFS = __resp.p_channel_is_DFS;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_regulatory_is_channel_DFS(const char * region_by_name, const qcsapi_unsigned_int the_channel, int * p_channel_is_DFS)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_regulatory_is_channel_DFS_request __req;
	struct qcsapi_regulatory_is_channel_DFS_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_channel_is_DFS == NULL) {
		return -EFAULT;
	}
	__req.region_by_name = (str)region_by_name;
	__req.the_channel = (unsigned int)the_channel;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_REGULATORY_IS_CHANNEL_DFS_REMOTE,
				(xdrproc_t)xdr_qcsapi_regulatory_is_channel_DFS_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_regulatory_is_channel_DFS_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_regulatory_is_channel_DFS call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_channel_is_DFS)
		*p_channel_is_DFS = __resp.p_channel_is_DFS;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_dfs_cce_channels(const char * ifname, qcsapi_unsigned_int * p_prev_channel, qcsapi_unsigned_int * p_cur_channel)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_dfs_cce_channels_request __req;
	struct qcsapi_wifi_get_dfs_cce_channels_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_prev_channel == NULL || p_cur_channel == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_DFS_CCE_CHANNELS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_dfs_cce_channels_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_dfs_cce_channels_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_dfs_cce_channels call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_prev_channel)
		*p_prev_channel = __resp.p_prev_channel;
	if (p_cur_channel)
		*p_cur_channel = __resp.p_cur_channel;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_DFS_alt_channel(const char * ifname, qcsapi_unsigned_int * p_dfs_alt_chan)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_DFS_alt_channel_request __req;
	struct qcsapi_wifi_get_DFS_alt_channel_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_dfs_alt_chan == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_DFS_ALT_CHANNEL_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_DFS_alt_channel_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_DFS_alt_channel_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_DFS_alt_channel call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_dfs_alt_chan)
		*p_dfs_alt_chan = __resp.p_dfs_alt_chan;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_DFS_alt_channel(const char * ifname, const qcsapi_unsigned_int dfs_alt_chan)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_DFS_alt_channel_request __req;
	struct qcsapi_wifi_set_DFS_alt_channel_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.dfs_alt_chan = (unsigned int)dfs_alt_chan;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_DFS_ALT_CHANNEL_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_DFS_alt_channel_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_DFS_alt_channel_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_DFS_alt_channel call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_start_dfs_reentry(const char * ifname)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_start_dfs_reentry_request __req;
	struct qcsapi_wifi_start_dfs_reentry_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_START_DFS_REENTRY_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_start_dfs_reentry_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_start_dfs_reentry_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_start_dfs_reentry call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_start_scan_ext(const char * ifname, const int scanflag)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_start_scan_ext_request __req;
	struct qcsapi_wifi_start_scan_ext_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.scanflag = (int)scanflag;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_START_SCAN_EXT_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_start_scan_ext_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_start_scan_ext_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_start_scan_ext call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_csw_records(const char * ifname, int reset, qcsapi_csw_record * record)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_csw_records_request __req;
	struct qcsapi_wifi_get_csw_records_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (record == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.reset = (int)reset;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_CSW_RECORDS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_csw_records_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_csw_records_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_csw_records call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	record->cnt = __resp.record_cnt;
	record->index = __resp.record_index;
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(record->channel); __i_0++) {
			record->channel[__i_0] = __resp.record_channel[__i_0];
		}
	}
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(record->timestamp); __i_0++) {
			record->timestamp[__i_0] = __resp.record_timestamp[__i_0];
		}
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_radar_status(const char * ifname, qcsapi_radar_status * rdstatus)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_radar_status_request __req;
	struct qcsapi_wifi_get_radar_status_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (rdstatus == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_RADAR_STATUS_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_radar_status_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_radar_status_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_radar_status call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	rdstatus->channel = __resp.rdstatus_channel;
	rdstatus->flags = __resp.rdstatus_flags;
	rdstatus->ic_radardetected = __resp.rdstatus_ic_radardetected;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_results_AP_scan(const char * ifname, qcsapi_unsigned_int * p_count_APs)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_results_AP_scan_request __req;
	struct qcsapi_wifi_get_results_AP_scan_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_count_APs == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_RESULTS_AP_SCAN_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_results_AP_scan_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_results_AP_scan_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_results_AP_scan call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_count_APs)
		*p_count_APs = __resp.p_count_APs;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_count_APs_scanned(const char * ifname, qcsapi_unsigned_int * p_count_APs)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_count_APs_scanned_request __req;
	struct qcsapi_wifi_get_count_APs_scanned_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_count_APs == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_COUNT_APS_SCANNED_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_count_APs_scanned_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_count_APs_scanned_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_count_APs_scanned call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_count_APs)
		*p_count_APs = __resp.p_count_APs;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_properties_AP(const char * ifname, const qcsapi_unsigned_int index_AP, qcsapi_ap_properties * p_ap_properties)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_properties_AP_request __req;
	struct qcsapi_wifi_get_properties_AP_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_ap_properties == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.index_AP = (unsigned int)index_AP;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_PROPERTIES_AP_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_properties_AP_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_properties_AP_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_properties_AP call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_ap_properties->ap_name_SSID && __resp.p_ap_properties_ap_name_SSID)
		strcpy(p_ap_properties->ap_name_SSID,
			__resp.p_ap_properties_ap_name_SSID);
	if (p_ap_properties->ap_mac_addr && __resp.p_ap_properties_ap_mac_addr)
		memcpy(p_ap_properties->ap_mac_addr,
			__resp.p_ap_properties_ap_mac_addr,
			sizeof(__resp.p_ap_properties_ap_mac_addr));
	p_ap_properties->ap_flags = __resp.p_ap_properties_ap_flags;
	p_ap_properties->ap_channel = __resp.p_ap_properties_ap_channel;
	p_ap_properties->ap_RSSI = __resp.p_ap_properties_ap_RSSI;
	p_ap_properties->ap_protocol = __resp.p_ap_properties_ap_protocol;
	p_ap_properties->ap_encryption_modes = __resp.p_ap_properties_ap_encryption_modes;
	p_ap_properties->ap_authentication_mode = __resp.p_ap_properties_ap_authentication_mode;
	p_ap_properties->ap_best_data_rate = __resp.p_ap_properties_ap_best_data_rate;
	p_ap_properties->ap_wps = __resp.p_ap_properties_ap_wps;
	p_ap_properties->ap_80211_proto = __resp.p_ap_properties_ap_80211_proto;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_backoff_fail_max(const char * ifname, const int fail_max)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_backoff_fail_max_request __req;
	struct qcsapi_wifi_backoff_fail_max_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.fail_max = (int)fail_max;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_BACKOFF_FAIL_MAX_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_backoff_fail_max_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_backoff_fail_max_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_backoff_fail_max call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_backoff_timeout(const char * ifname, const int timeout)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_backoff_timeout_request __req;
	struct qcsapi_wifi_backoff_timeout_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.timeout = (int)timeout;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_BACKOFF_TIMEOUT_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_backoff_timeout_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_backoff_timeout_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_backoff_timeout call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_mcs_rate(const char * ifname, qcsapi_mcs_rate current_mcs_rate)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_mcs_rate_request __req;
	struct qcsapi_wifi_get_mcs_rate_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (current_mcs_rate == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (current_mcs_rate) {
		current_mcs_rate[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_MCS_RATE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_mcs_rate_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_mcs_rate_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_mcs_rate call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (current_mcs_rate && __resp.current_mcs_rate)
		strcpy(current_mcs_rate,
			__resp.current_mcs_rate);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_mcs_rate(const char * ifname, const qcsapi_mcs_rate new_mcs_rate)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_mcs_rate_request __req;
	struct qcsapi_wifi_set_mcs_rate_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.new_mcs_rate = (str)new_mcs_rate;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_MCS_RATE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_mcs_rate_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_mcs_rate_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_mcs_rate call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_pairing_id(const char * ifname, const char * pairing_id)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_pairing_id_request __req;
	struct qcsapi_wifi_set_pairing_id_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.pairing_id = (str)pairing_id;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_PAIRING_ID_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_pairing_id_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_pairing_id_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_pairing_id call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_pairing_id(const char * ifname, char * pairing_id)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_pairing_id_request __req;
	struct qcsapi_wifi_get_pairing_id_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (pairing_id == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (pairing_id) {
		pairing_id[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_PAIRING_ID_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_pairing_id_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_pairing_id_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_pairing_id call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (pairing_id && __resp.pairing_id)
		strcpy(pairing_id,
			__resp.pairing_id);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_pairing_enable(const char * ifname, const char * enable)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_pairing_enable_request __req;
	struct qcsapi_wifi_set_pairing_enable_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.enable = (str)enable;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_PAIRING_ENABLE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_pairing_enable_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_pairing_enable_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_pairing_enable call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_pairing_enable(const char * ifname, char * enable)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_pairing_enable_request __req;
	struct qcsapi_wifi_get_pairing_enable_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (enable == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (enable) {
		enable[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_PAIRING_ENABLE_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_pairing_enable_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_pairing_enable_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_pairing_enable call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (enable && __resp.enable)
		strcpy(enable,
			__resp.enable);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_non_wps_set_pp_enable(const char * ifname, uint32_t ctrl_state)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_non_wps_set_pp_enable_request __req;
	struct qcsapi_non_wps_set_pp_enable_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.ctrl_state = (unsigned long)ctrl_state;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_NON_WPS_SET_PP_ENABLE_REMOTE,
				(xdrproc_t)xdr_qcsapi_non_wps_set_pp_enable_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_non_wps_set_pp_enable_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_non_wps_set_pp_enable call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_non_wps_get_pp_enable(const char * ifname, uint32_t * ctrl_state)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_non_wps_get_pp_enable_request __req;
	struct qcsapi_non_wps_get_pp_enable_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (ctrl_state == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_NON_WPS_GET_PP_ENABLE_REMOTE,
				(xdrproc_t)xdr_qcsapi_non_wps_get_pp_enable_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_non_wps_get_pp_enable_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_non_wps_get_pp_enable call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (ctrl_state)
		*ctrl_state = __resp.ctrl_state;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_vendor_fix(const char * ifname, int fix_param, int value)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_vendor_fix_request __req;
	struct qcsapi_wifi_set_vendor_fix_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.fix_param = (int)fix_param;
	__req.value = (int)value;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_VENDOR_FIX_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_vendor_fix_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_vendor_fix_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_vendor_fix call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_errno_get_message(const int qcsapi_retval, char * error_msg, unsigned int msglen)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_errno_get_message_request __req;
	struct qcsapi_errno_get_message_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (error_msg == NULL) {
		return -EFAULT;
	}
	__req.qcsapi_retval = (int)qcsapi_retval;
	if (error_msg) {
		error_msg[0] = 0;
	}
	__req.msglen = (unsigned int)msglen;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_ERRNO_GET_MESSAGE_REMOTE,
				(xdrproc_t)xdr_qcsapi_errno_get_message_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_errno_get_message_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_errno_get_message call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (error_msg && __resp.error_msg)
		strcpy(error_msg,
			__resp.error_msg);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_get_interface_stats(const char * ifname, qcsapi_interface_stats * stats)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_get_interface_stats_request __req;
	struct qcsapi_get_interface_stats_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (stats == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_GET_INTERFACE_STATS_REMOTE,
				(xdrproc_t)xdr_qcsapi_get_interface_stats_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_get_interface_stats_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_get_interface_stats call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	sscanf(__resp.stats_tx_bytes, "%" SCNu64, &stats->tx_bytes);
	stats->tx_pkts = __resp.stats_tx_pkts;
	stats->tx_discard = __resp.stats_tx_discard;
	stats->tx_err = __resp.stats_tx_err;
	stats->tx_unicast = __resp.stats_tx_unicast;
	stats->tx_multicast = __resp.stats_tx_multicast;
	stats->tx_broadcast = __resp.stats_tx_broadcast;
	sscanf(__resp.stats_rx_bytes, "%" SCNu64, &stats->rx_bytes);
	stats->rx_pkts = __resp.stats_rx_pkts;
	stats->rx_discard = __resp.stats_rx_discard;
	stats->rx_err = __resp.stats_rx_err;
	stats->rx_unicast = __resp.stats_rx_unicast;
	stats->rx_multicast = __resp.stats_rx_multicast;
	stats->rx_broadcast = __resp.stats_rx_broadcast;
	stats->rx_unknown = __resp.stats_rx_unknown;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_get_phy_stats(const char * ifname, qcsapi_phy_stats * stats)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_get_phy_stats_request __req;
	struct qcsapi_get_phy_stats_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (stats == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_GET_PHY_STATS_REMOTE,
				(xdrproc_t)xdr_qcsapi_get_phy_stats_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_get_phy_stats_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_get_phy_stats call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	stats->tstamp = __resp.stats_tstamp;
	stats->assoc = __resp.stats_assoc;
	stats->channel = __resp.stats_channel;
	stats->atten = __resp.stats_atten;
	stats->cca_total = __resp.stats_cca_total;
	stats->cca_tx = __resp.stats_cca_tx;
	stats->cca_rx = __resp.stats_cca_rx;
	stats->cca_int = __resp.stats_cca_int;
	stats->cca_idle = __resp.stats_cca_idle;
	stats->rx_pkts = __resp.stats_rx_pkts;
	stats->rx_gain = __resp.stats_rx_gain;
	stats->rx_cnt_crc = __resp.stats_rx_cnt_crc;
	sscanf(__resp.stats_rx_noise, "%f", &stats->rx_noise);
	stats->tx_pkts = __resp.stats_tx_pkts;
	stats->tx_defers = __resp.stats_tx_defers;
	stats->tx_touts = __resp.stats_tx_touts;
	stats->tx_retries = __resp.stats_tx_retries;
	stats->cnt_sp_fail = __resp.stats_cnt_sp_fail;
	stats->cnt_lp_fail = __resp.stats_cnt_lp_fail;
	stats->last_rx_mcs = __resp.stats_last_rx_mcs;
	stats->last_tx_mcs = __resp.stats_last_tx_mcs;
	sscanf(__resp.stats_last_rssi, "%f", &stats->last_rssi);
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(stats->last_rssi_array); __i_0++) {
			sscanf(__resp.stats_last_rssi_array[__i_0], "%f", &stats->last_rssi_array[__i_0]);
		}
	}
	sscanf(__resp.stats_last_rcpi, "%f", &stats->last_rcpi);
	sscanf(__resp.stats_last_evm, "%f", &stats->last_evm);
	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(stats->last_evm_array); __i_0++) {
			sscanf(__resp.stats_last_evm_array[__i_0], "%f", &stats->last_evm_array[__i_0]);
		}
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_reset_all_counters(const char * ifname, const uint32_t node_index, int local_remote_flag)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_reset_all_counters_request __req;
	struct qcsapi_reset_all_counters_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.node_index = (unsigned long)node_index;
	__req.local_remote_flag = (int)local_remote_flag;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_RESET_ALL_COUNTERS_REMOTE,
				(xdrproc_t)xdr_qcsapi_reset_all_counters_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_reset_all_counters_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_reset_all_counters call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_firmware_get_version(char * firmware_version, const qcsapi_unsigned_int version_size)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_firmware_get_version_request __req;
	struct qcsapi_firmware_get_version_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (firmware_version == NULL) {
		return -EFAULT;
	}
	if (firmware_version) {
		firmware_version[0] = 0;
	}
	__req.version_size = (unsigned int)version_size;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_FIRMWARE_GET_VERSION_REMOTE,
				(xdrproc_t)xdr_qcsapi_firmware_get_version_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_firmware_get_version_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_firmware_get_version call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (firmware_version && __resp.firmware_version)
		strcpy(firmware_version,
			__resp.firmware_version);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_flash_image_update(const char * image_file, qcsapi_flash_partiton_type partition_to_upgrade)
{
	int retries = retries_limit;
	static struct timeval timeout = { 60, 0 };
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_flash_image_update_request __req;
	struct qcsapi_flash_image_update_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.image_file = (str)image_file;
	__req.partition_to_upgrade = (int)partition_to_upgrade;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_FLASH_IMAGE_UPDATE_REMOTE,
				(xdrproc_t)xdr_qcsapi_flash_image_update_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_flash_image_update_response, (caddr_t)&__resp,
				timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_flash_image_update call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_set_soc_mac_addr(const char * ifname, qcsapi_mac_addr soc_mac_addr)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_set_soc_mac_addr_request __req;
	struct qcsapi_set_soc_mac_addr_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (soc_mac_addr == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SET_SOC_MAC_ADDR_REMOTE,
				(xdrproc_t)xdr_qcsapi_set_soc_mac_addr_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_set_soc_mac_addr_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_set_soc_mac_addr call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (soc_mac_addr && __resp.soc_mac_addr)
		memcpy(soc_mac_addr,
			__resp.soc_mac_addr,
			sizeof(__resp.soc_mac_addr));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_get_custom_value(const char * custom_key, string_128 custom_value)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_get_custom_value_request __req;
	struct qcsapi_get_custom_value_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (custom_value == NULL) {
		return -EFAULT;
	}
	__req.custom_key = (str)custom_key;
	if (custom_value) {
		custom_value[0] = 0;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_GET_CUSTOM_VALUE_REMOTE,
				(xdrproc_t)xdr_qcsapi_get_custom_value_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_get_custom_value_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_get_custom_value call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (custom_value && __resp.custom_value)
		strcpy(custom_value,
			__resp.custom_value);
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_mlme_stats_per_mac(const char * ifname, const qcsapi_mac_addr client_mac_addr, qcsapi_mlme_stats * stats)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_mlme_stats_per_mac_request __req;
	struct qcsapi_wifi_get_mlme_stats_per_mac_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (client_mac_addr == NULL || stats == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	memcpy(__req.client_mac_addr, client_mac_addr, sizeof(const qcsapi_mac_addr));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_MLME_STATS_PER_MAC_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_mlme_stats_per_mac_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_mlme_stats_per_mac_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_mlme_stats_per_mac call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	stats->auth = __resp.stats_auth;
	stats->auth_fails = __resp.stats_auth_fails;
	stats->assoc = __resp.stats_assoc;
	stats->assoc_fails = __resp.stats_assoc_fails;
	stats->deauth = __resp.stats_deauth;
	stats->diassoc = __resp.stats_diassoc;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_mlme_stats_per_association(const char * ifname, const qcsapi_unsigned_int association_index, qcsapi_mlme_stats * stats)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_mlme_stats_per_association_request __req;
	struct qcsapi_wifi_get_mlme_stats_per_association_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (stats == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.association_index = (unsigned int)association_index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_MLME_STATS_PER_ASSOCIATION_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_mlme_stats_per_association_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_mlme_stats_per_association_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_mlme_stats_per_association call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	stats->auth = __resp.stats_auth;
	stats->auth_fails = __resp.stats_auth_fails;
	stats->assoc = __resp.stats_assoc;
	stats->assoc_fails = __resp.stats_assoc_fails;
	stats->deauth = __resp.stats_deauth;
	stats->diassoc = __resp.stats_diassoc;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_mlme_stats_macs_list(const char * ifname, qcsapi_mlme_stats_macs * macs_list)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_mlme_stats_macs_list_request __req;
	struct qcsapi_wifi_get_mlme_stats_macs_list_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (macs_list == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_MLME_STATS_MACS_LIST_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_mlme_stats_macs_list_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_mlme_stats_macs_list_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_mlme_stats_macs_list call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(macs_list->addr); __i_0++) {
			if (macs_list->addr[__i_0] && __resp.macs_list_addr[__i_0])
		memcpy(macs_list->addr[__i_0],
			__resp.macs_list_addr[__i_0],
			sizeof(__resp.macs_list_addr[__i_0]));
		}
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_pm_set_mode(int mode)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_pm_set_mode_request __req;
	struct qcsapi_pm_set_mode_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.mode = (int)mode;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_PM_SET_MODE_REMOTE,
				(xdrproc_t)xdr_qcsapi_pm_set_mode_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_pm_set_mode_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_pm_set_mode call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_pm_get_mode(int * mode)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_pm_get_mode_request __req;
	struct qcsapi_pm_get_mode_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (mode == NULL) {
		return -EFAULT;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_PM_GET_MODE_REMOTE,
				(xdrproc_t)xdr_qcsapi_pm_get_mode_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_pm_get_mode_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_pm_get_mode call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (mode)
		*mode = __resp.mode;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_vsp_get_state(const char * ifname, unsigned int param, unsigned int * value)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_vsp_get_state_request __req;
	struct qcsapi_vsp_get_state_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (value == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.param = (unsigned int)param;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_VSP_GET_STATE_REMOTE,
				(xdrproc_t)xdr_qcsapi_vsp_get_state_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_vsp_get_state_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_vsp_get_state call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (value)
		*value = __resp.value;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_vsp_get_state_all(const char * ifname, unsigned int * value, unsigned int max)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_vsp_get_state_all_request __req;
	struct qcsapi_vsp_get_state_all_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (value == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.max = (unsigned int)max;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_VSP_GET_STATE_ALL_REMOTE,
				(xdrproc_t)xdr_qcsapi_vsp_get_state_all_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_vsp_get_state_all_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_vsp_get_state_all call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (value)
		*value = __resp.value;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_vsp_set_state(const char * ifname, unsigned int param, unsigned int value)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_vsp_set_state_request __req;
	struct qcsapi_vsp_set_state_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.param = (unsigned int)param;
	__req.value = (unsigned int)value;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_VSP_SET_STATE_REMOTE,
				(xdrproc_t)xdr_qcsapi_vsp_set_state_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_vsp_set_state_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_vsp_set_state call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_vsp_get_config(const char * ifname, unsigned int param, unsigned int * value)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_vsp_get_config_request __req;
	struct qcsapi_vsp_get_config_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (value == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.param = (unsigned int)param;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_VSP_GET_CONFIG_REMOTE,
				(xdrproc_t)xdr_qcsapi_vsp_get_config_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_vsp_get_config_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_vsp_get_config call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (value)
		*value = __resp.value;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_vsp_get_config_all(const char * ifname, unsigned int * value, unsigned int max)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_vsp_get_config_all_request __req;
	struct qcsapi_vsp_get_config_all_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (value == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.max = (unsigned int)max;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_VSP_GET_CONFIG_ALL_REMOTE,
				(xdrproc_t)xdr_qcsapi_vsp_get_config_all_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_vsp_get_config_all_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_vsp_get_config_all call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (value)
		*value = __resp.value;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_vsp_set_config(const char * ifname, unsigned int param, unsigned int value)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_vsp_set_config_request __req;
	struct qcsapi_vsp_set_config_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.param = (unsigned int)param;
	__req.value = (unsigned int)value;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_VSP_SET_CONFIG_REMOTE,
				(xdrproc_t)xdr_qcsapi_vsp_set_config_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_vsp_set_config_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_vsp_set_config call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_vsp_add_wl(const char * ifname, const struct qvsp_wl_flds * entry)
{
	/* stubbed, not implemented */
	fprintf(stderr, "%s not implemented\n", "qcsapi_vsp_add_wl");
	return -qcsapi_programming_error;
}
int qcsapi_vsp_del_wl(const char * ifname, const struct qvsp_wl_flds * entry)
{
	/* stubbed, not implemented */
	fprintf(stderr, "%s not implemented\n", "qcsapi_vsp_del_wl");
	return -qcsapi_programming_error;
}
int qcsapi_vsp_del_wl_index(const char * ifname, unsigned int index)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_vsp_del_wl_index_request __req;
	struct qcsapi_vsp_del_wl_index_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.index = (unsigned int)index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_VSP_DEL_WL_INDEX_REMOTE,
				(xdrproc_t)xdr_qcsapi_vsp_del_wl_index_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_vsp_del_wl_index_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_vsp_del_wl_index call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_vsp_get_wl(const char * ifname, struct qvsp_wl_flds * entries, unsigned int max_entries)
{
	/* stubbed, not implemented */
	fprintf(stderr, "%s not implemented\n", "qcsapi_vsp_get_wl");
	return -qcsapi_programming_error;
}
int qcsapi_vsp_add_rule(const char * ifname, const struct qvsp_rule_flds * entry)
{
	/* stubbed, not implemented */
	fprintf(stderr, "%s not implemented\n", "qcsapi_vsp_add_rule");
	return -qcsapi_programming_error;
}
int qcsapi_vsp_del_rule(const char * ifname, const struct qvsp_rule_flds * entry)
{
	/* stubbed, not implemented */
	fprintf(stderr, "%s not implemented\n", "qcsapi_vsp_del_rule");
	return -qcsapi_programming_error;
}
int qcsapi_vsp_del_rule_index(const char * ifname, unsigned int index)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_vsp_del_rule_index_request __req;
	struct qcsapi_vsp_del_rule_index_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.index = (unsigned int)index;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_VSP_DEL_RULE_INDEX_REMOTE,
				(xdrproc_t)xdr_qcsapi_vsp_del_rule_index_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_vsp_del_rule_index_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_vsp_del_rule_index call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_vsp_get_rule(const char * ifname, struct qvsp_rule_flds * entries, unsigned int max_entries)
{
	/* stubbed, not implemented */
	fprintf(stderr, "%s not implemented\n", "qcsapi_vsp_get_rule");
	return -qcsapi_programming_error;
}
int qcsapi_vsp_get_strm(const char * ifname, struct qvsp_strm_info * strms, unsigned int max_entries, int show_all)
{
	/* stubbed, not implemented */
	fprintf(stderr, "%s not implemented\n", "qcsapi_vsp_get_strm");
	return -qcsapi_programming_error;
}
int qcsapi_vsp_get_stats(const char * ifname, struct qvsp_stats * stats)
{
	/* stubbed, not implemented */
	fprintf(stderr, "%s not implemented\n", "qcsapi_vsp_get_stats");
	return -qcsapi_programming_error;
}
int qcsapi_vsp_get_inactive_flags(const char * ifname, unsigned long * flags)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_vsp_get_inactive_flags_request __req;
	struct qcsapi_vsp_get_inactive_flags_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (flags == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_VSP_GET_INACTIVE_FLAGS_REMOTE,
				(xdrproc_t)xdr_qcsapi_vsp_get_inactive_flags_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_vsp_get_inactive_flags_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_vsp_get_inactive_flags call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (flags)
		*flags = __resp.flags;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_run_script(const char * scriptname, const char * param)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_run_script_request __req;
	struct qcsapi_wifi_run_script_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.scriptname = (str)scriptname;
	__req.param = (str)param;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_RUN_SCRIPT_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_run_script_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_run_script_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_run_script call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_test_traffic(const char * ifname, uint32_t period)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_test_traffic_request __req;
	struct qcsapi_wifi_test_traffic_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.period = (unsigned long)period;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_TEST_TRAFFIC_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_test_traffic_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_test_traffic_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_test_traffic call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_add_ipff(qcsapi_unsigned_int ipaddr)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_add_ipff_request __req;
	struct qcsapi_wifi_add_ipff_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ipaddr = (unsigned int)ipaddr;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_ADD_IPFF_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_add_ipff_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_add_ipff_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_add_ipff call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_del_ipff(qcsapi_unsigned_int ipaddr)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_del_ipff_request __req;
	struct qcsapi_wifi_del_ipff_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ipaddr = (unsigned int)ipaddr;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_DEL_IPFF_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_del_ipff_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_del_ipff_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_del_ipff call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_ipff()
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_ipff_request __req;
	struct qcsapi_wifi_get_ipff_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_IPFF_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_ipff_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_ipff_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_ipff call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_get_temperature_info(int * temp_exter, int * temp_inter)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_get_temperature_info_request __req;
	struct qcsapi_get_temperature_info_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (temp_exter == NULL || temp_inter == NULL) {
		return -EFAULT;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_GET_TEMPERATURE_INFO_REMOTE,
				(xdrproc_t)xdr_qcsapi_get_temperature_info_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_get_temperature_info_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_get_temperature_info call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (temp_exter)
		*temp_exter = __resp.temp_exter;
	if (temp_inter)
		*temp_inter = __resp.temp_inter;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_calcmd_set_test_mode(qcsapi_unsigned_int channel, qcsapi_unsigned_int antenna, qcsapi_unsigned_int mcs, qcsapi_unsigned_int bw, qcsapi_unsigned_int pkt_size, qcsapi_unsigned_int eleven_n, qcsapi_unsigned_int bf)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_calcmd_set_test_mode_request __req;
	struct qcsapi_calcmd_set_test_mode_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.channel = (unsigned int)channel;
	__req.antenna = (unsigned int)antenna;
	__req.mcs = (unsigned int)mcs;
	__req.bw = (unsigned int)bw;
	__req.pkt_size = (unsigned int)pkt_size;
	__req.eleven_n = (unsigned int)eleven_n;
	__req.bf = (unsigned int)bf;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_CALCMD_SET_TEST_MODE_REMOTE,
				(xdrproc_t)xdr_qcsapi_calcmd_set_test_mode_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_calcmd_set_test_mode_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_calcmd_set_test_mode call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_calcmd_show_test_packet(qcsapi_unsigned_int * tx_packet_num, qcsapi_unsigned_int * rx_packet_num, qcsapi_unsigned_int * crc_packet_num)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_calcmd_show_test_packet_request __req;
	struct qcsapi_calcmd_show_test_packet_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (tx_packet_num == NULL || rx_packet_num == NULL || crc_packet_num == NULL) {
		return -EFAULT;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_CALCMD_SHOW_TEST_PACKET_REMOTE,
				(xdrproc_t)xdr_qcsapi_calcmd_show_test_packet_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_calcmd_show_test_packet_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_calcmd_show_test_packet call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (tx_packet_num)
		*tx_packet_num = __resp.tx_packet_num;
	if (rx_packet_num)
		*rx_packet_num = __resp.rx_packet_num;
	if (crc_packet_num)
		*crc_packet_num = __resp.crc_packet_num;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_calcmd_send_test_packet(qcsapi_unsigned_int to_transmit_packet_num)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_calcmd_send_test_packet_request __req;
	struct qcsapi_calcmd_send_test_packet_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.to_transmit_packet_num = (unsigned int)to_transmit_packet_num;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_CALCMD_SEND_TEST_PACKET_REMOTE,
				(xdrproc_t)xdr_qcsapi_calcmd_send_test_packet_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_calcmd_send_test_packet_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_calcmd_send_test_packet call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_calcmd_stop_test_packet()
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_calcmd_stop_test_packet_request __req;
	struct qcsapi_calcmd_stop_test_packet_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_CALCMD_STOP_TEST_PACKET_REMOTE,
				(xdrproc_t)xdr_qcsapi_calcmd_stop_test_packet_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_calcmd_stop_test_packet_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_calcmd_stop_test_packet call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_calcmd_send_dc_cw_signal(qcsapi_unsigned_int channel)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_calcmd_send_dc_cw_signal_request __req;
	struct qcsapi_calcmd_send_dc_cw_signal_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.channel = (unsigned int)channel;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_CALCMD_SEND_DC_CW_SIGNAL_REMOTE,
				(xdrproc_t)xdr_qcsapi_calcmd_send_dc_cw_signal_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_calcmd_send_dc_cw_signal_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_calcmd_send_dc_cw_signal call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_calcmd_stop_dc_cw_signal()
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_calcmd_stop_dc_cw_signal_request __req;
	struct qcsapi_calcmd_stop_dc_cw_signal_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_CALCMD_STOP_DC_CW_SIGNAL_REMOTE,
				(xdrproc_t)xdr_qcsapi_calcmd_stop_dc_cw_signal_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_calcmd_stop_dc_cw_signal_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_calcmd_stop_dc_cw_signal call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_calcmd_get_test_mode_antenna_sel(qcsapi_unsigned_int * antenna_bit_mask)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_calcmd_get_test_mode_antenna_sel_request __req;
	struct qcsapi_calcmd_get_test_mode_antenna_sel_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (antenna_bit_mask == NULL) {
		return -EFAULT;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_CALCMD_GET_TEST_MODE_ANTENNA_SEL_REMOTE,
				(xdrproc_t)xdr_qcsapi_calcmd_get_test_mode_antenna_sel_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_calcmd_get_test_mode_antenna_sel_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_calcmd_get_test_mode_antenna_sel call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (antenna_bit_mask)
		*antenna_bit_mask = __resp.antenna_bit_mask;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_calcmd_get_test_mode_mcs(qcsapi_unsigned_int * test_mode_mcs)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_calcmd_get_test_mode_mcs_request __req;
	struct qcsapi_calcmd_get_test_mode_mcs_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (test_mode_mcs == NULL) {
		return -EFAULT;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_CALCMD_GET_TEST_MODE_MCS_REMOTE,
				(xdrproc_t)xdr_qcsapi_calcmd_get_test_mode_mcs_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_calcmd_get_test_mode_mcs_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_calcmd_get_test_mode_mcs call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (test_mode_mcs)
		*test_mode_mcs = __resp.test_mode_mcs;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_calcmd_get_test_mode_bw(qcsapi_unsigned_int * test_mode_bw)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_calcmd_get_test_mode_bw_request __req;
	struct qcsapi_calcmd_get_test_mode_bw_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (test_mode_bw == NULL) {
		return -EFAULT;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_CALCMD_GET_TEST_MODE_BW_REMOTE,
				(xdrproc_t)xdr_qcsapi_calcmd_get_test_mode_bw_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_calcmd_get_test_mode_bw_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_calcmd_get_test_mode_bw call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (test_mode_bw)
		*test_mode_bw = __resp.test_mode_bw;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_calcmd_get_tx_power(qcsapi_calcmd_tx_power_rsp * tx_power)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_calcmd_get_tx_power_request __req;
	struct qcsapi_calcmd_get_tx_power_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (tx_power == NULL) {
		return -EFAULT;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_CALCMD_GET_TX_POWER_REMOTE,
				(xdrproc_t)xdr_qcsapi_calcmd_get_tx_power_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_calcmd_get_tx_power_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_calcmd_get_tx_power call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(tx_power->value); __i_0++) {
			tx_power->value[__i_0] = __resp.tx_power_value[__i_0];
		}
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_calcmd_set_tx_power(qcsapi_unsigned_int tx_power)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_calcmd_set_tx_power_request __req;
	struct qcsapi_calcmd_set_tx_power_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.tx_power = (unsigned int)tx_power;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_CALCMD_SET_TX_POWER_REMOTE,
				(xdrproc_t)xdr_qcsapi_calcmd_set_tx_power_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_calcmd_set_tx_power_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_calcmd_set_tx_power call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_calcmd_get_test_mode_rssi(qcsapi_calcmd_rssi_rsp * test_mode_rssi)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_calcmd_get_test_mode_rssi_request __req;
	struct qcsapi_calcmd_get_test_mode_rssi_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (test_mode_rssi == NULL) {
		return -EFAULT;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_CALCMD_GET_TEST_MODE_RSSI_REMOTE,
				(xdrproc_t)xdr_qcsapi_calcmd_get_test_mode_rssi_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_calcmd_get_test_mode_rssi_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_calcmd_get_test_mode_rssi call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	{
		int __i_0;
		for (__i_0 = 0; __i_0 < ARRAY_SIZE(test_mode_rssi->value); __i_0++) {
			test_mode_rssi->value[__i_0] = __resp.test_mode_rssi_value[__i_0];
		}
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_calcmd_set_mac_filter(int q_num, int sec_enable, const qcsapi_mac_addr mac_addr)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_calcmd_set_mac_filter_request __req;
	struct qcsapi_calcmd_set_mac_filter_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (mac_addr == NULL) {
		return -EFAULT;
	}
	__req.q_num = (int)q_num;
	__req.sec_enable = (int)sec_enable;
	memcpy(__req.mac_addr, mac_addr, sizeof(const qcsapi_mac_addr));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_CALCMD_SET_MAC_FILTER_REMOTE,
				(xdrproc_t)xdr_qcsapi_calcmd_set_mac_filter_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_calcmd_set_mac_filter_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_calcmd_set_mac_filter call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_calcmd_get_antenna_count(qcsapi_unsigned_int * antenna_count)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_calcmd_get_antenna_count_request __req;
	struct qcsapi_calcmd_get_antenna_count_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (antenna_count == NULL) {
		return -EFAULT;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_CALCMD_GET_ANTENNA_COUNT_REMOTE,
				(xdrproc_t)xdr_qcsapi_calcmd_get_antenna_count_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_calcmd_get_antenna_count_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_calcmd_get_antenna_count call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (antenna_count)
		*antenna_count = __resp.antenna_count;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_calcmd_clear_counter()
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_calcmd_clear_counter_request __req;
	struct qcsapi_calcmd_clear_counter_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_CALCMD_CLEAR_COUNTER_REMOTE,
				(xdrproc_t)xdr_qcsapi_calcmd_clear_counter_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_calcmd_clear_counter_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_calcmd_clear_counter call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_get_carrier_id(qcsapi_unsigned_int * p_carrier_id)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_get_carrier_id_request __req;
	struct qcsapi_get_carrier_id_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_carrier_id == NULL) {
		return -EFAULT;
	}
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_GET_CARRIER_ID_REMOTE,
				(xdrproc_t)xdr_qcsapi_get_carrier_id_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_get_carrier_id_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_get_carrier_id call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_carrier_id)
		*p_carrier_id = __resp.p_carrier_id;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_set_carrier_id(uint32_t carrier_id, uint32_t update_uboot)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_set_carrier_id_request __req;
	struct qcsapi_set_carrier_id_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.carrier_id = (unsigned long)carrier_id;
	__req.update_uboot = (unsigned long)update_uboot;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_SET_CARRIER_ID_REMOTE,
				(xdrproc_t)xdr_qcsapi_set_carrier_id_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_set_carrier_id_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_set_carrier_id call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_vht(const char * ifname, const qcsapi_unsigned_int the_vht)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_vht_request __req;
	struct qcsapi_wifi_set_vht_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.the_vht = (unsigned int)the_vht;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_VHT_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_vht_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_vht_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_vht call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_vht(const char * ifname, qcsapi_unsigned_int * vht)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_vht_request __req;
	struct qcsapi_wifi_get_vht_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (vht == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_VHT_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_vht_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_vht_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_vht call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (vht)
		*vht = __resp.vht;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_spinor_jedecid(const char * ifname, unsigned int * p_jedecid)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_spinor_jedecid_request __req;
	struct qcsapi_wifi_get_spinor_jedecid_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (p_jedecid == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_SPINOR_JEDECID_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_spinor_jedecid_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_spinor_jedecid_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_spinor_jedecid call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (p_jedecid)
		*p_jedecid = __resp.p_jedecid;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_set_nss_cap(const char * ifname, const qcsapi_mimo_type modulation, const qcsapi_unsigned_int nss)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_set_nss_cap_request __req;
	struct qcsapi_wifi_set_nss_cap_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	__req.ifname = (str)ifname;
	__req.modulation = (int)modulation;
	__req.nss = (unsigned int)nss;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_SET_NSS_CAP_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_set_nss_cap_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_set_nss_cap_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_set_nss_cap call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

int qcsapi_wifi_get_nss_cap(const char * ifname, const qcsapi_mimo_type modulation, qcsapi_unsigned_int * nss)
{
	int retries = 0;
	CLIENT *clnt = qcsapi_adapter_get_client();
	enum clnt_stat __rpcret;
	struct qcsapi_wifi_get_nss_cap_request __req;
	struct qcsapi_wifi_get_nss_cap_response __resp;
	memset(&__req, 0, sizeof(__req));
	memset(&__resp, 0, sizeof(__resp));
	if (nss == NULL) {
		return -EFAULT;
	}
	__req.ifname = (str)ifname;
	__req.modulation = (int)modulation;
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }
	client_qcsapi_pre();
	while (1) {
		__rpcret = clnt_call(clnt, QCSAPI_WIFI_GET_NSS_CAP_REMOTE,
				(xdrproc_t)xdr_qcsapi_wifi_get_nss_cap_request, (caddr_t)&__req,
				(xdrproc_t)xdr_qcsapi_wifi_get_nss_cap_response, (caddr_t)&__resp,
				__timeout);
		if (__rpcret == RPC_SUCCESS) {
			client_qcsapi_post(0);
			break;
		} else {
			clnt_perror (clnt, "qcsapi_wifi_get_nss_cap call failed");
			clnt_perrno (__rpcret);
			if (retries >= retries_limit) {
				client_qcsapi_post(1);
				return -ENOLINK;
			}
			retries++;
			client_qcsapi_reconnect();
		}

	}

	if (nss)
		*nss = __resp.nss;
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return __resp.return_code;
}

