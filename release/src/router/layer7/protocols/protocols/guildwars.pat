# Guild Wars - online game - http://guildwars.com
# Pattern attributes: marginal veryfast fast
# Protocol groups: game proprietary
# Wiki: http://www.protocolinfo.org/wiki/Guild_Wars
# Copyright (C) 2008 Matthew Strait; See ../LICENSE

# Contributed on protocolinfo by Greatwolf with the comment, "Guild Wars 
# uses encrypted data on tcp/6112 and may be impossible to match by 
# content. An experimental filter has been written to match Guild Wars 
# packets. More testing is still required to determine the effectiveness 
# of this pattern."

guildwars
^[\x04\x05]\x0c.i\x01
