/*
  File: comm.c
  Desc: This file implements the actual transmission portion
  of the "ok to power me down" message to the remote
  power cycling black box.

  It's been sepatated into a separate file so that
  it may be replaced by any other comm mechanism desired.

  (including none or non serial mode at all)

  $Id: comm.c,v 1.3 2005/11/07 11:15:17 gleixner Exp $
  $Log: comm.c,v $
  Revision 1.3  2005/11/07 11:15:17  gleixner
  [MTD / JFFS2] Clean up trailing white spaces

  Revision 1.2  2001/06/21 23:07:18  dwmw2
  Initial import to MTD CVS

  Revision 1.1  2001/06/08 22:26:05  vipin
  Split the modbus comm part of the program (that sends the ok to pwr me down
  message) into another file "comm.c"

 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/*
  This is the routine that forms and
  sends the "ok to pwr me down" message
  to the remote power cycling "black box".

 */
int do_pwr_dn(int fd, int cycleCnt)
{

    char buf[200];

    sprintf(buf, "ok to power me down!\nCount = %i\n", cycleCnt);

    if(write(fd, buf, strlen(buf)) < strlen(buf))
    {
        perror("write error");
        return -1;
    }

    return 0;
}













