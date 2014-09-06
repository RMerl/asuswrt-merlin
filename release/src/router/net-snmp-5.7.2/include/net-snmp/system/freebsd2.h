#include "freebsd.h"

/*
 * this is not good enough before freebsd3! 
 */
#undef HAVE_NET_IF_MIB_H
#undef PROC_SYMBOL
#undef NPROC_SYMBOL
#undef LOADAVE_SYMBOL
#undef TOTAL_MEMORY_SYMBOL
