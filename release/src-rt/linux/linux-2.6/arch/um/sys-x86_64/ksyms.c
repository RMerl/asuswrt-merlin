#include <linux/module.h>
#include <asm/string.h>
#include <asm/checksum.h>

/*XXX: we need them because they would be exported by x86_64 */
#if (__GNUC__ == 4 && __GNUC_MINOR__ >= 3) || __GNUC__ > 4
EXPORT_SYMBOL(memcpy);
#else
EXPORT_SYMBOL(__memcpy);
#endif
EXPORT_SYMBOL(csum_partial);
