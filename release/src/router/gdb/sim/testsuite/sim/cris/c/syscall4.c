/* As the included file, just actually specifying the default.
#notarget: cris*-*-elf
#sim: --cris-unknown-syscall=stop
#xerror:
#output: Unimplemented syscall: 0 (0x3, 0x2, 0x1, 0x4, 0x6, 0x5)\n
#output: program stopped with signal 4.\n
*/

#include "syscall2.c"
