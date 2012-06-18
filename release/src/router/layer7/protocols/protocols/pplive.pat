# PPLive - Chinese P2P streaming video - http://pplive.com
# Pattern attributes: ok notsofast notsofast
# Protocol groups: p2p streaming_video proprietary
# Wiki: http://www.protocolinfo.org/wiki/PPLive
# Copyright (C) 2008 Matthew Strait; See ../LICENSE

# By liangjun, who says that it works.  It may be easily improvable with 
# a bit more testing.

pplive
\x01...\xd3.+\x0c.$
