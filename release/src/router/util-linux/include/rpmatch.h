#ifndef UTIL_LINUX_RPMATCH_H
#define UTIL_LINUX_RPMATCH_H

#ifndef HAVE_RPMATCH
#define rpmatch(r) \
	(*r == 'y' || *r == 'Y' ? 1 : *r == 'n' || *r == 'N' ? 0 : -1)
#endif

#endif /* UTIL_LINUX_RPMATCH_H */
