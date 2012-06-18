# Ventrilo - VoIP - http://ventrilo.com
# Pattern attributes: good fast fast
# Protocol groups: voip proprietary
# Wiki: http://www.protocolinfo.org/wiki/Ventrilo
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# I have tested this with Ventrilo client 2.3.0 on Windows talking to
# Ventrilo server 2.3.1 (the public version) on Linux.  I've done this
# both within a LAN and over the Internet.  In one test, I tried
# monkeying around with the server settings to see if I could break the
# pattern, and I couldn't.  However, you can't change the port number in
# the public server. 
#
# It has also been tested by one other person in an unknown configuration.

ventrilo
^..?v\$\xcf

