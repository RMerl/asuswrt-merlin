/* As the included file, but specifying ENOSYS with message.
#notarget: cris*-*-elf
#sim: --cris-unknown-syscall=enosys
#output: Unimplemented syscall: 0 (0x3, 0x2, 0x1, 0x4, 0x6, 0x5)\n
#output: ENOSYS\n
#output: xyzzy\n
*/

#include "syscall2.c"
