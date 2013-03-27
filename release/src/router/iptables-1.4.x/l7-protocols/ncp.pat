# NCP - Novell Core Protocol
# Pattern attributes: good veryfast fast
# Protocol groups: networking proprietary
# Wiki: http://www.protocolinfo.org/wiki/NCP
#
# This pattern has been tested and is believed to work well.

# ncp request
# dmdt means Request
#  *any length
#
#  *any reply buffer size
# "" means service request
#  | \x17\x17 means create a service connection
#  | uu means destroy service connection

# ncp reply
# tncp means reply
# 33 means service reply

ncp
^(dmdt.*\x01.*(""|\x11\x11|uu)|tncp.*33)
