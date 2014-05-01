#define __uqmi_wms_commands \
	__uqmi_command(wms_list_messages, list-messages, no, QMI_SERVICE_WMS), \
	__uqmi_command(wms_get_message, get-message, required, QMI_SERVICE_WMS), \
	__uqmi_command(wms_get_raw_message, get-raw-message, required, QMI_SERVICE_WMS), \
	__uqmi_command(wms_send_message_smsc, send-message-smsc, required, CMD_TYPE_OPTION), \
	__uqmi_command(wms_send_message_target, send-message-target, required, CMD_TYPE_OPTION), \
	__uqmi_command(wms_send_message_flash, send-message-flash, no, CMD_TYPE_OPTION), \
	__uqmi_command(wms_send_message, send-message, required, QMI_SERVICE_WMS)

#define wms_helptext \
		"  --list-messages:                  List SMS messages\n" \
		"  --get-message <id>:               Get SMS message at index <id>\n" \
		"  --get-raw-message <id>:           Get SMS raw message contents at index <id>\n" \
		"  --send-message <data>:            Send SMS message (use options below)\n" \
		"    --send-message-smsc <nr>:       SMSC number (required)\n" \
		"    --send-message-target <nr>:     Destination number (required)\n" \
		"    --send-message-flash:           Send as Flash SMS\n" \

