# rlogin - remote login - RFC 1282
# Pattern attributes: ok fast fast
# Protocol groups: remote_access ietf_rfc_documented
# Wiki: http://www.protocolinfo.org/wiki/Rlogin
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# usually runs on port 443
#
# This pattern is untested.

rlogin
# At least three characters (user name, user name, terminal type), 
# the first of which could be the first character of a user name, a
# slash, then a terminal speed.  (Assumes that usernames and terminal
# types are alphanumeric only.  I'm sure there are usernames like
# "straitm-47" out there, but it's not common.) All terminal speeds 
# I know of end in two zeros and are between 3 and 6 digits long.
# This pattern is uncomfortably general.
^[a-z][a-z0-9][a-z0-9]+/[1-9][0-9]?[0-9]?[0-9]?00
