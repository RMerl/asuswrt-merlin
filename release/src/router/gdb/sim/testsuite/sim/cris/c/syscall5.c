/* As the included file, but specifying ENOSYS with message.
#notarget: cris*-*-elf
#sim: --cris-unknown-syscall=enosys
#output: Unimplemented syscall: 166 (0x1, 0x2, 0x3, 0x4, 0x5, 0x6)\n
#output: ENOSYS\n
#output: xyzzy\n
*/

#include "syscall1.c"
