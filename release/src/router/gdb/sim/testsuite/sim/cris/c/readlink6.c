/* Check that rare readlink calls don't cause the simulator to abort.
#notarget: cris*-*-elf
#dest: @exedir@/readlink6.c.x
*/
#include "readlink2.c"
