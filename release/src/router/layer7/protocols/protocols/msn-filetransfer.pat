# MSN (Micosoft Network) Messenger file transfers (MSNFTP and MSNSLP)
# Pattern attributes: good fast fast
# Protocol groups: chat document_retrieval proprietary
# Wiki: http://www.protocolinfo.org/wiki/MSN_Messenger
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# http://www.hypothetic.org/docs/msn/client/file_transfer.php

# NOTE!  This pattern does not catch the modern type of MSN filetransfers
# because they use the same TCP connection as the chat itself.  See 
# ../example_traffic/msn_chat_and_file_transfer.txt for a demonstration.

# This pattern has been tested and seems to work well.  It, does,
# however, require more testing with various versions of the official
# MSN client as well as with clones such as Trillian, Miranda, Gaim,
# etc.  If you are using a MSN clone and this pattern DOES work for you,
# please, also let us know.

# First part matches the older MSNFTP: A MSN filetransfer is a normal
# MSN connection except that the protocol is MSNFTP. Some clients
# (especially Trillian) send other protocol versions besides MSNFTP
# which should be matched by the [ -~]*.

# Second part matches newer MSNSLP: 
# http://msnpiki.msnfanatic.com/index.php/MSNC:MSNSLP
# This part is untested.

msn-filetransfer
^(ver [ -~]*msnftp\x0d\x0aver msnftp\x0d\x0ausr|method msnmsgr:)

