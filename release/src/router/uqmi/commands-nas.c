#include "qmi-message.h"

static struct qmi_nas_set_system_selection_preference_request sel_req;

#define cmd_nas_do_set_system_selection_cb no_cb
static enum qmi_cmd_result
cmd_nas_do_set_system_selection_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	qmi_set_nas_set_system_selection_preference_request(msg, &sel_req);
	return QMI_CMD_REQUEST;
}

static enum qmi_cmd_result
do_sel_network(void)
{
	static bool use_sel_req = false;

	if (!use_sel_req) {
		use_sel_req = true;
		uqmi_add_command(NULL, __UQMI_COMMAND_nas_do_set_system_selection);
	}

	return QMI_CMD_DONE;
}

#define cmd_nas_set_network_modes_cb no_cb
static enum qmi_cmd_result
cmd_nas_set_network_modes_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	static const struct {
		const char *name;
		QmiNasRatModePreference val;
	} modes[] = {
		{ "cdma", QMI_NAS_RAT_MODE_PREFERENCE_CDMA_1X | QMI_NAS_RAT_MODE_PREFERENCE_CDMA_1XEVDO },
		{ "td-scdma", QMI_NAS_RAT_MODE_PREFERENCE_TD_SCDMA },
		{ "gsm", QMI_NAS_RAT_MODE_PREFERENCE_GSM },
		{ "umts", QMI_NAS_RAT_MODE_PREFERENCE_UMTS },
		{ "lte", QMI_NAS_RAT_MODE_PREFERENCE_LTE },
	};
	QmiNasRatModePreference val = 0;
	char *word;
	int i;

	for (word = strtok(arg, ",");
	     word;
	     word = strtok(NULL, ",")) {
		bool found = false;

		for (i = 0; i < ARRAY_SIZE(modes); i++) {
			if (strcmp(word, modes[i].name) != 0 &&
				strcmp(word, "all") != 0)
				continue;

			val |= modes[i].val;
			found = true;
		}

		if (!found) {
			blobmsg_add_string(&status, "error", "Invalid network mode");
			return QMI_CMD_EXIT;
		}
	}

	qmi_set(&sel_req, mode_preference, val);
	return do_sel_network();
}

#define cmd_nas_set_network_preference_cb no_cb
static enum qmi_cmd_result
cmd_nas_set_network_preference_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	QmiNasGsmWcdmaAcquisitionOrderPreference pref = QMI_NAS_GSM_WCDMA_ACQUISITION_ORDER_PREFERENCE_AUTOMATIC;

	if (!strcmp(arg, "gsm"))
		pref = QMI_NAS_GSM_WCDMA_ACQUISITION_ORDER_PREFERENCE_GSM;
	else if (!strcmp(arg, "wcdma"))
		pref = QMI_NAS_GSM_WCDMA_ACQUISITION_ORDER_PREFERENCE_WCDMA;

	qmi_set(&sel_req, gsm_wcdma_acquisition_order_preference, pref);
	return do_sel_network();
}

#define cmd_nas_set_roaming_cb no_cb
static enum qmi_cmd_result
cmd_nas_set_roaming_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	QmiNasRoamingPreference pref;

	if (!strcmp(arg, "any"))
		pref = QMI_NAS_ROAMING_PREFERENCE_ANY;
	else if (!strcmp(arg, "only"))
		pref = QMI_NAS_ROAMING_PREFERENCE_NOT_OFF;
	else if (!strcmp(arg, "off"))
		pref = QMI_NAS_ROAMING_PREFERENCE_OFF;
	else {
		blobmsg_add_string(&status, "error", "Invalid argument");
		return QMI_CMD_EXIT;
	}

	qmi_set(&sel_req, roaming_preference, pref);
	return do_sel_network();
}

#define cmd_nas_initiate_network_register_cb no_cb
static enum qmi_cmd_result
cmd_nas_initiate_network_register_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	static struct qmi_nas_initiate_network_register_request register_req;
	qmi_set_nas_initiate_network_register_request(msg, &register_req);
	return QMI_CMD_REQUEST;
}

static void
cmd_nas_get_signal_info_cb(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg)
{
	struct qmi_nas_get_signal_info_response res;

	qmi_parse_nas_get_signal_info_response(msg, &res);

	if (res.set.cdma_signal_strength) {
		blobmsg_add_string(&status, "type", "cdma");
		blobmsg_add_u32(&status, "rssi", (int32_t) res.data.cdma_signal_strength.rssi);
		blobmsg_add_u32(&status, "ecio", (int32_t) res.data.cdma_signal_strength.ecio);
	}

	if (res.set.hdr_signal_strength) {
		blobmsg_add_string(&status, "type", "hdr");
		blobmsg_add_u32(&status, "rssi", (int32_t) res.data.hdr_signal_strength.rssi);
		blobmsg_add_u32(&status, "ecio", (int32_t) res.data.hdr_signal_strength.ecio);
		blobmsg_add_u32(&status, "io", res.data.hdr_signal_strength.io);
	}

	if (res.set.gsm_signal_strength) {
		blobmsg_add_string(&status, "type", "gsm");
		blobmsg_add_u32(&status, "signal", (int32_t) res.data.gsm_signal_strength);
	}

	if (res.set.wcdma_signal_strength) {
		blobmsg_add_string(&status, "type", "wcdma");
		blobmsg_add_u32(&status, "rssi", (int32_t) res.data.wcdma_signal_strength.rssi);
		blobmsg_add_u32(&status, "ecio", (int32_t) res.data.wcdma_signal_strength.ecio);
	}

	if (res.set.lte_signal_strength) {
		blobmsg_add_string(&status, "type", "lte");
		blobmsg_add_u32(&status, "rssi", (int32_t) res.data.lte_signal_strength.rssi);
		blobmsg_add_u32(&status, "rsrq", (int32_t) res.data.lte_signal_strength.rsrq);
		blobmsg_add_u32(&status, "rsrp", (int32_t) res.data.lte_signal_strength.rsrp);
		blobmsg_add_u32(&status, "snr", (int32_t) res.data.lte_signal_strength.snr);
	}
}

static enum qmi_cmd_result
cmd_nas_get_signal_info_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	qmi_set_nas_get_signal_info_request(msg);
	return QMI_CMD_REQUEST;
}

static void
cmd_nas_get_serving_system_cb(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg)
{
	struct qmi_nas_get_serving_system_response res;
	static const char *reg_states[] = {
		[QMI_NAS_REGISTRATION_STATE_NOT_REGISTERED] = "not_registered",
		[QMI_NAS_REGISTRATION_STATE_REGISTERED] = "registered",
		[QMI_NAS_REGISTRATION_STATE_NOT_REGISTERED_SEARCHING] = "searching",
		[QMI_NAS_REGISTRATION_STATE_REGISTRATION_DENIED] = "registering_denied",
		[QMI_NAS_REGISTRATION_STATE_UNKNOWN] = "unknown",
	};

	qmi_parse_nas_get_serving_system_response(msg, &res);

	if (res.set.serving_system) {
		int state = res.data.serving_system.registration_state;

		if (state > QMI_NAS_REGISTRATION_STATE_UNKNOWN)
			state = QMI_NAS_REGISTRATION_STATE_UNKNOWN;

		blobmsg_add_string(&status, "registration", reg_states[state]);
	}
	if (res.set.current_plmn) {
		blobmsg_add_u32(&status, "plmn_mcc", res.data.current_plmn.mcc);
		blobmsg_add_u32(&status, "plmn_mnc", res.data.current_plmn.mnc);
		if (res.data.current_plmn.description)
			blobmsg_add_string(&status, "plmn_description", res.data.current_plmn.description);
	}

	if (res.set.roaming_indicator)
		blobmsg_add_u8(&status, "roaming", !res.data.roaming_indicator);
}

static enum qmi_cmd_result
cmd_nas_get_serving_system_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	qmi_set_nas_get_serving_system_request(msg);
	return QMI_CMD_REQUEST;
}

