# RTSP - Real Time Streaming Protocol - http://www.rtsp.org - RFC 2326
# Pattern attributes: good notsofast notsofast
# Protocol groups: streaming_video ietf_proposed_standard
# Wiki: http://www.protocolinfo.org/wiki/RTSP
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# usually runs on port 554
#
# To take full advantage of this pattern, please see the RTSP connection 
# tracking patch to the Linux kernel referenced at the above site.
#
# This pattern has been tested and is believed to work well.

rtsp
rtsp/1.0 200 ok
