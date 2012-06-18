# NTP_SIGND server subsystem

#######################
# Start SUBSYSTEM NTP_signd
[MODULE::NTP_SIGND]
INIT_FUNCTION = server_service_ntp_signd_init
SUBSYSTEM = service
PRIVATE_DEPENDENCIES = \
		SAMDB NDR_NTP_SIGND
# End SUBSYSTEM NTP_SIGND
#######################

NTP_SIGND_OBJ_FILES = $(addprefix $(ntp_signdsrcdir)/, ntp_signd.o)

