#include "qmi-message.h"

static struct qmi_wds_start_network_request wds_sn_req = {
	QMI_INIT(authentication_preference,
	         QMI_WDS_AUTHENTICATION_PAP | QMI_WDS_AUTHENTICATION_CHAP),
};

#define cmd_wds_set_auth_cb no_cb
static enum qmi_cmd_result
cmd_wds_set_auth_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	static const struct {
		const char *name;
		QmiWdsAuthentication auth;
	} modes[] = {
		{ "pap", QMI_WDS_AUTHENTICATION_PAP },
		{ "chap", QMI_WDS_AUTHENTICATION_CHAP },
		{ "both", QMI_WDS_AUTHENTICATION_PAP | QMI_WDS_AUTHENTICATION_CHAP },
		{ "none", QMI_WDS_AUTHENTICATION_NONE },
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(modes); i++) {
		if (strcasecmp(modes[i].name, arg) != 0)
			continue;

		qmi_set(&wds_sn_req, authentication_preference, modes[i].auth);
		return QMI_CMD_DONE;
	}

	blobmsg_add_string(&status, "error", "Invalid auth mode (valid: pap, chap, both, none)");
	return QMI_CMD_EXIT;
}

#define cmd_wds_set_username_cb no_cb
static enum qmi_cmd_result
cmd_wds_set_username_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	qmi_set_ptr(&wds_sn_req, username, arg);
	return QMI_CMD_DONE;
}

#define cmd_wds_set_password_cb no_cb
static enum qmi_cmd_result
cmd_wds_set_password_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	qmi_set_ptr(&wds_sn_req, password, arg);
	return QMI_CMD_DONE;
}

#define cmd_wds_set_autoconnect_cb no_cb
static enum qmi_cmd_result
cmd_wds_set_autoconnect_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	qmi_set_ptr(&wds_sn_req, enable_autoconnect, true);
	return QMI_CMD_DONE;
}

static void
cmd_wds_start_network_cb(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg)
{
	struct qmi_wds_start_network_response res;

	qmi_parse_wds_start_network_response(msg, &res);
	if (res.set.packet_data_handle)
		blobmsg_add_u32(&status, "handle", res.data.packet_data_handle);
}

static enum qmi_cmd_result
cmd_wds_start_network_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	qmi_set_ptr(&wds_sn_req, apn, arg);
	qmi_set_wds_start_network_request(msg, &wds_sn_req);
	return QMI_CMD_REQUEST;
}

static void
cmd_wds_get_packet_service_status_cb(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg)
{
	struct qmi_wds_get_packet_service_status_response res;
	const char *data_status[] = {
		[QMI_WDS_CONNECTION_STATUS_UNKNOWN] = "unknown",
		[QMI_WDS_CONNECTION_STATUS_DISCONNECTED] = "disconnected",
		[QMI_WDS_CONNECTION_STATUS_CONNECTED] = "connected",
		[QMI_WDS_CONNECTION_STATUS_SUSPENDED] = "suspended",
		[QMI_WDS_CONNECTION_STATUS_AUTHENTICATING] = "authenticating",
	};
	int s = 0;

	qmi_parse_wds_get_packet_service_status_response(msg, &res);
	if (res.set.connection_status &&
	    res.data.connection_status >= 0 &&
	    res.data.connection_status < ARRAY_SIZE(data_status))
		s = res.data.connection_status;

	blobmsg_add_string(&status, NULL, data_status[s]);
}

static enum qmi_cmd_result
cmd_wds_get_packet_service_status_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	qmi_set_wds_get_packet_service_status_request(msg);
	return QMI_CMD_REQUEST;
}

#define cmd_wds_reset_cb no_cb
static enum qmi_cmd_result
cmd_wds_reset_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	qmi_set_wds_reset_request(msg);
	return QMI_CMD_REQUEST;
}
