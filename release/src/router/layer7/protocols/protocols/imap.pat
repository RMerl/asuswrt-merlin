# IMAP - Internet Message Access Protocol (A common e-mail protocol)
# Pattern attributes: great fast fast
# Protocol groups: mail ietf_proposed_standard
# Wiki: http://www.protocolinfo.org/wiki/IMAP
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# This matches IMAP4 (RFC 3501) and probably IMAP2 (RFC 1176)
#
# This pattern has been tested and is believed to work well.
# 
# This matches the IMAP welcome message or a noop command (which for 
# some unknown reason can happen at the start of a connection?)  
imap
^(\* ok|a[0-9]+ noop)
