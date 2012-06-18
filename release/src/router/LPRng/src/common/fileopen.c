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
/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: fileopen.c,v 1.1.1.1 2008/10/15 03:28:26 james26_jang Exp $";


#include "lp.h"
#include "fileopen.h"
#include "errorcodes.h"
#include "child.h"
/**** ENDINCLUDE ****/

/***************************************************************************
  Commentary:
  Patrick Powell Mon May  1 05:37:02 PDT 1995
   
  These routines were created in order to centralize all file open
  and checking.  Hopefully,  if there are portability problems, these
  routines will be the only ones to change.
 
***************************************************************************/

/***************************************************************************
 * int Checkread( char *file, struct stat *statb )
 * open a file for reading,  and check its permissions
 * Returns: fd of open file, -1 if error.
 ***************************************************************************/

int Checkread( const char *file, struct stat *statb )
{
	int fd = -1;
	int status = 0;
	int err = 0;

	/* open the file */
	DEBUG3("Checkread: file '%s'", file );

	if( (fd = open( file, O_RDONLY|O_NOCTTY, 0 ) )< 0 ){
		Max_open(fd);
		status = -1;
		err = errno;
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUG3( "Checkread: cannot open '%s', %s", file, Errormsg(err) );
#endif
		memset( statb, 0, sizeof(struct stat) );
	}

    if( status >= 0 && fstat( fd, statb ) < 0 ) {
		err = errno;
        LOGERR(LOG_ERR)
		"Checkread: fstat of '%s' failed, possible security problem", file);
        status = -1;
    }

	/* check for a security loophole: not a file */
	if( status >= 0 && !(S_ISREG(statb->st_mode))){
		/* AHA!  not a regular file! */
		DEBUG3( "Checkread: '%s' not regular file, mode = 0%o",
			file, statb->st_mode );
		status = -1;
	}

	if( status < 0 && fd >= 0 ){
		close( fd );
		fd = -1;
	}

	DEBUG3("Checkread: '%s' fd %d, size %0.0f", file, fd, (double)(statb->st_size) );
	errno = err;
	return( fd );
}


/***************************************************************************
 * int Checkwrite( char *file, struct stat *statb, int rw, int create,
 *  int nodelay )
 *  - if rw != 0, open for both read and write
 *  - if create != 0, create if it does not exist
 *  - if nodelay != 0, use nonblocking open
 * open a file or device for writing,  and check its permissions
 * Returns: fd of open file, -1 if error.
 *     status in *statb
 ***************************************************************************/
int Checkwrite( const char *file, struct stat *statb, int rw, int create,
	int nodelay )
{
	int fd = -1;
	int status = 0;
	int options = O_NOCTTY|O_APPEND;
	int mask, oldumask;
	int err = errno;

	/* open the file */
	DEBUG3("Checkwrite: file '%s', rw %d, create %d, nodelay %d",
		file, rw, create, nodelay );

	memset( statb, 0, sizeof( statb[0] ) );
	if( nodelay ){
		options |= NONBLOCK;
	}
	if( rw ){
		options |= rw;
	} else {
		options |= O_WRONLY;
	}
	if( create ){
		options |= O_CREAT;
	}
	/* turn off umask */
	oldumask = umask( 0 ); 
	fd = open( file, options, Is_server?Spool_file_perms_DYN:0600 );
	Max_open(fd);
	err = errno;
	umask( oldumask );
	if( fd < 0 ){
		status = -1;
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUG3( "Checkwrite: cannot open '%s', %s", file, Errormsg(err) );
#endif
	} else if( nodelay ){
		/* turn off nonblocking */
		mask = fcntl( fd, F_GETFL, 0 );
		if( mask == -1 ){
			LOGERR(LOG_ERR) "Checkwrite: fcntl F_GETFL of '%s' failed", file);
			status = -1;
		} else if( mask & NONBLOCK ){
			DEBUG3( "Checkwrite: F_GETFL value '0x%x', BLOCK 0x%x",
				mask, NONBLOCK );
			mask &= ~NONBLOCK;
			mask = fcntl( fd, F_SETFL, mask );
			err = errno;
			DEBUG3( "Checkwrite: after F_SETFL value now '0x%x'",
				fcntl( fd, F_GETFL, 0 ) );
			if( mask == -1 && err != ENODEV ){
				errno = err;
				LOGERR(LOG_ERR) "Checkwrite: fcntl F_SETFL of '%s' failed",
					file );
				status = -1;
			}
		}
	}

    if( status >= 0 && fstat( fd, statb ) < 0 ) {
		err = errno;
        LOGERR_DIE(LOG_ERR) "Checkwrite: fstat of '%s' failed, possible security problem", file);
        status = -1;
    }

	/* check for a security loophole: not a file */
	if( status >= 0 && (S_ISDIR(statb->st_mode))){
		/* AHA!  Directory! */
		DEBUG3( "Checkwrite: '%s' directory, mode 0%o",
			file, statb->st_mode );
		status = -1;
	}
	if( fd == 0 ){
		int tfd;
		tfd = dup(fd);
		Max_open(tfd);
		err = errno;
		if( tfd < 0 ){
			LOGERR(LOG_ERR) "Checkwrite: dup of '%s' failed", file);
			status = -1;
		} else {
			close(fd);
			fd = tfd;
		}
    }
	if( status < 0 ){
		close( fd );
		fd = -1;
	}
	DEBUG2("Checkwrite: file '%s' fd %d, inode 0x%x, perms 0%o",
		file, fd, (int)(statb->st_ino), (int)(statb->st_mode) );
	errno = err;
	return( fd );
}

/***************************************************************************
 * int Checkwrite_timeout(int timeout, ... )
 *  Tries to do Checkwrite() with a timeout 
 ***************************************************************************/
int Checkwrite_timeout(int timeout,
	const char *file, struct stat *statb, int rw, int create, int nodelay )
{
	int fd;
	if( Set_timeout() ){
		Set_timeout_alarm( timeout );
		fd = Checkwrite( file, statb, rw, create, nodelay );
	} else {
		fd = -1;
	}
	Clear_timeout();
	return(fd);
}
