#ifndef __UQMI_COMMANDS_H
#define __UQMI_COMMANDS_H

#include <stdbool.h>
#include "commands-wds.h"
#include "commands-dms.h"
#include "commands-nas.h"
#include "commands-wms.h"

enum qmi_cmd_result {
	QMI_CMD_DONE,
	QMI_CMD_REQUEST,
	QMI_CMD_EXIT,
};

enum {
	CMD_TYPE_OPTION = -1,
};

struct uqmi_cmd_handler {
	const char *name;
	int type;

	enum qmi_cmd_result (*prepare)(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg);
	void (*cb)(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg);
};

struct uqmi_cmd {
	const struct uqmi_cmd_handler *handler;
	char *arg;
};

#define __uqmi_commands \
	__uqmi_command(version, get-versions, no, QMI_SERVICE_CTL), \
	__uqmi_command(set_client_id, set-client-id, required, CMD_TYPE_OPTION), \
	__uqmi_command(get_client_id, get-client-id, required, QMI_SERVICE_CTL), \
	__uqmi_wds_commands, \
	__uqmi_dms_commands, \
	__uqmi_nas_commands, \
	__uqmi_wms_commands

#define __uqmi_command(_name, _optname, _arg, _option) __UQMI_COMMAND_##_name
enum uqmi_command {
	__uqmi_commands,
	__UQMI_COMMAND_LAST
};
#undef __uqmi_command

extern const struct uqmi_cmd_handler uqmi_cmd_handler[];
void uqmi_add_command(char *arg, int longidx);
bool uqmi_run_commands(struct qmi_dev *qmi);

#endif
