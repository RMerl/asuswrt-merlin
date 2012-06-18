# GoBoogy - a Korean P2P protocol
# Pattern attributes: marginal slow notsofast
# Protocol groups: p2p
# Wiki: http://www.protocolinfo.org/wiki/GoBoogy
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
# 
# This pattern is untested and likely does not work in all cases!
#
# By Adam Przybyla, modified by Matthew Strait.  Possibly lifted from 
# Josh Ballard (oofle.com).

goboogy
<peerplat>|^get /getfilebyhash\.cgi\?|^get /queue_register\.cgi\?|^get /getupdowninfo\.cgi\?
