# Samba/SMB - Server Message Block - Microsoft Windows filesharing
# Pattern attributes: good fast notsofast
# Protocol groups: document_retrieval networking proprietary
# Wiki: http://www.protocolinfo.org/wiki/SMB
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# "This protocol is sometimes also referred to as the Common Internet File 
# System (CIFS), LanManager or NetBIOS protocol." -- "man samba"
#
# Actually, SMB is a higher level protocol than NetBIOS.  However, the 
# NetBIOS header is only 4 bytes: not much to match on.
#
# http://www.ubiqx.org/cifs/SMB.html
#
# This pattern is lightly tested.

smb
# matches a NEGOTIATE PROTOCOL or TRANSACTION REQUEST command
\xffsmb[\x72\x25]
