/*
    © Copyright 2004 Apple Computer, Inc. All rights reserved.

    IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. 
    (“Apple”) in consideration of your agreement to the following terms, and 
    your use, installation, modification or redistribution of this Apple software 
    constitutes acceptance of these terms.  If you do not agree with these terms, 
    please do not use, install, modify or redistribute this Apple software.

    In consideration of your agreement to abide by the following terms, and 
    subject to these terms, Apple grants you a personal, non-exclusive license, 
    under Apple’s copyrights in this original Apple software (the “Apple Software”), 
    to use, reproduce, modify and redistribute the Apple Software, with or without 
    modifications, in source and/or binary forms; provided that if you redistribute 
    the Apple Software in its entirety and without modifications, you must retain 
    this notice and the following text and disclaimers in all such redistributions 
    of the Apple Software.  Neither the name, trademarks, service marks or logos of 
    Apple Computer, Inc. may be used to endorse or promote products derived from the 
    Apple Software without specific prior written permission from Apple.  Except as 
    expressly stated in this notice, no other rights or licenses, express or 
    implied, are granted by Apple herein, including but not limited to any patent 
    rights that may be infringed by your derivative works or by other works in 
    which the Apple Software may be incorporated.

    The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES 
    NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED 
    WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
    PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN 
    COMBINATION WITH YOUR PRODUCTS. 

    IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR 
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE 
    GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
    ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR 
    DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY 
    OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN 
    IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 *  BPFLib.c
 *  BPFSample
 *
 *  Created by admin on Wed Oct 30 2002.
 *  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
 *
 *  The descriptions for these calls come from the NetBSD Programmer's Manual - enter
 *  "man bpf" in a terminal window.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <net/bpf.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include "bpflib.h"

#define BPF_FORMAT  "/dev/bpf%d"

/*
   bpf_set_timeout - Sets the read timeout parameter. The timeval specifies the length
   of time to wait before timing out a read request. This paramter is init'd to 0 by 
   open(2) indicating no timeout
   */
int
bpf_set_timeout(int fd, struct timeval * tv_p)
{
    return (ioctl(fd, BIOCSRTIMEOUT, tv_p));
}

/*
   bpf_get_data_link - Returns the type of the data link layer underlying the attached 
   interface.  EINVAL is returned if no inter-face has been specified.  The device types, 
   prefixed with ``DLT_'', are defined in <net/bpf.h>.

*/
int
bpf_get_data_link(int fd, u_int * dl_p)
{
    return (ioctl(fd, BIOCGDLT, dl_p));
}

/*
   bpf_get_blen - returns the required buffer length for reads on bpf files
   */
int 
bpf_get_blen(int fd, u_int * blen)
{
    int result = ioctl(fd, BIOCGBLEN, blen);

    printf("BIOCGBLEN: %d\n", *blen);

    return result;
}

/*
   bpf_set_promiscuous - Forces the interface into promiscuous mode.  All packets,
   not just those destined for the local host, are processed. Since more than one file 
   can be listening on a given interface, a listener that opened its interface non-
   promiscuously may receive packets promiscuously.  This problem can be remedied with 
   an appropriate filter.
   */
int
bpf_set_promiscuous(int bpf_fd)
{
  return(ioctl(bpf_fd, BIOCPROMISC, NULL));    
} 

/*
   bpf_get_stats - (struct bpf_stat) Returns the following structure of
   packet statistics:

   struct bpf_stat {
   u_int bs_recv;     // number of packets received
   u_int bs_drop;     // number of packets dropped 
   };

   The fields are:

   bs_recv the number of packets received by the
   descriptor since opened or reset (including
   any buffered since the last read call); and

   bs_drop the number of packets which were accepted by
   the filter but dropped by the kernel because
   of buffer overflows (i.e., the application's
   reads aren't keeping up with the packet
   traffic).
   */
int
bpf_get_stats(int bpf_fd, struct bpf_stat *bs_p)
{
    return ioctl(bpf_fd, BIOCGSTATS, bs_p);  
} 

int
bpf_dispose(int bpf_fd)
{
    if (bpf_fd >= 0)
        return (close(bpf_fd));
    return (0);
}

int
bpf_new()
{
    char bpfdev[256];
    int i;
    int fd;

    for (i = 0; i < 10; i++) 
    {
        struct stat sb;

        sprintf(bpfdev, BPF_FORMAT, i);
        if (stat(bpfdev, &sb) < 0)
            return -1;
        fd = open(bpfdev, O_RDWR , 0);
        if (fd < 0) {
            if (errno != EBUSY)
                return (-1);
        }
        else 
        {
            return (fd);
        }
    }
    return (-1);
}

/*
   bpf_setif - Sets the hardware interface associated with the file descriptor. this call
   must be made before any packets can be read. the device name is indicated by setting the
   ifr_name field of the ifreq. Also performs the actions of BIOCFLUSH
   */
int
bpf_setif(int fd, char * en_name)
{
    struct ifreq ifr;

    strcpy(ifr.ifr_name, en_name);
    return (ioctl(fd, BIOCSETIF, &ifr));
}

/*
   bpf_set_immediate - enable or disable immediate mode based on the truth value of argument
   value 0 - disable, not 0 - enable.
   When immediate mode is enabled, reads return immediately upon packet reception. Otherwise
   a read will block until either the kernel buffer becomes full or a timeout occurs. Default
   setting is off.
   */
int
bpf_set_immediate(int fd, u_int value)
{
    return (ioctl(fd, BIOCIMMEDIATE, &value));
}

int
bpf_filter_receive_none(int fd)
{
    struct bpf_insn insns[] = {
        BPF_STMT(BPF_RET+BPF_K, 0),
    };
    struct bpf_program prog;

    prog.bf_len = sizeof(insns) / sizeof(struct bpf_insn);
    prog.bf_insns = insns;
    return ioctl(fd, BIOCSETF, (u_int)&prog);
}

/*int
  bpf_arp_filter(int fd)
  {
  struct bpf_insn insns[] = {
  BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 12),
  BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, ETHERTYPE_ARP, 0, 3),
  BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 20),
  BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, ARPOP_REPLY, 0, 1),
  BPF_STMT(BPF_RET+BPF_K, sizeof(struct ether_arp) +
  sizeof(struct ether_header)),
  BPF_STMT(BPF_RET+BPF_K, 0),
  };
  struct bpf_program prog;

  prog.bf_len = sizeof(insns) / sizeof(struct bpf_insn);
  prog.bf_insns = insns;
  return ioctl(fd, BIOCSETF, (u_int)&prog);
  }*/

int
bpf_see_sent(int fd, int value) {
    //OpenBSD doesn't support BIOCSSEESENT, so don't call.
#ifndef OPENBSD
    return(ioctl(fd, BIOCSSEESENT, &value));
#endif /* OPENBSD */
}

int
bpf_write(int fd, void * pkt, int len)
{
    return (write(fd, pkt, len));
}

