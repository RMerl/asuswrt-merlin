/* Copyright 2007 Free Software Foundation, Inc.

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

   This file is part of the gdb testsuite.

   Contributed by Markus Deuling <deuling@de.ibm.com>.
   Tests for 'info spu' commands.  */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <spu_mfcio.h>


/* PPE-assisted call interface.  */
void
send_to_ppe (unsigned int signalcode, unsigned int opcode, void *data)
{
  __vector unsigned int stopfunc =
    {
      signalcode,     /* stop */
      (opcode << 24) | (unsigned int) data,
      0x4020007f,     /* nop */
      0x35000000      /* bi $0 */
    };

  void (*f) (void) = (void *) &stopfunc;
  asm ("sync");
  f ();
}

/* PPE-assisted call to mmap from SPU.  */
unsigned long long
mmap_ea (unsigned long long start, size_t length,
         int prot, int flags, int fd, off_t offset)
{
  struct mmap_args
    {
      unsigned long long start __attribute__ ((aligned (16)));
      size_t length __attribute__ ((aligned (16)));
      int prot __attribute__ ((aligned (16)));
      int flags __attribute__ ((aligned (16)));
      int fd __attribute__ ((aligned (16)));
      off_t offset __attribute__ ((aligned (16)));
    } args;

  args.start = start;
  args.length = length;
  args.prot = prot;
  args.flags = flags;
  args.fd = fd;
  args.offset = offset;

  send_to_ppe (0x2101, 11, &args);
  return args.start;
}

/* This works only in a Linux environment with <= 1024 open
   file descriptors for one process. Result is the file
   descriptor for the current context if available.  */
int
find_context_fd (void)
{
  int dir_fd = -1;
  int i;

  for (i = 0; i < 1024; i++)
    {
      struct stat stat;

      if (fstat (i, &stat) < 0)
        break;
      if (S_ISDIR (stat.st_mode))
        dir_fd = dir_fd == -1 ? i : -2;
    }
  return dir_fd < 0 ? -1 : dir_fd;
}

/* Open the context file and return the file handler.  */
int
open_context_file (int context_fd, char *name, int flags)
{
  char buf[128];

  if (context_fd < 0)
    return -1;

  sprintf (buf, "/proc/self/fd/%d/%s", context_fd, name);
  return open (buf, flags);
}


int
do_event_test ()
{
  spu_write_event_mask (MFC_MULTI_SRC_SYNC_EVENT); /* 0x1000 */  /* Marker Event */
  spu_write_event_mask (MFC_PRIV_ATTN_EVENT); /* 0x0800 */
  spu_write_event_mask (MFC_LLR_LOST_EVENT); /* 0x0400 */
  spu_write_event_mask (MFC_SIGNAL_NOTIFY_1_EVENT); /* 0x0200 */
  spu_write_event_mask (MFC_SIGNAL_NOTIFY_2_EVENT); /* 0x0100 */
  spu_write_event_mask (MFC_OUT_MBOX_AVAILABLE_EVENT); /* 0x0080 */
  spu_write_event_mask (MFC_OUT_INTR_MBOX_AVAILABLE_EVENT); /* 0x0040 */
  spu_write_event_mask (MFC_DECREMENTER_EVENT); /* 0x0020 */
  spu_write_event_mask (MFC_IN_MBOX_AVAILABLE_EVENT); /* 0x0010 */
  spu_write_event_mask (MFC_COMMAND_QUEUE_AVAILABLE_EVENT); /* 0x0008 */
  spu_write_event_mask (MFC_LIST_STALL_NOTIFY_EVENT); /* 0x0002 */
  spu_write_event_mask (MFC_TAG_STATUS_UPDATE_EVENT); /* 0x0001 */

  return 0;
}

int
do_dma_test ()
{
  #define MAP_FAILED      (-1ULL)
  #define PROT_READ       0x1
  #define MAP_PRIVATE     0x002
  #define BSIZE 128
  static char buf[BSIZE] __attribute__ ((aligned (128)));
  char *file = "/var/tmp/tmp_buf";
  struct stat fdstat;
  int fd, cnt;
  unsigned long long src;

  /* Create a file and fill it with some bytes.  */
  fd = open (file, O_CREAT | O_RDWR | O_TRUNC, 0777);
  if (fd == -1)
    return -1;
  memset ((void *)buf, '1', BSIZE);
  write (fd, buf, BSIZE);
  write (fd, buf, BSIZE);
  memset ((void *)buf, 0, BSIZE);

  if (fstat (fd, &fdstat) != 0
      || !fdstat.st_size)
    return -2;

  src = mmap_ea(0ULL, fdstat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (src == MAP_FAILED)
    return -3;

  /* Copy some data via DMA.  */
  mfc_get (&buf, src, BSIZE, 5, 0, 0);   /* Marker DMA */
  mfc_write_tag_mask (1<<5);   /* Marker DMAWait */
  spu_mfcstat (MFC_TAG_UPDATE_ALL);

  /* Close the file.  */
  close (fd);

  return cnt;
}

int
do_mailbox_test ()
{
  /* Write to SPU Outbound Mailbox.  */
  if (spu_stat_out_mbox ())            /* Marker Mbox */
    spu_write_out_mbox (0x12345678);

  /* Write to SPU Outbound Interrupt Mailbox.  */
  if (spu_stat_out_intr_mbox ())
    spu_write_out_intr_mbox (0x12345678);

  return 0;       /* Marker MboxEnd */
}

int
do_signal_test ()
{
  struct stat fdstat;
  int context_fd = find_context_fd ();
  int ret, buf, fd;

  buf = 23;    /* Marker Signal */
  /* Write to signal1.  */
  fd = open_context_file (context_fd, "signal1", O_RDWR);
  if (fstat (fd, &fdstat) != 0)
    return -1;
  ret = write (fd, buf, sizeof (int));
  close (fd);  /* Marker Signal1 */

  /* Write to signal2.  */
  fd = open_context_file (context_fd, "signal2", O_RDWR);
  if (fstat (fd, &fdstat) != 0)
    return -1;
  ret = write (fd, buf, sizeof (int));
  close (fd);  /* Marker Signal2 */

  /* Read signal1.  */
  if (spu_stat_signal1 ())
    ret = spu_read_signal1 ();

  /* Read signal2.  */
  if (spu_stat_signal2 ())
    ret = spu_read_signal2 ();   /* Marker SignalRead */

  return 0;
}

int
main (unsigned long long speid, unsigned long long argp, 
      unsigned long long envp)
{
  int res;

  /* info spu event  */
  res = do_event_test ();

  /* info spu dma  */
  res = do_dma_test ();

  /* info spu mailbox  */
  res = do_mailbox_test ();

  /* info spu signal  */
  res = do_signal_test ();

  return 0;
}

