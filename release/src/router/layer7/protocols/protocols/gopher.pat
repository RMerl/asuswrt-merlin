# Gopher - A precursor to HTTP - RFC 1436
# Pattern attributes: good slow notsofast undermatch
# Protocol groups: document_retrieval obsolete ietf_rfc_documented
# Wiki: http://www.protocolinfo.org/wiki/Gopher
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# Gopher servers usually run on TCP port 70.
#
# This pattern is lightly tested using gopher.dna.affrc.go.jp .

gopher
# This matches the server's response, but naturally only if it is a
# directory listing, not if it is sending a file, because then the data 
# is totally arbitrary.

# Matches the client saying "list what you have", then the server
# response: one of the file type characters, any printable characters, a
# tab, any printable characters, a tab, something that looks like a
# domain name, a tab, and then a number which could be the start of a
# port number.

# "0About internet Gopher\tStuff:About us\trawBits.micro.umn.edu\t70"
# "\r7search by keywords on protein data using wais\twaissrc:/protein_all/protein\tgopher.dna.affrc.go.jp\t70"

^[\x09-\x0d]*[1-9,+tgi][\x09-\x0d -~]*\x09[\x09-\x0d -~]*\x09[a-z0-9.]*\.[a-z][a-z].?.?\x09[1-9]
