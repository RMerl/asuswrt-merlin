/* Check that rare readlink calls don't cause the simulator to abort.
#notarget: cris*-*-elf
#simenv: env(-u\ PWD\ foo)=bar
   FIXME: Need to unset PWD, but right now I won't bother tweaking the
   generic parts of the testsuite machinery and instead abuse a flaw.  */
#include "readlink2.c"
