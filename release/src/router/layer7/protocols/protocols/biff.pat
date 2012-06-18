# Biff - new mail notification 
# Pattern attributes: good fast fast undermatch overmatch
# Protocol groups: mail
# Wiki: http://www.protocolinfo.org/wiki/Biff
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# Usually runs on port 512
#
# This pattern is completely untested.

biff
# This is a rare case where we will specify a $ (end of line), since
# this is the entirety of the communication.
# something that looks like a username, an @, a number.
# won't catch usernames that have strange characters in them.
^[a-z][a-z0-9]+@[1-9][0-9]+$
