# STUN - Simple Traversal of UDP Through NAT - RFC 3489
# Pattern attributes: ok veryfast fast
# Protocol groups: networking ietf_proposed_standard
# Wiki: http://www.protocolinfo.org/wiki/STUN
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# This pattern is untested as far as I know.

# Wikipedia says: "The STUN server is contacted on UDP port 3478,
# however the server will hint clients to perform tests on alternate IP
# and port number too (STUN servers have two IP addresses). The RFC
# states that this port and IP are arbitrary."

stun
# \x01 is a Binding Request.  \x02 is a Shared Secret Request. Binding
# Requests are, experimentally, exactly 20 Bytes with three NULL Bytes. 
# The first NULL is part of the two byte message type field.  The other
# two give the message length, zero.  I'm guessing that Shared Secret
# Requests are similar, but I have not checked.  Please read the RFC and
# do experiments to find out.  All other message types are responses,
# and so don't matter.
#
# The .? allows one of the Message Transaction ID Bytes to be \x00.  If
# two are \x00, it will fail.  This will happen 0.37% of the time, since
# the Message Transaction ID is supposed to be random.  If this is
# unacceptable to you, add another ? to reduce this to 0.020%, but be
# aware of the increased possibility of false positives.
^[\x01\x02]................?$

# From my post to the mailing list:
# http://sourceforge.net/mailarchive/message.php?msg_id=36787107
# 
# This is a rather permissive pattern, but you can make it a little better 
# by combining it with another iptables rule that checks that the packet 
# data is exactly 20 Bytes.  Of course, the second packet is longer, so 
# maybe that introduces more complications than benefits.
# 
# If you're willing to wait until the second packet to make the 
# identification, you could use this:
# 
# ^\x01................?\x01\x01
# 
# or if the Message Length is always \x24 (I'm not sure it is from your 
# single example):
# 
# ^\x01................?\x01\x01\x24
