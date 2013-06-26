#!/usr/bin/env python

import dns.query
import dns.zone

z = dns.zone.from_xfr(dns.query.xfr('204.152.189.147', 'dnspython.org'))
names = z.nodes.keys()
names.sort()
for n in names:
        print z[n].to_text(n)
