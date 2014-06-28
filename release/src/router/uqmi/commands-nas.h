#define __uqmi_nas_commands \
	__uqmi_command(nas_do_set_system_selection, __set-system-selection, no, QMI_SERVICE_NAS), \
	__uqmi_command(nas_set_network_modes, set-network-modes, required, CMD_TYPE_OPTION), \
	__uqmi_command(nas_initiate_network_register, network-register, no, QMI_SERVICE_NAS), \
	__uqmi_command(nas_get_signal_info, get-signal-info, no, QMI_SERVICE_NAS), \
	__uqmi_command(nas_get_serving_system, get-serving-system, no, QMI_SERVICE_NAS), \
	__uqmi_command(nas_set_network_preference, set-network-preference, required, CMD_TYPE_OPTION), \
	__uqmi_command(nas_set_roaming, set-network-roaming, required, CMD_TYPE_OPTION) \

#define nas_helptext \
		"  --set-network-modes <modes>:      Set usable network modes (Syntax: <mode1>[,<mode2>,...])\n" \
		"                                    Available modes: all, lte, umts, gsm, cdma, td-scdma\n" \
		"  --set-network-preference <mode>:  Set preferred network mode to <mode>\n" \
		"                                    Available modes: auto, gsm, wcdma\n" \
		"  --set-network-roaming <mode>:     Set roaming preference:\n" \
		"                                    Available modes: any, off, only\n" \
		"  --network-register:               Initiate network register\n" \
		"  --get-signal-info:                Get signal strength info\n" \
		"  --get-serving-system:             Get serving system info\n" \

