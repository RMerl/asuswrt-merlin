# HTTP - iTunes (Apple's music program)
# Pattern attributes: good notsofast notsofast subset
# Protocol groups: streaming_audio ietf_draft_standard
# Wiki: http://protocolinfo.org/wiki/HTTP
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# Port 80
# iTunes program basically uses the HTTP protocol for its initial
# communication.
# Pattern contributed by Deepak Seshadri <dseshadri AT broadbandmaritime.com>

http-itunes
http/(0\.9|1\.0|1\.1).*(user-agent: itunes)

