# Computer Interface to Message Distribution, an SMSC protocol by Nokia
# Pattern attributes: good notsofast notsofast subset
# Protocol groups: proprietary chat
# Wiki: http://www.protocolinfo.org/wiki/CIMD
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE

# I don't know whether CIMD is ever found by itself in a TCP connection. 
# I have only seen it myself as part of the Chikka login process, in 
# which the second and third packets (at least) are CIMD.  So I am not 
# using a '^' at the beginning.
#
# This pretty well explains the pattern:
# http://en.wikipedia.org/w/index.php?title=CIMD&oldid=42707583
# However, Chikka does NOT terminate the last field with a tab.
#
# Tested with Chikka Javalite on 14 Jan 2007.

cimd
\x02[0-4][0-9]:[0-9]+.*\x03$
