#mach: crisv0 crisv3 crisv8 crisv10 crisv32
#sim: --cris-unknown-syscall=foo
#xerror:
#output: Unknown option `--cris-unknown-syscall=foo'\n
 .include "testutils.inc"
 start
 fail
