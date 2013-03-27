# SIP - Session Initiation Protocol - Internet telephony - RFC 3261
# Pattern attributes: ok fast fast
# Protocol groups: voip ietf_proposed_standard
# Wiki: http://www.protocolinfo.org/wiki/SIP
#
# This pattern has been tested with the Ubiquity SIP user agent.
#
# Thanks to Ankit Desai for this pattern.
#
# SIP typically uses port 5060.
#
# This pattern is based on SIP request format as per RFC 3261. I'm not
# sure about the version part. The RFC doesn't say anything about it, so
# I have allowed version ranging from 0.x to 2.x.

#Request-Line  =  Method SP Request-URI SP SIP-Version CRLF
sip
^(invite|register|cancel) sip[\x09-\x0d -~]*sip/[0-2]\.[0-9]
