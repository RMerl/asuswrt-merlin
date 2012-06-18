# World of Warcraft - popular network game - http://blizzard.com/
# Pattern attributes: ok veryfast fast
# Protocol groups: game proprietary
# Wiki: http://www.protocolinfo.org/wiki/World_of_Warcraft
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE

worldofwarcraft
^\x06\xec\x01

# Quoth the author of this pattern, Weisskopf Beat <weisb AT bfh.ch>:

# I have written a pattern for wow (tested with versions 1.8.3 and
# 1.8.4, german edition). It does not match the login as i think this is
# uncritical, but i have added the necessary info later on. So only the
# actual in-game traffic is matched.
#
# I hope the pattern is specific enough, otherwise one may add some
# bytes from the response.
#
# some captured info:
#
# login:
#
# 0000:  00 02 28 00 57 6F 57 00 01 08 03 C7 12 36 38 78  ..(.WoW......68x
# 0010:  00 6E 69 57 00 45 44 65 64 3C 00 00 00 C0 A8 01  .niW.EDed<......
# 0020:  22 0A 42 57 45 49 53 53 4B 4F 50 46              ".BWEISSKOPF
#
# 0000:  00 02 28 00 57 6F 57 00 01 08 03 C7 12 36 38 78  ..(.WoW......68x
# 0010:  00 6E 69 57 00 45 44 65 64 3C 00 00 00 C0 A8 01  .niW.EDed<......
# 0020:  22 0A 42 57 45 49 53 53 4B 4F 50 46              ".BWEISSKOPF
#
# server asking:
#
# #1
# 0000:  00 06 EC 01 04 49 C5 33                          .....I.3
#
# #2
# 0000:  00 06 EC 01 C3 A8 6E 63                          ......nc
#
# client response
# #1
# 0000:  00 A4 ED 01 00 00 C7 12 00 00 00 00 00 00 42 57  ..............BW
# 0010:  45 49 53 53 4B 4F 50 46 00 EB 35 DC 89 5A CA 6D  EISSKOPF..5..Z.m
# 0020:  17 95 DE 5B 74 6E 1E 5D 23 73 C6 8F 27 9F 11 12  ...[tn.]#s..'...
# 0030:  BB 21 01 00 00 78 9C 75 CC 41 0A 83 50 0C 84 E1  .!...x.u.A..P...
# 0040:  E7 3D 7A 19 75 25 D4 4D AB EB 12 5E A2 0C 8D 51  .=z.u%.M...^...Q
# 0050:  D2 57 04 4F DF 2E 2D A4 B3 FD 86 3F A5 EF 1A C5  .W.O..-....?....
# 0060:  71 90 F3 A3 7E E7 82 D5 C6 2E 55 CB 7E B9 FE 58  q...~.....U.~..X
# 0070:  43 A5 A8 4C 10 E5 1E 86 85 B6 E8 04 63 D8 1C 06  C..L........c...
# 0080:  5A A7 A9 84 D2 D9 6B 93 1C 5B 4F D9 D7 50 6E 04  Z.....k..[O..Pn.
# 0090:  0E 61 20 15 8B 6B 83 13 CB FD 09 D5 7F 0C 13 3F  .a ..k.........?
# 00A0:  DB 07 B4 EA 54 F8                                ....T.
#
# #2
# 0000:  00 A4 ED 01 00 00 C7 12 00 00 00 00 00 00 42 57  ..............BW
# 0010:  45 49 53 53 4B 4F 50 46 00 38 4C B5 95 C3 AD 25  EISSKOPF.8L....%
# 0020:  CB 73 48 BD 82 FC 99 63 59 AC BF F3 D0 C6 8D AB  .sH....cY.......
# 0030:  3D 21 01 00 00 78 9C 75 CC 41 0A 83 50 0C 84 E1  =!...x.u.A..P...
# 0040:  E7 3D 7A 19 75 25 D4 4D AB EB 12 5E A2 0C 8D 51  .=z.u%.M...^...Q
# 0050:  D2 57 04 4F DF 2E 2D A4 B3 FD 86 3F A5 EF 1A C5  .W.O..-....?....
# 0060:  71 90 F3 A3 7E E7 82 D5 C6 2E 55 CB 7E B9 FE 58  q...~.....U.~..X
# 0070:  43 A5 A8 4C 10 E5 1E 86 85 B6 E8 04 63 D8 1C 06  C..L........c...
# 0080:  5A A7 A9 84 D2 D9 6B 93 1C 5B 4F D9 D7 50 6E 04  Z.....k..[O..Pn.
# 0090:  0E 61 20 15 8B 6B 83 13 CB FD 09 D5 7F 0C 13 3F  .a ..k.........?
# 00A0:  DB 07 B4 EA 54 F8                                ....T.

