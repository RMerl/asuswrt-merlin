# KuGoo - a Chinese P2P program - http://www.kugoo.com
# Pattern attributes: ok fast fast
# Protocol groups: p2p
# Wiki: http://www.protocolinfo.org/wiki/KuGoo
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE

kugoo
# liangjun says: "i find old pattern is not working for kugoo 2008. so i 
# write a new pattern of kugoo 2008 ,it's working with all of kugoo 2008 
# version!"
^(\x64.....\x70....\x50\x37|\x65.+)

# Pattern before 2008 11 08
#
# The author of this pattern says it works, but this is unconfirmed.
# Written by www.routerclub.com wsgtrsys.
#
# LanTian submitted \x64.+\x74\x47\x50\x37 for "KuGoo2", but adding as 
# another branch makes the pattern REALLY slow.  If it could have a ^, that'd
# be ok (still veryfast/fast).  Waiting to hear.
#^(\x31..\x8e|\x64.+\x74\x47\x50\x37)
