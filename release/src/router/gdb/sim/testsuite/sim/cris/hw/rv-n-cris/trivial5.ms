#mach: crisv10 crisv32
#sim(crisv10): --hw-device "/rv/trace? true"
#sim(crisv32): --hw-device "/rv/trace? true"
#output: /rv: WD\n
#output: /rv: REG R 0xd0000032\n
#output: /rv: := 0xabcdef01\n
#output: /rv: REG W 0xd0000036 := 0xaabbccdd\n
#output: /rv: REG R 0xd0000036\n
#output: /rv: := 0x76543210\n
#output: pass\n

# Test trace output for read and write.

#r @,@srcdir@/trivial4.r

 .include "trivial4.ms"
