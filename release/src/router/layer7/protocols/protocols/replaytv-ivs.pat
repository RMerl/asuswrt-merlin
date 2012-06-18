# ReplayTV Internet Video Sharing - Digital Video Recorder - http://replaytv.com
# Pattern attributes: good fast fast
# Protocol groups:
# Wiki: http://www.protocolinfo.org/wiki/ReplayTV
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# Pattern by jm 409 at hot mail dot com, who says that this one "worked best".

replaytv-ivs
^(get /ivs-IVSGetFileChunk|http/(0\.9|1\.0|1\.1) [1-5][0-9][0-9] [\x09-\x0d -~]*\x23\x23\x23\x23\x23REPLAY_CHUNK_START\x23\x23\x23\x23\x23)

