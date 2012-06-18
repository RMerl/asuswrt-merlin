# Finger - User information server - RFC 1288
# Pattern attributes: good slow slow undermatch overmatch
# Protocol groups: ietf_draft_standard
# Wiki: http://www.protocolinfo.org/wiki/Finger
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# Usually runs on port 79
#
# This pattern is lightly tested.

finger
# The first matches the client request, which should look like a username.
# The second matches the usual UNIX reply (but remember that they are
# allowed to say whatever they want)
^[a-z][a-z0-9\-_]+|login: [\x09-\x0d -~]* name: [\x09-\x0d -~]* Directory: 
