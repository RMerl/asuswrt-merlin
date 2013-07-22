#ifndef LINUX_VERSION_H
#define LINUX_VERSION_H

#ifdef HAVE_LINUX_VERSION_H
# include <linux/version.h>
#endif

#ifndef KERNEL_VERSION
# define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif

int get_linux_version(void);

#endif /* LINUX_VERSION_H */
