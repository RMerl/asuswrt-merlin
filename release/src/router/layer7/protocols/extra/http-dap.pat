# HTTP by Download Accelerator Plus - http://www.speedbit.com
# Pattern attributes: good notsofast notsofast subset
# Protocol groups: document_retrieval ietf_draft_standard
# Wiki: http://protocolinfo.org/wiki/HTTP
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# Uses HTTP to download.

http-dap

# DAP identifies itself in the User-Agent field of every HTTP request it
# makes.  This is pretty trivial to get around if speedbit.com ever
# wanted to.

# The latest version uses "User-Agent: DA 7.0".  The additional version
# allowance is an attempt at "future proofing".

User-Agent: DA [678]\.[0-9]

