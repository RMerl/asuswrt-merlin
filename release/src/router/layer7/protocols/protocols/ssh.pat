# SSH - Secure SHell
# Pattern attributes: great veryfast fast
# Protocol groups: remote_access secure ietf_draft_standard
# Wiki: http://www.protocolinfo.org/wiki/SSH
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# usually runs on port 22
#
# http://www.ietf.org/internet-drafts/draft-ietf-secsh-transport-22.txt
#
# This pattern has been tested and is believed to work well.

ssh
^ssh-[12]\.[0-9]

# old pattern:
# (diffie-hellman-group-exchange-sha1|diffie-hellman-group1-sha1.ssh-rsa|ssh-dssfaes128-cbc|3des-cbc|blowfish-cbc|cast128-cbc|arcfour|aes192-cbc|aes256-cbc|rijndael-cbc@lysator.liu.sefaes128-cbc|3des-cbc|blowfish-cbc|cast128-cbc|arcfour|aes192-cbc|aes256-cbc|rijndael-cbc@lysator.liu.seuhmac-md5|hmac-sha1|hmac-ripemd160)+
