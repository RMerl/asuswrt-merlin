# $Id: inc_const.py 2052 2008-06-25 18:18:32Z nanang $
# Useful constants


##########################
# MENU OUTPUT
#


##########################
# EVENTS
#

# Text to expect when there is incoming call
EVENT_INCOMING_CALL = "Press .* answer"


##########################
# CALL STATES
#

# Call state is CALLING
STATE_CALLING = "state.*CALLING"
# Call state is CONFIRMED
STATE_CONFIRMED = "state.*CONFIRMED"
# Call state is DISCONNECTED
STATE_DISCONNECTED = "Call .* DISCONNECTED"

# Media call is put on-hold
MEDIA_HOLD = "Media for call [0-9]+ is suspended.*hold"
# Media call is active
MEDIA_ACTIVE = "Media for call [0-9]+ is active"
# RX_DTMF
RX_DTMF = "Incoming DTMF on call [0-9]+: "

##########################
# MEDIA
#

# Connecting/disconnecting ports
MEDIA_CONN_PORT_SUCCESS = "Port \d+ \(.+\) transmitting to port"
MEDIA_DISCONN_PORT_SUCCESS = "Port \d+ \(.+\) stop transmitting to port"

# Filename to play / record
MEDIA_PLAY_FILE = "--play-file\s+(\S+)"
MEDIA_REC_FILE = "--rec-file\s+(\S+)"

##########################
# MISC
#

# The command prompt
PROMPT = ">>>"
# When pjsua has been destroyed
DESTROYED = "PJSUA destroyed"
# Assertion failure
ASSERT = "Assertion failed"
# Stdout refresh text
STDOUT_REFRESH = "XXSTDOUT_REFRESHXX"


