#mach: crisv0 crisv3 crisv8 crisv10 crisv32
#sim: --cris-cycles=foo
#xerror:
#output: Unknown option `--cris-cycles=foo'\n
 .include "testutils.inc"
 start
 fail
