#mach: crisv0 crisv3 crisv8 crisv10 crisv32
#sim: --cris-trace=foo
#xerror:
#output: Unknown option `--cris-trace=foo'\n
 .include "testutils.inc"
 start
 fail
