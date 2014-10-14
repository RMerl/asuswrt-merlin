#ifdef __KERNEL__
# ifdef CONFIG_SUPERH32
#  include "posix_types_32.h"
# else
#  include "posix_types_64.h"
# endif
#else
# ifdef __SH5__
#  include "posix_types_64.h"
# else
#  include "posix_types_32.h"
# endif
#endif /* __KERNEL__ */
