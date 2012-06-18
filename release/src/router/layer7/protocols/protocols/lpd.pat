# LPD - Line Printer Daemon Protocol (old-style UNIX printing) - RFC 1179
# Pattern attributes: ok fast fast
# Protocol groups: printer ietf_rfc_documented
# Wiki: http://www.protocolinfo.org/wiki/LPD
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# This pattern is untested.

lpd
# print waiting jobs:  ^\x01[!-~]+\x0a$
# receive a print job: ^\x02[!-~]+\x0a.[\x01\x02\x03][\x01-\x0a -~]*\x0a$
# Send queue state:    ^[\x03\x04][!-~]+[\x09-\x0d]+[a-z][\x09-\x0d -~]*\x0a$
# Remove jobs:         ^\x05[!-~]+[\x09-\x0d]+([a-z][!-~]*[\x09-\x0d]+[1-9][0-9]?[0-9]?|root[\x09-\x0d]+[!-~]+).*\x0a$

# This pattern looks like it might match random data once in a while, but 
# testing shows that this is not the case.

^(\x01[!-~]+|\x02[!-~]+\x0a.[\x01\x02\x03][\x01-\x0a -~]*|[\x03\x04][!-~]+[\x09-\x0d]+[a-z][\x09-\x0d -~]*|\x05[!-~]+[\x09-\x0d]+([a-z][!-~]*[\x09-\x0d]+[1-9][0-9]?[0-9]?|root[\x09-\x0d]+[!-~]+).*)\x0a$
