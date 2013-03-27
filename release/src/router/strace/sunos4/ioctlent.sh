#!/bin/sh
# Copyright (c) 1993, 1994, 1995 Rick Sladkey <jrs@world.std.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#	$Id$

if [ $# -ne 1 ]
then
	echo "usage: $0 include-directory" >&2
	exit 1
fi

bad_defines='WINGETVALIDVALUES'
(
	cd $1
	find . -name '*.h' -print | sed 's/^\.\///' |
		xargs egrep '^[	 ]*#[	 ]*define[	 ][ 	]*[A-Z_][A-Za-z0-9_]*[ 	][	 ]*_IO[RW]?\(' /dev/null |
		sed 's/\(.*\):#[ 	]*define[ 	]*\([A-Z_][A-Za-z0-9_]*\)[ 	]*\(_IO[^)]*)\)[ 	]*\(.*\)/	{ "\1",	"\2",	\2	},	\4/' |
	sort -u
) >ioctlent.tmp
echo "\
#include <sys/types.h>
#define KERNEL
#include <stdio.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/des.h>
#include <sys/mtio.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/vcmd.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/route.h>
#include <net/nit.h>
#include <net/nit_if.h>
#include <net/nit_pf.h>
#include <net/nit_buf.h>
#include <net/packetfilt.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/if_ether.h>

#include <scsi/impl/uscsi.h>
#define sprintf scsi_sprintf
#include <scsi/scsi.h>
#undef sprintf
#include <scsi/targets/srdef.h>
#include <scsi/targets/stdef.h>
#if 0
#include <scsi/targets/sddef.h>
#endif

#include <sun/audioio.h>
#include <sun/fbio.h>
#include <sun/gpio.h>
#include <sun/ndio.h>
#include <sun/tvio.h>
#include <sun/mem.h>
#include <sun/sqz.h>
#include <sun/vddrv.h>
#include <sun/isdnio.h>

#include <machine/reg.h>

#include <sundev/kbio.h>
#include <sundev/msio.h>
#include <sundev/fdreg.h>
#include <sundev/ppreg.h>
#include <sundev/openpromio.h>
#include <sundev/lightpenreg.h>

#include <sunwindow/window_hs.h>
#include <sunwindow/win_enum.h>
#include <sunwindow/win_ioctl.h>

#include <sbusdev/audiovar.h>
#define AMD_CHIP
#include <sbusdev/audio_79C30.h>
#include <sbusdev/bpp_io.h>
#include <sbusdev/gtreg.h>

#include <sys/termio.h>
"
echo "struct ioctlent ioctlent[] = {"
egrep -v "$bad_defines" ioctlent.tmp | awk '
{
	print "#ifdef " $4
	print
	print "#endif"
}
'
echo "};"
rm -f ioctlent.tmp
