# Tor - The Onion Router - used for anonymization - http://tor.eff.org
# Pattern attributes: good notsofast notsofast
# Protocol groups: networking
# Wiki: http://protocolinfo.org/wiki/Tor
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# This pattern has been tested and is believed to work well.
#
# It matches on the second packet.  I have no idea how the protocol
# works, but this matches every stream I have made using Tor 0.1.0.16 as
# a client on Linux.
#
# It does NOT attempt to match the HTTP request that fetches the list of
# Tor servers.

tor
TOR1.*<identity>
