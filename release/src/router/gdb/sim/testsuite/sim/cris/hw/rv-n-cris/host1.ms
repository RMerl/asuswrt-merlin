#mach: crisv32
#sim(crisv32): --hw-device "/rv/host localhost"

# Check that we trivially resolve a hostname.

#r @,@srcdir@/trivial4.r

 .include "trivial4.ms"
