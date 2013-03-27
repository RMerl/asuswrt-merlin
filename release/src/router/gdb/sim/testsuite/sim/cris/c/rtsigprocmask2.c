/* As the included file, but specifying silent ENOSYS.
#notarget: cris*-*-elf
#cc: additional_flags=-pthread
#sim: --cris-unknown-syscall=enosys-quiet
#output: ENOSYS\n
#output: xyzzy\n
*/

#include "rtsigprocmask1.c"
