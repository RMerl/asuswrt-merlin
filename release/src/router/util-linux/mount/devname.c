#include "fsprobe.h"

#include "devname.h"
#include "sundries.h"		/* for xstrdup */

const char *
spec_to_devname(const char *spec)
{
	if (!spec)
		return NULL;
	if (nocanonicalize || is_pseudo_fs(spec))
		return xstrdup(spec);
	return fsprobe_get_devname_by_spec(spec);
}

