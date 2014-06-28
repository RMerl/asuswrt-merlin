#include "qmi-message.h"

static const char *get_pin_status(QmiDmsUimPinStatus status)
{
	static const char *pin_status[] = {
		[QMI_DMS_UIM_PIN_STATUS_NOT_INITIALIZED] = "not_initialized",
		[QMI_DMS_UIM_PIN_STATUS_ENABLED_NOT_VERIFIED] = "not_verified",
		[QMI_DMS_UIM_PIN_STATUS_ENABLED_VERIFIED] = "verified",
		[QMI_DMS_UIM_PIN_STATUS_DISABLED] = "disabled",
		[QMI_DMS_UIM_PIN_STATUS_BLOCKED] = "blocked",
		[QMI_DMS_UIM_PIN_STATUS_PERMANENTLY_BLOCKED] = "permanently_blocked",
		[QMI_DMS_UIM_PIN_STATUS_UNBLOCKED] = "unblocked",
		[QMI_DMS_UIM_PIN_STATUS_CHANGED] = "changed",
	};
	const char *res = "Unknown";

	if (status < ARRAY_SIZE(pin_status) && pin_status[status])
		res = pin_status[status];

	return res;
}

static void cmd_dms_get_pin_status_cb(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg)
{
	struct qmi_dms_uim_get_pin_status_response res;

	qmi_parse_dms_uim_get_pin_status_response(msg, &res);
	if (res.set.pin1_status) {
		blobmsg_add_string(&status, "pin1_status", get_pin_status(res.data.pin1_status.current_status));
		blobmsg_add_u32(&status, "pin1_verify_tries", (int32_t) res.data.pin1_status.verify_retries_left);
		blobmsg_add_u32(&status, "pin1_unblock_tries", (int32_t) res.data.pin1_status.unblock_retries_left);
	}
	if (res.set.pin2_status) {
		blobmsg_add_string(&status, "pin2_status", get_pin_status(res.data.pin2_status.current_status));
		blobmsg_add_u32(&status, "pin2_verify_tries", (int32_t) res.data.pin2_status.verify_retries_left);
		blobmsg_add_u32(&status, "pin2_unblock_tries", (int32_t) res.data.pin2_status.unblock_retries_left);
	}
}

static enum qmi_cmd_result
cmd_dms_get_pin_status_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	qmi_set_dms_uim_get_pin_status_request(msg);
	return QMI_CMD_REQUEST;
}

#define cmd_dms_verify_pin1_cb no_cb
static enum qmi_cmd_result
cmd_dms_verify_pin1_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	struct qmi_dms_uim_verify_pin_request data = {
		QMI_INIT_SEQUENCE(info,
			.pin_id = QMI_DMS_UIM_PIN_ID_PIN,
			.pin = arg
		)
	};
	qmi_set_dms_uim_verify_pin_request(msg, &data);
	return QMI_CMD_REQUEST;
}

#define cmd_dms_verify_pin2_cb no_cb
static enum qmi_cmd_result
cmd_dms_verify_pin2_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	struct qmi_dms_uim_verify_pin_request data = {
		QMI_INIT_SEQUENCE(info,
			.pin_id = QMI_DMS_UIM_PIN_ID_PIN2,
			.pin = arg
		)
	};
	qmi_set_dms_uim_verify_pin_request(msg, &data);
	return QMI_CMD_REQUEST;
}

static void cmd_dms_get_imsi_cb(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg)
{
	struct qmi_dms_uim_get_imsi_response res;

	qmi_parse_dms_uim_get_imsi_response(msg, &res);
	if (res.data.imsi)
		blobmsg_add_string(&status, "number", res.data.imsi);
}

static enum qmi_cmd_result
cmd_dms_get_imsi_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	qmi_set_dms_uim_get_imsi_request(msg);
	return QMI_CMD_REQUEST;
}

#define cmd_dms_reset_cb no_cb
static enum qmi_cmd_result
cmd_dms_reset_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	qmi_set_dms_reset_request(msg);
	return QMI_CMD_REQUEST;
}

#define cmd_dms_set_operating_mode_cb no_cb
static enum qmi_cmd_result
cmd_dms_set_operating_mode_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	static const char *modes[] = {
		[QMI_DMS_OPERATING_MODE_ONLINE] = "online",
		[QMI_DMS_OPERATING_MODE_LOW_POWER] = "low_power",
		[QMI_DMS_OPERATING_MODE_FACTORY_TEST] = "factory_test",
		[QMI_DMS_OPERATING_MODE_OFFLINE] = "offline",
		[QMI_DMS_OPERATING_MODE_RESET] = "reset",
		[QMI_DMS_OPERATING_MODE_SHUTTING_DOWN] = "shutting_down",
		[QMI_DMS_OPERATING_MODE_PERSISTENT_LOW_POWER] = "persistent_low_power",
		[QMI_DMS_OPERATING_MODE_MODE_ONLY_LOW_POWER] = "mode_only_low_power",
	};
	static struct qmi_dms_set_operating_mode_request sreq = {
		QMI_INIT(mode, QMI_DMS_OPERATING_MODE_ONLINE),
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(modes); i++) {
		if (!modes[i])
			continue;

		if (strcmp(arg, modes[i]) != 0)
			continue;

		sreq.data.mode = i;
		qmi_set_dms_set_operating_mode_request(msg, &sreq);
		return QMI_CMD_REQUEST;
	}

	blobmsg_add_string(&status, "error", "Invalid argument");

	return QMI_CMD_EXIT;
}
