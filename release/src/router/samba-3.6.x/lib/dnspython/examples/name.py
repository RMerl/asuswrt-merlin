#!/usr/bin/env python

import dns.name

n = dns.name.from_text('www.dnspython.org')
o = dns.name.from_text('dnspython.org')
print n.is_subdomain(o)         # True
print n.is_superdomain(o)       # False
print n > o                     # True
rel = n.relativize(o)           # rel is the relative name www
n2 = rel + o
print n2 == n                   # True
print n.labels                  # ['www', 'dnspython', 'org', '']
