# DNS - Domain Name System - RFC 1035
# Pattern attributes: great slow fast
# Protocol groups: networking ietf_internet_standard
# Wiki: http://www.protocolinfo.org/wiki/DNS
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE

# Thanks to Sebastien Bechet <s.bechet AT av7.net> for TLD detection
# improvements

# While RFC 2181 says "Occasionally it is assumed that the Domain Name
# System serves only the purpose of mapping Internet host names to data,
# and mapping Internet addresses to host names.  This is not correct, the
# DNS is a general (if somewhat limited) hierarchical database, and can
# store almost any kind of data, for almost any purpose.", we will assume 
# just that, because that represents the vast majority of DNS traffic.

# The packet starts with a 2 byte random ID number and 2 bytes of flags that
# aren't easy to match on.

# The first thing that is matchable is QDCOUNT, the number of queries.
# Despite the fact that you can apparently ask for up to 65535
# things at a time, usually you only ask for one and I doubt you ever ask for
# zero.  Let's allow up to two, just in case (even though I can't find any 
# situation that generates more than one).

# Next comes the ANCOUNT, NSCOUNT, and ARCOUNT fields, which could be null
# or some smallish number, not matchable except by length (up to 6)

# The next matchable thing is the query address. The first byte indicates the
# length of the first part of the address, which is limited to 63 (0x3F == '?').
# The next byte has to be a letter (for domain names) or number (for reverse lookups).
# Then there can be an combination of 
# letters, digits, hyphens, and 0x01-0x3F length markers.
# Then we check for the presence of a top-level-domain at some later point.
# This is indicated by a 0x02-0x06 and at least two letters, followed by no
# more than four more letters.
# Note that this will miss a very few queries that are for a TLD alone.
# i.e. "host museum" (195.7.77.17)
#
# http://www.icann.org/tlds   http://www.iana.org/cctld/cctld-whois.htm

# next is the QTYPE field, which has valid values 1-16 (although this
# could probably be restricted further since many are rare) and \x1c for
# IPv6 (and maybe more?).  It should follow immediately after the TLD
# (and some stripped-out nulls)

# next is QCLASS, which has valid values 1-4 and 255, except 2 is never used.
# I'm not sure if 3 and 4 are used, so I'll include them. 1=Internet 255=any

# If we wanted to match queries and responses separately, there could be
# more specifics after this for the responses.

dns
# here's a sane way of doing it
^.?.?.?.?[\x01\x02].?.?.?.?.?.?[\x01-?][a-z0-9][\x01-?a-z]*[\x02-\x06][a-z][a-z][fglmoprstuvz]?[aeop]?(um)?[\x01-\x10\x1c][\x01\x03\x04\xFF]

# This way assumes that TLDs are any alpha string 2-6 characters long.
# If TLDs are added, this is a good fallback.
#^.?.?.?.?[\x01\x02].?.?.?.?.?.?[\x01-?][a-z0-9][\x01-?a-z]*[\x02-\x06][a-z][a-z][a-z]?[a-z]?[a-z]?[a-z]?[\x01-\x10][\x01\x03\x04\xFF]

# If you have more processing power than me, you can substitute this for
# the [a-z][a-z][a-z]?[a-z]?[a-z]?[a-z]?
#(aero|arpa|biz|com|coop|edu|gov|info|int|mil|museum|name|net|org|pro|arpa|ac|ad|ae|af|ag|ai|al|am|an|ao|aq|ar|as|at|au|aw|az|ba|bb|bd|be|bf|bg|bh|bi|bj|bm|bn|bo|br|bs|bt|bv|bw|by|bz|ca|cc|cd|cf|cg|ch|ci|ck|cl|cm|cn|co|cr|cu|cv|cx|cy|cz|de|dj|dk|dm|do|dz|ec|ee|eg|eh|er|es|et|fi|fj|fk|fm|fo|fr|ga|gd|ge|gf|gg|gh|gi|gl|gm|gn|gp|gq|gr|gs|gt|gu|gw|gy|hk|hm|hn|hr|ht|hu|id|ie|il|im|in|io|iq|ir|is|it|je|jm|jo|jp|ke|kg|kh|ki|km|kn|kp|kr|kw|ky|kz|la|lb|lc|li|lk|lr|ls|lt|lu|lv|ly|ma|mc|md|mg|mh|mk|ml|mm|mn|mo|mp|mq|mr|ms|mt|mu|mv|mw|mx|my|mz|na|nc|ne|nf|ng|ni|nl|no|np|nr|nu|nz|om|pa|pe|pf|pg|ph|pk|pl|pm|pn|pr|ps|pt|pw|py|qa|re|ro|ru|rw|sa|sb|sc|sd|se|sg|sh|si|sj|sk|sl|sm|sn|so|sr|st|sv|sy|sz|tc|td|tf|tg|th|tj|tk|tm|tn|to|tp|tr|tt|tv|tw|tz|ua|ug|uk|um|us|uy|uz|va|vc|ve|vg|vi|vn|vu|wf|ws|ye|yt|yu|za|zm|zw)
