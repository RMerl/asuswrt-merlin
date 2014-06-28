#define __uqmi_dms_commands \
	__uqmi_command(dms_get_pin_status, get-pin-status, no, QMI_SERVICE_DMS), \
	__uqmi_command(dms_verify_pin1, verify-pin1, required, QMI_SERVICE_DMS), \
	__uqmi_command(dms_verify_pin2, verify-pin2, required, QMI_SERVICE_DMS), \
	__uqmi_command(dms_get_imsi, get-imsi, no, QMI_SERVICE_DMS), \
	__uqmi_command(dms_set_operating_mode, set-device-operating-mode, required, QMI_SERVICE_DMS), \
	__uqmi_command(dms_reset, reset-dms, no, QMI_SERVICE_DMS) \

#define dms_helptext \
		"  --get-pin-status:                 Get PIN verification status\n" \
		"  --verify-pin1 <pin>:              Verify PIN1\n" \
		"  --verify-pin2 <pin>:              Verify PIN2\n" \
		"  --get-imsi:                       Get International Mobile Subscriber ID\n" \
		"  --reset-dms:                      Reset the DMS service\n" \
		"  --set-device-operating-mode <m>   Set the device operating mode\n" \
		"                                    (modes: online, low_power, factory_test, offline\n" \
		"                                     reset, shutting_down, persistent_low_power,\n" \
		"                                     mode_only_low_power)\n" \

