/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   file read prediction routines
   Copyright (C) Andrew Tridgell 1992-1998
   
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

#if USE_READ_PREDICTION

/* variables used by the read prediction module */
static int rp_fd = -1;
static SMB_OFF_T rp_offset = 0;
static ssize_t rp_length = 0;
static ssize_t rp_alloced = 0;
static int rp_predict_fd = -1;
static SMB_OFF_T rp_predict_offset = 0;
static size_t rp_predict_length = 0;
static int rp_timeout = 5;
static time_t rp_time = 0;
static char *rp_buffer = NULL;
static BOOL predict_skip=False;
extern struct timeval smb_last_time;

/****************************************************************************
handle read prediction on a file
****************************************************************************/
ssize_t read_predict(int fd,SMB_OFF_T offset,char *buf,char **ptr,size_t num)
{
  ssize_t ret = 0;
  ssize_t possible = rp_length - (offset - rp_offset);

  possible = MIN(possible,num);

  /* give data if possible */
  if (fd == rp_fd && 
      offset >= rp_offset && 
      possible>0 &&
      smb_last_time.tv_secs - rp_time < rp_timeout)
  {
    ret = possible;
    if (buf)
      memcpy(buf,rp_buffer + (offset-rp_offset),possible);
    else
      *ptr = rp_buffer + (offset-rp_offset);
    DEBUG(5,("read-prediction gave %d bytes of %d\n",ret,num));
  }

  if (ret == num) {
    predict_skip = True;
  } else {
    SMB_STRUCT_STAT rp_stat;

    /* Find the end of the file - ensure we don't
       read predict beyond it. */
    if(sys_fstat(fd,&rp_stat) < 0)
    {
      DEBUG(0,("read-prediction failed on fstat. Error was %s\n", strerror(errno)));
      predict_skip = True;
    }
    else
    {
      predict_skip = False;

      /* prepare the next prediction */
      rp_predict_fd = fd;
      /* Make sure we don't seek beyond the end of the file. */
      rp_predict_offset = MIN((offset + num),rp_stat.st_size);
      rp_predict_length = num;
    }
  }

  if (ret < 0) ret = 0;

  return(ret);
}

/****************************************************************************
pre-read some data
****************************************************************************/
void do_read_prediction(void)
{
  static size_t readsize = 0;

  if (predict_skip) return;

  if (rp_predict_fd == -1) 
    return;

  rp_fd = rp_predict_fd;
  rp_offset = rp_predict_offset;
  rp_length = 0;

  rp_predict_fd = -1;

  if (readsize == 0) {
    readsize = lp_readsize();
    readsize = MAX(readsize,1024);
  }

  rp_predict_length = MIN(rp_predict_length,2*readsize);
  rp_predict_length = MAX(rp_predict_length,1024);
  rp_offset = (rp_offset/1024)*1024;
  rp_predict_length = (rp_predict_length/1024)*1024;

  if (rp_predict_length > rp_alloced)
    {
      rp_buffer = Realloc(rp_buffer,rp_predict_length);
      rp_alloced = rp_predict_length;
      if (!rp_buffer)
	{
	  DEBUG(0,("can't allocate read-prediction buffer\n"));
	  rp_predict_fd = -1;
	  rp_fd = -1;
	  rp_alloced = 0;
	  return;
	}
    }

  if (sys_lseek(rp_fd,rp_offset,SEEK_SET) != rp_offset) {
    rp_fd = -1;
    rp_predict_fd = -1;
    return;
  }

  rp_length = read(rp_fd,rp_buffer,rp_predict_length);
  rp_time = time(NULL);
  if (rp_length < 0)
    rp_length = 0;
}

/****************************************************************************
invalidate read-prediction on a fd
****************************************************************************/
void invalidate_read_prediction(int fd)
{
 if (rp_fd == fd) 
   rp_fd = -1;
 if (rp_predict_fd == fd)
   rp_predict_fd = -1;
}

#else
 void read_prediction_dummy(void) ;
#endif
