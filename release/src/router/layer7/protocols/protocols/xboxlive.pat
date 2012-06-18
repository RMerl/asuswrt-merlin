# XBox Live - Console gaming
# Pattern attributes: marginal slow notsofast
# Protocol groups: game proprietary
# Wiki: http://www.protocolinfo.org/wiki/XBox_Live
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# This may match all XBox traffic, or may only match Halo 2 traffic.  
# We don't know yet.
#
# Thanks to Myles Uyema <mylesuyema AT gmail DOT com>, who says:
#
# Analyzing packet traces using Ethereal, the Xbox typically connects
# to remote users using UDP port 3074.  The first frame is typically
# a 156 byte UDP payload.  I've only scrutinized the first 20 or so bytes.
#
# Each line below represents the first frame between my Xbox and a remote
# player's IP address playing Halo2 on Xbox Live.
#
# 00 00 00 00 00 58 80 00 00 00 00 00 82 31 9e a8 05 0f c5 62 00 f3 96 08
# 00 00 00 00 00 58 80 00 00 00 00 00 82 31 9e a8 0f 0f c5 62 00 f3 97 09
# 00 00 00 00 00 58 80 00 00 00 00 00 82 31 9e a8 05 0f c5 62 00 f3 95 07
# 00 00 00 00 00 58 80 00 00 00 00 00 81 87 ea 59 aa 11 ff 89 00 f3 bc 07
# 00 00 00 00 00 58 80 00 00 00 00 00 81 87 ea 59 aa 11 ff 89 00 f3 be 09
# 00 00 00 00 00 58 80 00 00 00 00 00 81 87 ea 59 aa 11 ff 89 00 f3 bf 0a
# 00 00 00 00 00 58 80 00 00 00 00 00 81 87 ea 59 aa 11 ff 89 00 f3 bd 08
# 00 00 00 00 00 58 80 00 00 00 00 00 81 87 ea 59 aa 11 ff 89 00 f3 ba 05
# 00 00 00 00 00 58 80 00 00 00 00 00 81 87 ea 59 aa 11 ff 89 00 f3 bb 06
# 00 00 00 00 00 58 80 00 00 00 00 00 81 7f dd 14 f2 8e a3 a1 00 f3 ca 06
# 00 00 00 00 00 58 80 00 00 00 00 00 81 7f dd 14 f2 8e a3 a1 00 f3 cc 08
# 00 00 00 00 00 58 80 00 00 00 00 00 81 7f dd 14 f2 8e a3 a1 00 f3 c9 05
# 00 00 00 00 00 58 80 00 00 00 00 00 8b ca 5b c0 d8 9c f8 c3 00 f3 d4 0a
# 00 00 00 00 00 58 80 00 00 00 00 00 8b ca 5b c0 d8 9c f3 c3 00 f3 d1 07
# 00 00 00 00 00 58 80 00 00 00 00 00 8b ca 5b c0 d8 9c f8 c3 00 f3 d2 08
# 00 00 00 00 00 58 80 00 00 00 00 00 8b ca 5b c0 d8 9c f8 c3 00 f3 cf 05
# 00 00 00 00 06 58 4e 00 00 00 e6 d9 6e ab 65 0d 63 9f 02 00 00 02 80 dd
# 00 00 00 00 06 58 4e 00 00 00 46 e2 95 74 cd f9 bc 3d 00 00 00 00 8b ca
# 00 00 00 00 06 58 4e 00 00 00 cf ce 3b 5c f5 f2 49 9a 00 00 00 00 8b ca
# 00 00 00 00 06 58 4e 00 00 00 a9 c0 ac c5 16 e5 c9 92 00 00 00 00 8b ca

xboxlive
^\x58\x80........\xf3|^\x06\x58\x4e
