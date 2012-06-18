# Soribada - A Korean P2P filesharing program/protocol - http://www.soribada.com
# Pattern attributes: good slow notsofast
# Protocol groups: p2p
# Wiki: http://www.protocolinfo.org/wiki/Soribada
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE

# I am told that there are three versions of this protocol, the first no
# longer being used.  That would probably explain why incoming searches
# have two different formats...

# There are three parts to Soribada protocal:
# 1: Ping/Pong to establish a relationship on the net (UDP with 2 useful bytes)
# 2: Searching (in two formats) (UDP with two short easy to match starts)
# 3: Download requests/transfers (TCP with an obvious first packet)

# 1 -- Pings/Pongs:
# Requester send 2 bytes and a 6 byte response is sent back.
# \x10 for the first byte and \x14-\x16 for the second.
# The response is the first byte (\x10) and the second byte incremented
# by 1 (\x15-\x17).
# No further communication happens between the hosts except for searches.
# A regex match: ^\x10[\x14-\x16]\x10[\x15-\x17].?.?.?.?$
# First Packet ---^^^^^^^^^^^^^^^
# Second Packet -----------------^^^^^^^^^^^^^^^^^^^^^^^

# 2 -- Search requests:
# All searches are totally stateless and are only responded to if the user
# actually has the file.
# Both format start with a \x01 byte, have 3 "random bytes" and then 3 bytes
# corasponding to one of two formats.
# Format 1 is \x51\x3a\+ and format 2 is \x51\x32\x3a
# A regex match: ^\x01.?.?.?(\x51\x3a\+|\x51\x32\x3a)

# 3 -- Download requests:
# All downloads start with "GETMP3\x0d\x0aFilename"
# A regex match: ^GETMP3\x0d\x0aFilename

soribada

# This will match the second packet of two.
# ^\x10[\x14-\x16]\x10[\x15-\x17].?.?.?.?$

# Again, matching this is the end of the comunication.
# ^\x01.?.?.?(\x51\x3a\+|\x51\x32\x3a)

# This is the start of the transfer and an easy match
#^GETMP3\x0d\x0aFilename

# This will match everything including the udp packet portions
^GETMP3\x0d\x0aFilename|^\x01.?.?.?(\x51\x3a\+|\x51\x32\x3a)|^\x10[\x14-\x16]\x10[\x15-\x17].?.?.?.?$

