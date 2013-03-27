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

bad_includes='cg[48]var\.h|sys/spad\.h'
bad_defines='cg[48]var\.h|READSLICE|I_E_RECVFD|FBIOGPIXRECT|JTIMO|TTYTYPE|TIOCCONS|TCL_LINK|TCL_UNLINK'

(
	cd $1 || exit
	find sys -name '*.h' -print |
		xargs grep '^[	 ]*#[	 ]*define[	 ][ 	]*[A-Z_][A-Za-z0-9_]*[ 	][	 ]*( *[A-Za-z_][A-Za-z0-9_]* *| *[0-9][0-9]* *)' /dev/null |
		sed 's/\(.*\):#[ 	]*define[ 	]*\([A-Z_][A-Za-z0-9_]*\)[ 	]*\(([^)]*)\)[ 	]*\(.*\)/	{ "\1",	"\2",	\2	},	\4 \/**\//'
) >ioctlent.tmp
cat ioctlent.tmp |
	awk '{ print "#include <" substr($2, 2, length($2) - 3) ">" }' |
	sort -u |
	egrep -v "$bad_includes"
echo xyzzy
echo "struct ioctlent ioctlent[] = {"
egrep -v "$bad_defines" ioctlent.tmp |
awk '{ print "#ifdef " $4; print $0; print "#endif" }'
echo "};"
rm -f ioctlent.tmp
