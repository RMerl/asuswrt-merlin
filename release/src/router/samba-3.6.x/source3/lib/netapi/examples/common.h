#include <popt.h>

void popt_common_callback(poptContext con,
			 enum poptCallbackReason reason,
			 const struct poptOption *opt,
			 const char *arg, const void *data);

extern struct poptOption popt_common_netapi_examples[];

#define POPT_COMMON_LIBNETAPI_EXAMPLES { NULL, 0, POPT_ARG_INCLUDE_TABLE, popt_common_netapi_examples, 0, "Common samba netapi example options:", NULL },

