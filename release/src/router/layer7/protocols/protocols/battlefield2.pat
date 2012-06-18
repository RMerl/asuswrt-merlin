# Battlefield 2 - An EA game.
# Pattern attributes: ok slow notsofast
# Protocol groups: game proprietary
# Wiki: http://www.protocolinfo.org/wiki/Battlefield_2
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# This pattern is unconfirmed except implicitly by a comment on protocolinfo.

battlefield2
# gameplay|account-login|server browsing/information
# See http://protocolinfo.org/wiki/Battlefield_2
# Can we put a ^ on the last branch?  If so, nosofast --> veryfast

# 193.85.217.35 on protocolinfo says:
# The first part of the pattern, \x11\x20\x01\xa0\x98\x11, has to be 
# modified for different version of Battlefield 2. The gameplay part of 
# pattern for BF2 v1.4 is \x11\x20\x01\x30\xb9\x10\x11, and for BF2 
# v1.41 is \x11\x20\x01\x50\xb9\x10\x11
#
# Rather than put all of those in, I've just gone with "...?" in the 
# middle.

^(\x11\x20\x01...?\x11|\xfe\xfd.?.?.?.?.?.?(\x14\x01\x06|\xff\xff\xff))|[]\x01].?battlefield2

# Pattern prior to 193.85.217.35's comment on protocolinfo:
#^(\x11\x20\x01\xa0\x98\x11|\xfe\xfd.?.?.?.?.?.?(\x14\x01\x06|\xff\xff\xff))|[]\x01].?battlefield2
