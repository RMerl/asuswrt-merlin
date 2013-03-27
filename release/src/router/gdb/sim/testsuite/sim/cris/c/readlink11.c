/* As readlink5.c (sic), but specifying silent ENOSYS.
#notarget: cris*-*-elf
#dest: ./readlink11.c.x
#sim: --cris-unknown-syscall=enosys-quiet
#output: ENOSYS\n
#output: xyzzy\n
*/

#include "readlink2.c"
