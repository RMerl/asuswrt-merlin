/* Make sure the thread system trivially works with trace output.
#notarget: cris*-*-elf
#sim: --cris-trace=basic --trace-file=@exedir@/clone2.tmp
#output: got: a\nthen: bc\nexit: 0\n
*/
#include "clone1.c"
