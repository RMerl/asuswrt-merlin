# Soulseek - P2P filesharing - http://slsknet.org
# Pattern attributes: good fast fast
# Protocol groups: p2p
# Wiki: http://www.protocolinfo.org/wiki/Soulseek
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# All my tests show that this pattern is fast, but one user has reported that
# it is slow.  Your milage may vary.

# This has been tested and works for "pierce firewall" commands and file
# transfers.  It does *not* match all the various sorts of chatter that go on, 
# such as searches, pings and whatnot.

soulseek
# (Pierce firewall: in theory the token could be 4 bytes, but the last two 
# seem to always be zero.|download: Peer Init)
^(\x05..?|.\x01.[ -~]+\x01F..?.?.?.?.?.?.?)$
