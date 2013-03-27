/* Check that rare readlink calls don't cause the simulator to abort.
#notarget: cris*-*-elf
#sim: --sysroot=@exedir@
#simenv: env(-u\ PWD\ foo)=bar
   FIXME: Need to unset PWD, but right now I won't bother tweaking the
   generic parts of the testsuite machinery and instead abuse a flaw.  */
#define SYSROOTED 1
#include "readlink2.c"
