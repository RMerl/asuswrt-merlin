# Shoutcast and Icecast - streaming audio
# Pattern attributes: good slow notsofast
# Protocol groups: streaming_audio
# Wiki: http://www.protocolinfo.org/wiki/Icecast
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# usually runs on port 80
#
# Original pattern contributed by Deepak Seshadri <dseshadri AT 
# broadbandmaritime.com> who says "The difference between [Shoutcast and 
# Icecast] is not clearly mentioned anywhere. According to this 
# document, my pattern would filter JUST shoutcast packets."
#
# Should now match both Shoutcast and Icecast.  Tested with Winamp (in 
# 2005) and Totem using streams at dir.xiph.org (in Nov 2007).
#
# http://sander.vanzoest.com/talks/2002/audio_and_apache/
# http://forums.radiotoolbox.com/viewtopic.php?t=74
# http://www.icecast.org

shoutcast
# The first branch looks for an HTTP request that looks like it is asking for
# a SHOUTcast stream.  The second branch looks for the server's reply.  However,
# some (newer?) servers answer with "http/1.0 200 OK", not "ICY 200 OK", so
# this will not work.
# This pattern was discovered using Ethereal.
^get /.*icy-metadata:1|icy [1-5][0-9][0-9] [\x09-\x0d -~]*(content-type:audio|icy-)
