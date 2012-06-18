# Xunlei - Chinese P2P filesharing - http://xunlei.com
# Pattern attributes: good slow notsofast
# Protocol groups: p2p
# Wiki: http://www.protocolinfo.org/wiki/Xunlei
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# This has been tested by a number of people.
#
# Written by wsgtrsys of www.routerclub.com.  Improved by VeNoMouS.
# Improved more by wsgtrsys and platinum of bbs.chinaunix.net.
#
# Further additions of HTTP-like content by liangjunATdcuxD.Tcom, who
# says: "i find old pattern is not working . so i write a new pattern of 
# xunlei,it's working with all of xunlei 5 version!"  Matthew Strait notes
# in response:
# 
# I've looked around and I'm fairly sure that Internet Explorer 5.0 
# never identifies itself as "Mozilla/4.0 (compatible; MSIE 5.00; 
# Windows 98)" and that Internet Explorer 6.0 never identifies itself as 
# either "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; )" or 
# "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)".

# The keep-alive part needs some examination too.  These might validly 
# occur in an HTTP/1.0 connection, although I think in practical cases 
# they don't since there's general only one \x0d\x0a after it and/or the 
# next line starts with a letter (especially because it's the client 
# sending it).  It wouldn't be crazy, though, if another protocol 
# (besides Xunlei) used keep-alive in a way that did match this.  But 
# since I can't think of any examples, I'll assume it's ok for now.

xunlei
^([()]|get)(...?.?.?(reg|get|query)|.+User-Agent: (Mozilla/4\.0 \(compatible; (MSIE 6\.0; Windows NT 5\.1;? ?\)|MSIE 5\.00; Windows 98\))))|Keep-Alive\x0d\x0a\x0d\x0a[26]


# This was the pattern until 2008 11 08.  It is safer than the above against
# overmatching ordinary HTTP connections
#^[()]...?.?.?(reg|get|query)

# More detail:
# From http://sourceforge.net/tracker/index.php?func=detail&aid=1885209&group_id=80085&atid=558668
# 
##############################################################################
# Date: 2008-02-03
# Sender: hydr0g3n
# 
# Xunlei (Chinese P2P) traffic is not matched anymore by layer7 xunlei
# pattern. It used to work in the past but not anymore. Maybe Xunlei was
# updated and pattern should be adapted?
#
# Apparently ipp2p was edited by Chinese people to detect pplive and xunlei.
# It is interesting and very recent:
# http://www.chinaunix.net/jh/4/914377.html
##############################################################################
# Date: 2008-02-03
# Sender: quadong
# 
# Ok.  Only some of the ipp2p function can be translated into an l7-filter
# regular expression.  The first part of search_xunlei can't be, since it
# works by checking whether the length of the packet matches a byte in the
# packet.  The second part of search_xunlei becomes: 
# 
# \x20.?\x01?.?[\x01\x77]............?.?.?.?\x38
# 
# Or possibly:
# 
# ^\x20.?\x01?.?[\x01\x77]............?.?.?.?\x38
# 
# I'm not sure whether IPP2P looks at every packet or only the first of each
# connection.
# 
# udp_search_xunlei says:
# \x01\x01\x01\xfe\xff\xfe\xff|\x01\x11\xa0\xfe\xff\xfe\xff
# 
# Again, putting a ^ at the beginning might work:
# 
# ^(\x01\x01\x01\xfe\xff\xfe\xff|\x01\x11\xa0\xfe\xff\xfe\xff)
# 
# So this *might* work:
# 
# ^(\x20.?\x01?.?[\x01\x77]............?.?.?.?\x38|\x01\x01\x01\xfe\xff\xfe\xff|\x01\x11\xa0\xfe\xff\xfe\xff)
# 
# but the ^ might be wrong and it will not match the HTTP part of Xunlei. 
##############################################################################
