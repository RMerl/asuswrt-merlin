# HL1, Quake, CS - Half Life 1 engine games (HL 1, Quake 2/3/World, Counterstrike 1.6, etc.)
# Pattern attributes: good veryfast fast
# Protocol groups: game proprietary
# Wiki: http://www.protocolinfo.org/wiki/Half-Life http://www.protocolinfo.org/wiki/Counter-Strike http://www.protocolinfo.org/wiki/Day_of_Defeat
#
# Contributed by Laurens Blankers <laurens AT blankersfamily.com>, who says:
#
# This pattern has been tested with QuakeWorld (2.30), Quake 2 (3.20), 
# Quake 3 (1.32), and Half-life (1.1.1.0). But may also work on other
# games based on the Quake engine.
#
# Clayton Macleod <cherrytwist A gmail.com> says:
# [This should match] Counter-Strike v1.6, [...] the slightly updated
# Counter-Strike: Condition Zero, and the game Day Of Defeat, Team
# Fortress Classic, Deathmatch Classic, Ricochet, Half-Life [1] Deathmatch,
# and I imagine all the other 3rd party mods that also use this engine
# will match that pattern.

quake-halflife
# All quake (like) protocols start with 4x 0xFF.  Then the client either
# issues getinfo or getchallenge.
^\xff\xff\xff\xffget(info|challenge)

# A previous quake pattern allowed the connection to start with only 2 bytes
# of 0xFF.  This doesn't seem to ever happen, but we should keep an eye out
# for it.
