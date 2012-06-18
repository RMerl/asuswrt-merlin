/* 
   Unix SMB/Netbios implementation.
   Version 1.9.

   Functions to create reasonable random numbers for crypto use.

   Copyright (C) Jeremy Allison 1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"

extern int DEBUGLEVEL;
static uint32 counter = 0;

/****************************************************************
get a 16 byte hash from the contents of a file
Note that the hash is not initialised.
*****************************************************************/
static void do_filehash(char *fname, unsigned char *hash)
{
	unsigned char buf[1011]; /* deliberate weird size */
	unsigned char tmp_md4[16];
	int fd, n;

	fd = sys_open(fname,O_RDONLY,0);
	if (fd == -1) return;

	while ((n = read(fd, (char *)buf, sizeof(buf))) > 0) {
		mdfour(tmp_md4, buf, n);
		for (n=0;n<16;n++)
			hash[n] ^= tmp_md4[n];
	}
	close(fd);
}



/****************************************************************
 Try and get a seed by looking at the atimes of files in a given
 directory. XOR them into the buf array.
*****************************************************************/

static void do_dirrand(char *name, unsigned char *buf, int buf_len)
{
  DIR *dp = opendir(name);
  pstring fullname;
  int len_left;
  int fullname_len;
  char *pos;

  pstrcpy(fullname, name);
  fullname_len = strlen(fullname);

  if(fullname_len + 2 > sizeof(pstring))
    return;

  if(fullname[fullname_len] != '/') {
    fullname[fullname_len] = '/';
    fullname[fullname_len+1] = '\0';
    fullname_len = strlen(fullname);
  }

  len_left = sizeof(pstring) - fullname_len - 1;
  pos = &fullname[fullname_len];

  if(dp != NULL) {
    char *p;

    while ((p = readdirname(dp))) {           
      SMB_STRUCT_STAT st;

      if(strlen(p) <= len_left)
        pstrcpy(pos, p);

      if(sys_stat(fullname,&st) == 0) {
        SIVAL(buf, ((counter * 4)%(buf_len-4)), 
              IVAL(buf,((counter * 4)%(buf_len-4))) ^ st.st_atime);
        counter++;
        DEBUG(10,("do_dirrand: value from file %s.\n", fullname));
      }
    }
    closedir(dp); 
  }
}

/**************************************************************
 Try and get a good random number seed. Try a number of
 different factors. Firstly, try /dev/urandom and try and
 read from this. If this fails iterate through /tmp and
 /dev and XOR all the file timestamps. Next add in
 a hash of the contents of /etc/shadow and the smb passwd
 file and a combination of pid and time of day (yes I know this
 sucks :-). Finally md4 the result.

 We use /dev/urandom as a read of /dev/random can block if
 the entropy pool dries up. This leads clients to timeout
 or be very slow on connect.

 The result goes in a 16 byte buffer passed from the caller
**************************************************************/

static uint32 do_reseed(unsigned char *md4_outbuf)
{
  unsigned char md4_inbuf[40];
  BOOL got_random = False;
  uint32 v1, v2, ret;
  int fd;
  struct timeval tval;
  pid_t mypid;
  struct passwd *pw;

  memset(md4_inbuf, '\0', sizeof(md4_inbuf));

  fd = sys_open( "/dev/urandom", O_RDONLY,0);
  if(fd >= 0) {
    /* 
     * We can use /dev/urandom !
     */
    if(read(fd, md4_inbuf, 40) == 40) {
      got_random = True;
      DEBUG(10,("do_reseed: got 40 bytes from /dev/urandom.\n"));
    }
    close(fd);
  }

  if(!got_random) {
    /*
     * /dev/urandom failed - try /dev for timestamps.
     */
    do_dirrand("/dev", md4_inbuf, sizeof(md4_inbuf));
  }

  /* possibly add in some secret file contents */
  do_filehash("/etc/shadow", &md4_inbuf[0]);
  do_filehash(lp_smb_passwd_file(), &md4_inbuf[16]);

  /* add in the root encrypted password. On any system where security is taken
     seriously this will be secret */
  pw = sys_getpwnam("root");
  if (pw && pw->pw_passwd) {
	  int i;
	  unsigned char md4_tmp[16];
	  mdfour(md4_tmp, (unsigned char *)pw->pw_passwd, strlen(pw->pw_passwd));
	  for (i=0;i<16;i++)
		  md4_inbuf[8+i] ^= md4_tmp[i];
  }

  /*
   * Finally add the counter, time of day, and pid.
   */
  GetTimeOfDay(&tval);
  mypid = getpid();
  v1 = (counter++) + mypid + tval.tv_sec;
  v2 = (counter++) * mypid + tval.tv_usec;

  SIVAL(md4_inbuf, 32, v1 ^ IVAL(md4_inbuf, 32));
  SIVAL(md4_inbuf, 36, v2 ^ IVAL(md4_inbuf, 36));

  mdfour(md4_outbuf, md4_inbuf, sizeof(md4_inbuf));

  /*
   * Return a 32 bit int created from XORing the
   * 16 bit return buffer.
   */

  ret = IVAL(md4_outbuf, 0);
  ret ^= IVAL(md4_outbuf, 4);
  ret ^= IVAL(md4_outbuf, 8);
  return (ret ^ IVAL(md4_outbuf, 12));
}

/*******************************************************************
 Interface to the (hopefully) good crypto random number generator.
********************************************************************/

void generate_random_buffer( unsigned char *out, int len, BOOL re_seed)
{
  static BOOL done_reseed = False;
  static unsigned char md4_buf[16];
  unsigned char tmp_buf[16];
  unsigned char *p;

  if(!done_reseed || re_seed) {
	  sys_srandom(do_reseed(md4_buf));
	  done_reseed = True;
  }

  /*
   * Generate random numbers in chunks of 64 bytes,
   * then md4 them & copy to the output buffer.
   * Added XOR with output from random, seeded
   * by the original md4_buf. This is to stop the
   * output from this function being the previous
   * md4_buf md4'ed. The output from this function
   * is often output onto the wire, and so it should
   * not be possible to guess the next output from
   * this function based on the previous output.
   * XORing in the output from random(), seeded by
   * the original md4 hash should stop this. JRA.
   */

  p = out;
  while(len > 0) {
    int i;
    int copy_len = len > 16 ? 16 : len;
    mdfour(tmp_buf, md4_buf, sizeof(md4_buf));
    memcpy(md4_buf, tmp_buf, sizeof(md4_buf));
    /* XOR in output from random(). */
    for(i = 0; i < 4; i++)
      SIVAL(tmp_buf, i*4, (IVAL(tmp_buf, i*4) ^ (uint32)sys_random()));
    memcpy(p, tmp_buf, copy_len);
    p += copy_len;
    len -= copy_len;
  }
}
