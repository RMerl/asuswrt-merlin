# RTP - Real-time Transport Protocol - RFC 3550
# Pattern attributes: ok overmatch undermatch fast fast
# Protocol groups: streaming_video ietf_internet_standard
# Wiki: http://www.protocolinfo.org/wiki/RTP
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# RTP headers are *very* short and compact.  They have almost nothing in 
# them that can be matched by l7-filter.  As RTP connections take place 
# between even numbered ports, you should probably check for that before 
# applying this pattern.  If you want to match them along with their 
# associated SIP packets, you might try setting up some iptables rules 
# that watch for SIP packets and then also match any other UDP packets 
# that are going between the same two IP addresses.
#
# I think we can count on the first bit being 1 and the second bit being 
# 0 (meaning protocol version 2). The next two bits could go either way, 
# but in the example I've seen, they are zero, so I'll assume they are 
# usually zero.  The next four bits are a count of "contributing source 
# identifiers".  I'm not sure how big that could be, but in the example 
# I've seen, they're zero, so I'll assume they're usually zero. So that 
# gives us ^\x80.  The next bit is a tossup. Next is the payload type, 7 
# bits.  I've taken likely values from the WireShark code: 0-34, 96-127 
# (decimal). The rest of the header is random numbers (sequence number, 
# timestamp, synchronization source identifier), so that's no help at 
# all.

rtp
^\x80[\x01-"`-\x7f\x80-\xa2\xe0-\xff]?..........*\x80

# Might also try this.  It's a bit slower (one packet and not too much extra
# regexec load) and a bit more accurate:
#^\x80[\x01-"`-\x7f\x80-\xa2\xe0-\xff]?..........*\x80.*\x80

