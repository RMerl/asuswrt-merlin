#ifndef VSF_CMSG_EXTRAS_H
#define VSF_CMSG_EXTRAS_H

#include <sys/types.h>
#include <sys/socket.h>

/* These are from Linux glibc-2.2 */
#ifndef CMSG_ALIGN
#define CMSG_ALIGN(len) (((len) + sizeof (size_t) - 1) \
               & ~(sizeof (size_t) - 1))
#endif

#ifndef CMSG_SPACE
#define CMSG_SPACE(len) (CMSG_ALIGN (len) \
               + CMSG_ALIGN (sizeof (struct cmsghdr)))
#endif

#ifndef CMSG_LEN
#define CMSG_LEN(len)   (CMSG_ALIGN (sizeof (struct cmsghdr)) + (len))
#endif

#endif /* VSF_CMSG_EXTRAS_H */

