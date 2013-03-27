# KuGoo - a Chinese P2P program - http://www.kugoo.com
# Pattern attributes: ok veryfast fast
# Protocol groups: p2p
# Wiki: http://www.protocolinfo.org/wiki/KuGoo
#
# The author of this pattern says it works, but this is unconfirmed.
# Written by www.routerclub.com wsgtrsys.
#
# LanTian submitted \x64.+\x74\x47\x50\x37 for "KuGoo2", but adding as 
# another branch makes the pattern REALLY slow.  If it could have a ^, that'd
# be ok (still veryfast/fast).  Waiting to hear.

kugoo
^(\x31..\x8e|\x64.+\x74\x47\x50\x37)
