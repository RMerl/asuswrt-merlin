# Quicktime HTTP 
# Pattern attributes: good notsofast notsofast subset
# Protocol groups: streaming_video streaming_audio ietf_draft_standard
# Wiki: http://protocolinfo.org/wiki/HTTP
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# This pattern has been tested and is believed to work well.  
# (Quick Time v6.5.1 downloading from www.apple.com/trailers)
#
# To get or provide more information about this protocol and/or pattern:
# http://www.protocolinfo.org/wiki/HTTP
# http://lists.sourceforge.net/lists/listinfo/l7-filter-developers
#
# Since this is a subset of HTTP, it should be put earlier in the packet
# filtering chain than HTTP.  Also, please don't use this to block Quicktime.
# If you must do that, you should use a filtering HTTP proxy, which is probably
# more accurate.

quicktime
user-agent: quicktime \(qtver=[0-9].[0-9].[0-9];os=[\x09-\x0d -~]+\)\x0d\x0a

