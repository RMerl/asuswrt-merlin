# Chikka - SMS service which can be used without phones - http://chikka.com
# Pattern attributes: good fast fast superset
# Protocol groups: proprietary chat
# Wiki: http://www.protocolinfo.org/wiki/Chikka
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE

# Tested with Chikka Javalite on 14 Jan 2007.
# The login and chat use the same TCP connection.

# "Kamusta" means "Hello" in Tagalog, apparently, so that will probably 
# stay the same.  I've only seen v1.2, but I've given it some leeway for 
# past and future versions.

# Chikka uses CIMD as part of the login process, see cimd.pat

chikka
^CTPv1\.[123] Kamusta.*\x0d\x0a$
