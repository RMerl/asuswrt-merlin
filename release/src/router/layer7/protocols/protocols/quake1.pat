# Quake 1 - A popular computer game.
# Pattern attributes: marginal veryfast fast
# Protocol groups: game proprietary
# Wiki: http://www.protocolinfo.org/wiki/Quake
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# This pattern is untested and unconfirmed.

# Info taken from http://www.gamers.org/dEngine/quake/QDP/qnp.html,
# which says that it "is incomplete, inaccurate and only applies to
# versions 0.91, 0.92, 1.00 and 1.01 of QUAKE"

quake1
# Connection request: 80 00 00 0c 01 51 55 41 4b 45 00 03
# \x80 = control packet.
# \x0c = packet length
# \x01 = CCREQ_CONNECT
# \x03 = protocol version (3 == 0.91, 0.92, 1.00, 1.01)
^\x80\x0c\x01quake\x03
