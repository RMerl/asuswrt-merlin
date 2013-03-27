# RTP - Real-time Transport Protocol - RFC 3550
# Pattern attributes: marginal overmatch undermatch veryfast fast
# Protocol groups: streaming_video ietf_internet_standard
# Wiki: http://www.protocolinfo.org/wiki/RTP
#
# RTP headers are *very* short and compact.  They have almost nothing in 
# them that can be matched by l7-filter.  If you want to match them 
# along with their associated SIP packets, I think the best way might be 
# to set up some iptables rules that watch for SIP packets and then also 
# match any other UDP packets that are going between the same two IP 
# addresses.
#
# However, I will attempt a pattern anyway.  This is UNTESTED!
# 
# I think we can count on the first bit being 1 and the second bit being 
# 0 (meaning protocol version 2). The next two bits could go either way, 
# but in the example I've seen, they are zero, so I'll assume they are 
# usually zero.  The next four bits are a count of "contributing source 
# identifiers".  I'm not sure how big that could be, but in the example 
# I've seen, they're zero, so I'll assume they're usually zero. So that 
# gives us ^\x80.  The marker bit that comes next is probably zero for 
# the first packet, although that's not a sure thing.  Next is the 
# payload type, 7 bits that might usually only take a few values, but 
# maybe not. In the example I've seen, it's zero, which (with a zero 
# marker bit) means it looks to l7-filter like it's not there at all.  
# The rest of the header is random numbers (sequence number, timestamp, 
# synchronization source identifier), so that's no help at all.
#
# I think the best we could do is to watch to see if several \x80 bytes 
# come in with a small number of bytes between them.  This makes all the 
# above assumptions and also assumes that the first packet has no 
# payload and not too much trailing gargage.  So this will definitely not
# work all the time.  It clearly also might match other stuff.

rtp
^\x80......?.?.?.?.?.?.?.?.?.?.?.?.?\x80

# Might also try this.  It's a bit slower (one packet and not too much extra
# regexec load) and a bit more accurate:
#^\x80......?.?.?.?.?.?.?.?.?.?.?.?.?\x80.*\x80
