# Runes of Magic - game - http://www.runesofmagic.com
# Pattern attributes: ok veryfast fast
# Protocol groups: game proprietary
# Wiki: http://www.protocolinfo.org/wiki/Runes_of_Magic
# Copyright (C) 2008 Matthew Strait; See ../LICENSE

runesofmagic
^\x10\x03...........\x0a\x02.....\x0e
# See below (this is also veryfast fast)
#^\x10\x03...........?\x0a\x02.....?$

# Greatwolf captured the following:
#  
# Server:
#
# 10 00 00 00 03 78 76 7a  1e 8a dd b5 95 a3 3a de .....xvz ......:.
# 0a 00 00 00 02 df 85 cc  cc cc                   ........ ..
# 
# Client reply:
#
# 0e 00 00 00 02 28 82 cc  cc cc 8b c9 cc cc       .....(.. ......
#  
# Server:
# 
# 2e 00 00 00 02 1e 7f f4  f4 f4 ef f4 f4 f4 b3 8c ........ ........
# [...]
#
# And says: "Bytes 10 00 00 00 03, 0a 00 00 00 02 and 0e (client reply) 
# were consistently present.
#
# ^\x10\x03...........\x0a\x02.....\x0e
#
# Pattern was able to match during the closed beta period. It is still 
# matching okay after RoM started open beta but could definitely use 
# more testing from others to verify effectiveness."
#
# Matthew Strait says:
#
# * If the server consistently sends those four bytes in the first packet,   
# it is probably wasteful to wait for the next (client) packet before
# matching.
#
# * If we switch the match strategy to just looking at the first packet, and
# the first packet is always the same (or nearly the same) length, we can
# anchor (i.e. use a '$') at the end of the packet.
# 
# * When there's a string of bytes that I don't understand and that take    
# different values from connection to connection, I think it's good to allow
# for the possibility that at least one might be \x00, and so I'd make one  
# of the "." into ".?", unless you *know* that \x00 is impossible somehow.
#
# * All of those \xcc bytes don't look random to me.  Your comments suggest
# that it isn't always exactly like that, but is there always pattern of
# repeated bytes or something else that might be useful?  It probably isn't
# necessary to exploit this, since it looks like there's already enough to  
# go with, but it would be nice to understand.
#
# So perhaps it would be an improvement to use:
#
# ^\x10\x03...........?\x0a\x02.....?$
#
# but this depends on the assumptions I made above.

