# ZMAAP - Zeroconf Multicast Address Allocation Protocol
# Pattern attributes: ok veryfast fast
# Protocol groups: networking ietf_draft_standard
# Wiki: http://www.protocolinfo.org/wiki/ZMAAP
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# http://files.zeroconf.org/draft-ietf-zeroconf-zmaap-02.txt
# (Note that this reference is an Internet-Draft, and therefore must
# be considered a work in progress.)
#
# This pattern is untested!

zmaap
# - 4 byte magic number.
# - 1 byte version. Allow 1 & 2, even though only version 1 currently exists.
# - 1 byte message type,which is either 0 or 1
# - 1 byte address family.  L7-filter only works in IPv4, so this is 1.
^\x1b\xd7\x3b\x48[\x01\x02]\x01?\x01
