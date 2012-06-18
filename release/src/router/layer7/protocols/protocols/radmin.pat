# Famatech Remote Administrator - remote desktop for MS Windows
# Pattern attributes: ok veryfast fast
# Protocol groups: remote_access proprietary
# Wiki: http://www.protocolinfo.org/wiki/Radmin
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# This pattern has been verified with Radmin v1.1 and v3.0beta on Win2000/XP
# It has only been tested between a single pair of computers.

# The first packet of every TCP stream appears to be either one of:
#
# 01 00 00 00  01 00 00 00  08 08
# 01 00 00 00  01 00 00 00  1b 1b

radmin
^\x01\x01(\x08\x08|\x1b\x1b)$

