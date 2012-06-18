# RTSP tunneled within HTTP
# Pattern attributes: ok notsofast fast subset
# Protocol groups: streaming_audio streaming_video ietf_draft_standard
# Wiki: http://www.protocolinfo.org/wiki/RTSP
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# Apple's documentation on what Quicktime does:
# http://developer.apple.com/quicktime/icefloe/dispatch028.html
# This is what the first part of the pattern is about
#
# The second part is based on the example in RFC 2326.  For this part to
# work, this pattern MUST be earlier in the iptables rules chain than
# HTTP.  Otherwise, the stream will be identified as HTTP.

http-rtsp
^(get[\x09-\x0d -~]* Accept: application/x-rtsp-tunnelled|http/(0\.9|1\.0|1\.1) [1-5][0-9][0-9] [\x09-\x0d -~]*a=control:rtsp://)
