# Counterstrike (using the new "Source" engine) - network game 
# Pattern attributes: good veryfast fast
# Protocol groups: game proprietary
# Wiki: http://www.protocolinfo.org/wiki/Counter-Strike
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# By adam.randazzoATgmail.com

counterstrike-source
^\xff\xff\xff\xff.*cstrikeCounter-Strike

# These games use Steam, which is developed by Valve Software.
#
# This was based off of the following captured data from ethereal:
# --Source--
# 0000   00 11 09 2a a8 79 00 13 10 2c 3f d7 08 00 45 20  ...*.y...,?...E 
# 0010   00 72 b9 f6 00 00 6b 11 b6 78 18 0e 04 cc c0 a8  .r....k..x......
# 0020   01 6a 69 87 04 65 00 5e 01 ac ff ff ff ff 49 07  .ji..e.^......I.
# 0030   54 4a 27 73 20 50 6c 61 63 65 20 6f 66 20 50 61  TJ's Place of Pa
# 0040   69 6e 00 64 65 5f 70 69 72 61 6e 65 73 69 00 63  in.de_piranesi.c
# 0050   73 74 72 69 6b 65 00 43 6f 75 6e 74 65 72 2d 53  strike.Counter-S
# 0060   74 72 69 6b 65 3a 20 53 6f 75 72 63 65 00 dc 00  trike: Source...
# 0070   08 10 06 64 77 00 00 31 2e 30 2e 30 2e 31 38 00  ...dw..1.0.0.18.
# 0080  
# 
# --1.6--
# 0000   00 11 09 2a a8 79 00 13 10 2c 3f d7 08 00 45 00  ...*.y...,?...E.
# 0010   00 8e c4 1a 00 00 76 11 b3 85 08 09 02 fa c0 a8  ......v.........
# 0020   01 14 69 91 04 37 00 7a c9 90 ff ff ff ff 6d 38  ..i..7.z......m8
# 0030   2e 39 2e 32 2e 32 35 30 3a 32 37 30 32 35 00 49  .9.2.250:27025.I
# 0040   50 20 2d 20 43 6c 61 6e 20 73 65 72 76 65 72 00  P - Clan server.
# 0050   64 65 5f 64 75 73 74 32 00 63 73 74 72 69 6b 65  de_dust2.cstrike
# 0060   00 43 6f 75 6e 74 65 72 2d 53 74 72 69 6b 65 00  .Counter-Strike.
# 0070   0a 0c 2f 64 77 00 01 77 77 77 2e 63 6f 75 6e 74  ../dw..www.count
# 0080   65 72 2d 73 74 72 69 6b 65 2e 6e 65 74 00 00 00  er-strike.net...
# 0090   01 00 00 00 00 9e f7 0a 00 01 00 00              ............


# Old pattern.  (Adam Randazzo says "CS 1.6 and CS: Source are the
# only two versions that are playable on the Internet since Valve
# disabled the WON system in favor of steam.")
# cs .*dl.www.counter-strike.net
