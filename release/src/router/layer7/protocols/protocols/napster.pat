# Napster - P2P filesharing
# Pattern attributes: good fast fast
# Protocol groups: p2p
# Wiki: http://www.protocolinfo.org/wiki/Napster
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# All my tests show that this pattern is fast, but one user has reported that
# it is slow.  Your milage may vary.
# 
# Should work for any Napster offspring, like OpenNAP.
# (Yes, people still use this!)
# Matches both searches and downloads.
#
# http://opennap.sourceforge.net/napster.txt
#
# This pattern has been tested and is believed to work well.

napster
# (client-server: length, assumed to be less than 256, login or new user login, 
# username, password, port, client ID, link-type |
# client-client: 1, firewalled or not, username, filename) 
# Assumes that filenames are well-behaved ASCII strings.  I have found
# one case where this assumptions fails (filename had \x99 in it).
^(.[\x02\x06][!-~]+ [!-~]+ [0-9][0-9]?[0-9]?[0-9]?[0-9]? "[\x09-\x0d -~]+" ([0-9]|10)|1(send|get)[!-~]+ "[\x09-\x0d -~]+")
