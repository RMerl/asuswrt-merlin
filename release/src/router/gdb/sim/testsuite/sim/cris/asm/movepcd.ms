# mach: crisv3 crisv8 crisv10
# xerror: 
# output: General register * PC is not implemented.\nprogram stopped with signal 5.\n

# Both source and dest contain PC for "test.d r" (move.d r,r).  Ideally,
# the output message should say "read" of PC, but we allow PC as source in
# a move.d r,R insn, so there's no logical way to get that, short of a
# special pattern, which would be just too ugly.  The output message says
# "write", but let's match "read" too so we won't fail if things suddenly
# improve.

 .include "testutils.inc"
 startnostack
 setf
 test.d pc
 quit
