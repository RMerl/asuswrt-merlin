/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
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

