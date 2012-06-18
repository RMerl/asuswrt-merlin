# Valid certificate SSL 
# Pattern attributes: good slow notsofast subset
# Protocol groups: secure ietf_proposed_standard
# Wiki: http://www.protocolinfo.org/wiki/SSL
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE

# This matches anything claiming to use a valid certificate from a well 
# known certificate authority.
#
# This is a subset of ssl, so it needs to come first to match.
#
# Note that opening a website that has a valid certificate will 
# open one connection that matches this and many ssl connections that
# only match the ssl pattern.  Thus, this pattern may not be very useful.
#
# This pattern is believed match only the above, but may not match all
# of it.
#
# the certificate authority info is sent in quasi plain text, if it matches 
# a well known certificate authority then we will assume it is a 
# web/imaps/etc server. Other ssl may be good too, but it should fall under 
# a different rule

validcertssl
^(.?.?\x16\x03.*\x16\x03|.?.?\x01\x03\x01?.*\x0b).*(thawte|equifax secure|rsa data security, inc|verisign, inc|gte cybertrust root|entrust\.net limited)
