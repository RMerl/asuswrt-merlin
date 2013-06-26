/* 
   Unix SMB/CIFS implementation.
   SMB client library test program
   Copyright (C) Andrew Tridgell 1998
   Copyright (C) Richard Sharpe 2000
   Copyright (C) John Terpsra 2000
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "libsmbclient.h"
#include "get_auth_data_fn.h"

int global_id = 0;

void print_list_fn(struct print_job_info *pji)
{

  fprintf(stdout, "Print job: ID: %u, Prio: %u, Size: %lu, User: %s, Name: %s\n",
	  pji->id,
          pji->priority,
          (unsigned long) pji->size,
          pji->user,
          pji->name);

  global_id = pji->id;

}

int main(int argc, char *argv[])
{
  int err, fd, dh1, dh2, dh3, dsize, dirc;
  const char *file = "smb://samba/public/testfile.txt";
  const char *file2 = "smb://samba/public/testfile2.txt";
  char buff[256];
  char dirbuf[512];
  char *dirp;
  struct stat st1, st2;

  err = smbc_init(get_auth_data_fn,  10); /* Initialize things */

  if (err < 0) {

    fprintf(stderr, "Initializing the smbclient library ...: %s\n", strerror(errno));

  }

  if (argc > 1) {

    if ((dh1 = smbc_opendir(argv[1]))<1) {

      fprintf(stderr, "Could not open directory: %s: %s\n",
	      argv[1], strerror(errno));

      exit(1);

    }

    fprintf(stdout, "Directory handles: %u, %u, %u\n", dh1, dh2, dh3);

    /* Now, list those directories, but in funny ways ... */

    dirp = (char *)dirbuf;

    if ((dirc = smbc_getdents(dh1, (struct smbc_dirent *)dirp, 
			      sizeof(dirbuf))) < 0) {

      fprintf(stderr, "Problems getting directory entries: %s\n",
	      strerror(errno));

      exit(1);

    }

    /* Now, process the list of names ... */

    fprintf(stdout, "Directory listing, size = %u\n", dirc);

    while (dirc > 0) {

      dsize = ((struct smbc_dirent *)dirp)->dirlen;
      fprintf(stdout, "Dir Ent, Type: %u, Name: %s, Comment: %s\n",
	      ((struct smbc_dirent *)dirp)->smbc_type, 
	      ((struct smbc_dirent *)dirp)->name, 
	      ((struct smbc_dirent *)dirp)->comment);

      dirp += dsize;
      dirc -= dsize;

    }

    dirp = (char *)dirbuf;

    exit(1);

  }

  /* For now, open a file on a server that is hard coded ... later will
   * read from the command line ...
   */

  fd = smbc_open(file, O_RDWR | O_CREAT | O_TRUNC, 0666);

  if (fd < 0) {

    fprintf(stderr, "Creating file: %s: %s\n", file, strerror(errno));
    exit(0);

  }

  fprintf(stdout, "Opened or created file: %s\n", file);

  /* Now, write some date to the file ... */

  bzero(buff, sizeof(buff));
  strcpy(buff, "Some test data for the moment ...");

  err = smbc_write(fd, buff, sizeof(buff));

  if (err < 0) {
    
    fprintf(stderr, "writing file: %s: %s\n", file, strerror(errno));
    exit(0);

  }

  fprintf(stdout, "Wrote %lu bytes to file: %s\n",
          (unsigned long) sizeof(buff), buff);

  /* Now, seek the file back to offset 0 */

  err = smbc_lseek(fd, SEEK_SET, 0);

  if (err < 0) {

    fprintf(stderr, "Seeking file: %s: %s\n", file, strerror(errno));
    exit(0);

  }

  fprintf(stdout, "Completed lseek on file: %s\n", file);

  /* Now, read the file contents back ... */

  err = smbc_read(fd, buff, sizeof(buff));

  if (err < 0) {

    fprintf(stderr, "Reading file: %s: %s\n", file, strerror(errno));
    exit(0);

  }

  fprintf(stdout, "Read file: %s\n", buff);  /* Should check the contents */

  fprintf(stdout, "Now fstat'ing file: %s\n", file);

  err = smbc_fstat(fd, &st1);

  if (err < 0) {

    fprintf(stderr, "Fstat'ing file: %s: %s\n", file, strerror(errno));
    exit(0);

  }


  /* Now, close the file ... */

  err = smbc_close(fd);

  if (err < 0) {

    fprintf(stderr, "Closing file: %s: %s\n", file, strerror(errno));

  }

  /* Now, rename the file ... */

  err = smbc_rename(file, file2);

  if (err < 0) {

    fprintf(stderr, "Renaming file: %s to %s: %s\n", file, file2, strerror(errno));

  }

  fprintf(stdout, "Renamed file %s to %s\n", file, file2);

  /* Now, create a file and delete it ... */

  fprintf(stdout, "Now, creating file: %s so we can delete it.\n", file);

  fd = smbc_open(file, O_RDWR | O_CREAT, 0666);

  if (fd < 0) {

    fprintf(stderr, "Creating file: %s: %s\n", file, strerror(errno));
    exit(0);

  }

  fprintf(stdout, "Opened or created file: %s\n", file);

  err = smbc_close(fd);

  if (err < 0) {

    fprintf(stderr, "Closing file: %s: %s\n", file, strerror(errno));
    exit(0);

  }
  
  /* Now, delete the file ... */

  fprintf(stdout, "File %s created, now deleting ...\n", file);

  err = smbc_unlink(file);

  if (err < 0) {

    fprintf(stderr, "Deleting file: %s: %s\n", file, strerror(errno));
    exit(0);

  }

  /* Now, stat the file, file 2 ... */

  fprintf(stdout, "Now stat'ing file: %s\n", file);

  err = smbc_stat(file2, &st2);

  if (err < 0) {

    fprintf(stderr, "Stat'ing file: %s: %s\n", file, strerror(errno));
    exit(0);

  }

  fprintf(stdout, "Stat'ed file:   %s. Size = %d, mode = %04X\n", file2, 
	  (int)st2.st_size, st2.st_mode);
  fprintf(stdout, "    time: %s\n", ctime(&st2.st_atime));
  fprintf(stdout, "Earlier stat:   %s, Size = %d, mode = %04X\n", file, 
	  (int)st1.st_size, st1.st_mode);
  fprintf(stdout, "    time: %s\n", ctime(&st1.st_atime));

  /* Now, make a directory ... */

  fprintf(stdout, "Making directory smb://samba/public/make-dir\n");

  if (smbc_mkdir("smb://samba/public/make-dir", 0666) < 0) {

    fprintf(stderr, "Error making directory: smb://samba/public/make-dir: %s\n", 
	    strerror(errno));

    if (errno == EEXIST) { /* Try to delete the directory */

      fprintf(stdout, "Trying to delete directory: smb://samba/public/make-dir\n");

      if (smbc_rmdir("smb://samba/public/make-dir") < 0) { /* Error */

	fprintf(stderr, "Error removing directory: smb://samba/public/make-dir: %s\n", strerror(errno));

	exit(0);

      }

      fprintf(stdout, "Making directory: smb://samba/public/make-dir\n");

      if (smbc_mkdir("smb://samba/public/make-dir", 666) < 0) {

	fprintf(stderr, "Error making directory: smb://samba/public/make-dir: %s\n",
		strerror(errno));

	fprintf(stderr, "I give up!\n");

	exit(1);

      }

    }

    exit(0);
    
  }

  fprintf(stdout, "Made dir: make-dir\n");
  return 0;
}
