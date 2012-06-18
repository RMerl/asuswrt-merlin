# Battlefield 2142 - An EA game.
# Pattern attributes: ok fast fast
# Protocol groups: proprietary game
# Wiki: http://protocolinfo.org/wiki/Battlefield_2142
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE

# Submitted by Telsin.  Not confirmed.

battlefield2142
# gameplay|account-login|server browsing/information
# Can't put a ^ on the last branch: it fails to match if you do.
# This branch seems to matter very rarely, though
^(\x11\x20\x01\x90\x50\x64\x10|\xfe\xfd.?.?.?\x18|[\x01\\].?battlefield2)

