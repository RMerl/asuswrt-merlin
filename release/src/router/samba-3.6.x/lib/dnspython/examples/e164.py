#!/usr/bin/env python

import dns.e164
n = dns.e164.from_e164("+1 555 1212")
print n
print dns.e164.to_e164(n)
