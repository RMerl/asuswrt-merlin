/*
 * util_funcs/get_pid_from_inode.h:  utilitiy function to retrieve the pid
 * that controls a given inode on linux.
 */
#ifndef NETSNMP_MIBGROUP_UTIL_FUNCS_GET_PID_FROM_INODE_H
#define NETSNMP_MIBGROUP_UTIL_FUNCS_HEADER_SIMPLE_TABLE_H

#ifndef linux
config_error(get_pid_from_inode is only suppored on linux)
#endif

#define _LARGEFILE64_SOURCE 1

#if HAVE_DIRENT_H
#include <dirent.h>
#else
# define dirent direct
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include <sys/types.h>

void netsnmp_get_pid_from_inode_init(void);
pid_t netsnmp_get_pid_from_inode(ino64_t);

#endif /* NETSNMP_MIBGROUP_UTIL_FUNCS_HEADER_SIMPLE_TABLE_H */
