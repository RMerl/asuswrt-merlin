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
#include "portable.h"
/**** ENDINCLUDE ****/

#ifdef HAVE_SYS_MOUNT_H
# include <sys/mount.h>
#endif
#ifdef HAVE_SYS_STATVFS_H
# include <sys/statvfs.h>
#endif
#ifdef HAVE_SYS_STATFS_H
# include <sys/statfs.h>
#endif
#if defined(HAVE_SYS_VFS_H) && !defined(SOLARIS)
# include <sys/vfs.h>
#endif

#ifdef SUNOS
extern int statfs(const char *, struct statfs *);
#endif

# if USE_STATFS_TYPE == STATVFS
#  define plp_statfs(path,buf) statvfs(path,buf)
#  define plp_struct_statfs struct statvfs
#  define statfs(path, buf) statvfs(path, buf)
#  define USING "STATVFS"
#  define BLOCKSIZE(f) (unsigned long)(f.f_frsize?f.frsize:f.f_bsize)
#  define BLOCKS(f)    (unsigned long)f.f_bavail
# endif

# if USE_STATFS_TYPE == ULTRIX_STATFS
#  define plp_statfs(path,buf) statfs(path,buf)
#  define plp_struct_statfs struct fs_data
#  define USING "ULTRIX_STATFS"
#  define BLOCKSIZE(f) (unsigned long)f.fd_bsize
#  define BLOCKS(f)    (unsigned long)f.fd_bfree
# endif

# if USE_STATFS_TYPE ==  SVR3_STATFS
#  define plp_struct_statfs struct statfs
#  define plp_statfs(path,buf) statfs(path,buf,sizeof(struct statfs),0)
#  define USING "SV3_STATFS"
#  define BLOCKSIZE(f) (unsigned long)f.f_bsize
#  define BLOCKS(f)    (unsigned long)f.f_bfree
# endif

# if USE_STATFS_TYPE == STATFS
#  define plp_struct_statfs struct statfs
#  define plp_statfs(path,buf) statfs(path,buf)
#  define USING "STATFS"
#  define BLOCKSIZE(f) (unsigned long)f.f_bsize
#  define BLOCKS(f)    (unsigned long)f.f_bavail
# endif

/***************************************************************************
 * Space_avail() - get the amount of free space avail in the spool directory
 ***************************************************************************/

int main( int argc, char *argv[], char *envp[] )
{
	char *pathname = argv[1];
	plp_struct_statfs fsb;
	unsigned long space = 0;

	if( !pathname ){
		pathname = ".";
	}
	if( plp_statfs( pathname, &fsb ) == -1 ){
		fprintf(stderr, "Space_avail: cannot stat '%s'", pathname );
		exit(1);
	}
	space = (1.0 * BLOCKS(fsb) * BLOCKSIZE(fsb))/1024;
	printf("path '%s', Using '%s', %lu blocks, %lu blocksize, space %lu\n",
	pathname, USING, BLOCKS(fsb), BLOCKSIZE(fsb), space );
	return(0);
}
