# SIP - Session Initiation Protocol - Internet telephony - RFC 3261, 3265, etc.
# Pattern attributes: good fast fast
# Protocol groups: voip ietf_proposed_standard
# Wiki: http://www.protocolinfo.org/wiki/SIP
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# This pattern has been tested with the Ubiquity SIP user agent and has been
# confirmed by at least one other user.
# 
# Thanks to Ankit Desai for this pattern.  Updated by tehseen sagar.
#
# SIP typically uses port 5060.
#
# This pattern is based on SIP request format as per RFC 3261. I'm not
# sure about the version part. The RFC doesn't say anything about it, so
# I have allowed version ranging from 0.x to 2.x.

#Request-Line  =  Method SP Request-URI SP SIP-Version CRLF
sip
^(invite|register|cancel|message|subscribe|notify) sip[\x09-\x0d -~]*sip/[0-2]\.[0-9]
