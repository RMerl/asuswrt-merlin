# NetBIOS - Network Basic Input Output System
# Pattern attributes: marginal notsofast notsofast
# Protocol groups: networking ietf_internet_standard proprietary
# Wiki: http://www.protocolinfo.org/wiki/NetBIOS
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# As mentioned in smb.pat:
#
# "This protocol is sometimes also referred to as the Common Internet File 
# System (CIFS), LanManager or NetBIOS protocol." -- "man samba"
#
# Actually, SMB is a higher level protocol than NetBIOS.  However, the 
# NetBIOS header is only 4 bytes: not much to match on.
#
# http://www.ubiqx.org/cifs/SMB.html
# See also RFCs 1001 and 1002.
#
# This pattern attempts to match the (Session layer) NetBIOS Session request. 
# If sucessful, you may be able to match NetBIOS several packets earlier 
# than if you just waited for the easier-to-match SMB header.
#
# This pattern is untested.

netbios
# session request byte, three bytes of flags and length.  Then 
# there should be a big mess of letters between A and P which represent
# the NetBIOS names of the involved computers (with a null between them).  
# (40ish here, damn this regexp implementation and its lack of {40,})
\x81.?.?.[A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P][A-P]
