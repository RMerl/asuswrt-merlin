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
#ifndef SG_LINUX_INC_H
#define SG_LINUX_INC_H

#ifdef SG_KERNEL_INCLUDES
  #define __user
  typedef unsigned char u8;
  #include "/usr/src/linux/include/scsi/sg.h"
  #include "/usr/src/linux/include/scsi/scsi.h"
#else
  #ifdef SG_TRICK_GNU_INCLUDES
    #include <linux/../scsi/sg.h>
    #include <linux/../scsi/scsi.h>
  #else
    #include <scsi/sg.h>
    #include <scsi/scsi.h>
  #endif
#endif

#ifdef BLKGETSIZE64
  #ifndef u64
    #include <stdint.h>   /* C99 header for exact integer types */
    typedef uint64_t u64; /* problems with BLKGETSIZE64 ioctl in lk 2.4 */
  #endif
#endif

/*
  Getting the correct include files for the sg interface can be an ordeal.
  In a perfect world, one would just write:
    #include <scsi/sg.h>
    #include <scsi/scsi.h>
  This would include the files found in the /usr/include/scsi directory.
  Those files are maintained with the GNU library which may or may not
  agree with the kernel and version of sg driver that is running. Any
  many cases this will not matter. However in some it might, for example
  glibc 2.1's include files match the sg driver found in the lk 2.2
  series. Hence if glibc 2.1 is used with lk 2.4 then the additional
  sg v3 interface will not be visible.
  If this is a problem then defining SG_KERNEL_INCLUDES will access the
  kernel supplied header files (assuming they are in the normal place).
  The GNU library maintainers and various kernel people don't like
  this approach (but it does work).
  The technique selected by defining SG_TRICK_GNU_INCLUDES worked (and
  was used) prior to glibc 2.2 . Prior to that version /usr/include/linux
  was a symbolic link to /usr/src/linux/include/linux .

  There are other approaches if this include "mixup" causes pain. These
  would involve include files being copied or symbolic links being
  introduced.

  Sorry about the inconvenience. Typically neither SG_KERNEL_INCLUDES
  nor SG_TRICK_GNU_INCLUDES is defined.

  dpg 20010415, 20030522
*/

#endif
