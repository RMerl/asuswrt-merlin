# Ares - P2P filesharing - http://aresgalaxy.sf.net
# Pattern attributes: good veryfast fast undermatch
# Protocol groups: p2p open_source
# Wiki: http://www.protocolinfo.org/wiki/Ares
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE

# This pattern catches only client-server connect messages.  This is
# sufficient for blocking, but not for shaping, since it doesn't catch
# the actual file transfers (see below).

# Original pattern by Brandon Enright <bmenrigh at the server known as ucsd.edu>

# This pattern has been tested with Ares 1.8.8.2998.

ares
# regular expression madness: "[]Z]" means ']' or 'Z'.
^\x03[]Z].?.?\x05$

# It appears that the general packet format is:
# - Two byte little endian integer giving the data length
# - One byte packet type
# - data
#
# Login packets (TCP) have the following format:
# - \x03\x00 (the length appears to always be 3)
# - \x5a - The login packet type.
#   The source code suggests that for supernodes \x5d is used instead.
# - Three more bytes.  I don't know the meaning of these, but for me they 
#   are always \x06\x06\x05 (in Ares 1.8.8.2998).  From the comments in IPP2P, 
#   it seems that they are not always exactly that, but seem to always end in 
#   \x05.
#
# Search packets have the following format:
# - Two byte little endian integer giving the data length
#   A single two letter word make this \x0a
#   The biggest I could get it was \x4f
# - Packet type = \x09
# - One byte document type:
#   - "all"      = 00
#   - "audio"    = 01
#   - "software" = 03
#   - "video"    = 05
#   - "document" = 06
#   - "image"    = 07
#   - "other"    = 08
# - \x0f - I don't know what this means, but it is always this for me
# - Two bytes of unknown meaning that change
# - Some number search words: 
#   - \x14 - I don't know what this means, but it is always this for me
#   - One byte length of the first search word 
#     Between 2 and \x14 in my tests with Ares 1.8.8.2998
#     It ignores single letter words and truncates ones longer than \x14
#   - Two bytes of unknown meaning that change
#   - The search word (not null terminated)
# This was all investigated by searching for strings in "all".  Searches
# can also be performed in "title" and "author".  I'm not going to
# bother to research these because I new realize that searches are done
# on the same TCP connection as the login packets, so there is no need
# to match them separately.
#
# File transfers appear to be encrypted or at least obfuscated.  (The
# files themselves, at least, are not transmitted in the clear.) I
# haven't found any patterns.
