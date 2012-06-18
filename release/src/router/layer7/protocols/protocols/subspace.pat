# Subspace - 2D asteroids-style space game - http://sscentral.com
# Pattern attributes: marginal veryfast fast
# Protocol groups: game
# Wiki: http://www.protocolinfo.org/wiki/Subspace
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# By Myles Uyema <mylesuyema AT gmail.com>
#
# This pattern matches the initial 2 packets of the client-server
# 'handshake' when joining a Zone.
#
# The first packet is an 8 byte UDP payload sent from client
# 0x00 0x01 0x?? 0x?? 0x?? 0x?? 0x11
# The next packet is a 12 byte UDP response from server
# 0x00 0x10 0x?? 0x?? 0x?? 0x?? 0x?? 0x?? 0x?? 0x?? 0x01 0x00
#
# l7-filter strips out the null bytes, leaving me with this pattern

subspace
^\x01....\x11\x10........\x01$

