# HTTP by Fresh Download - http://www.freshdevices.com
# Pattern attributes: good notsofast notsofast subset
# Protocol groups: document_retrieval ietf_draft_standard
# Wiki: http://protocolinfo.org/wiki/HTTP
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
# Uses HTTP to download.

http-freshdownload

# Fresh Download identifies itself in the User-Agent field of every HTTP
# request it makes.

# The latest version uses "User-Agent: FreshDownload/4.40".  The
# additional version allowance is an attempt at "future proofing".

User-Agent: FreshDownload/[456](\.[0-9][0-9]?)?

