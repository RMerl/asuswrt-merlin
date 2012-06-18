# HTTP - Video over HyperText Transfer Protocol (RFC 2616)
# Pattern attributes: good notsofast notsofast subset
# Protocol groups: streaming_video document_retrieval ietf_draft_standard
# Wiki: http://protocolinfo.org/wiki/HTTP
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# Usually runs on port 80
#
# Contributed by Deepak Seshadri <dseshadri AT broadbandmaritime.com>
#
# This pattern has been tested and is believed to work well.
#
# To get or provide more information about this protocol and/or pattern:
# http://www.protocolinfo.org/wiki/HTTP
# http://lists.sourceforge.net/lists/listinfo/l7-filter-developers
#
# If you use this, you should be aware that:
#
# - they match both simple downloads of audio/video and streaming content.
#
# - blocking based on content-type encourages server
# writers/administrators to misreport content-type (which will just make
# headaches for everyone, including us), so I would strongly recommend
# shaping audio/video down to a speed that discourages use of streaming
# players without actually blocking it.
#
# - obviously, since this is a subset of HTTP, you need to match it
# earlier in your iptables rules than HTTP.

httpvideo
http/(0\.9|1\.0|1\.1)[\x09-\x0d ][1-5][0-9][0-9][\x09-\x0d -~]*(content-type: video)

