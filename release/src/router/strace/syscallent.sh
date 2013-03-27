#!/bin/sh
# Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
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

cat ${1+"$@"} |
	sed -n 's/^#[ 	]*define[ 	][ 	]*SYS_\([^ 	]*\)[ 	]*[^0-9]*\([0-9]*\).*$/\1 \2/p
s/^#[ 	]*define[ 	][ 	]*__NR_\([^ 	]*\)[ 	]*[^0-9]*\([0-9]*\).*$/\1 \2/p
s/^#[ ]*define[ ][ ]*__NR_\([^ ]*\)[ ]*[^0-9()]*(__NR_Linux + \([0-9]*\))$/\1 \2/p' |
	sort -k2n | uniq |
	awk '
	BEGIN {
		tabs = "\t\t\t\t\t\t\t\t"
		call = -1;
	}
	{
		while (++call < $2) {
			f = "printargs"
			n = "SYS_" call
			s = "\t{ -1,\t0,\t"
			s = s f ","
			s = s substr(tabs, 1, 24/8 - int((length(f) + 1)/8))
			s = s "\"" n "\""
			s = s substr(tabs, 1, 16/8 - int((length(n) + 2)/8))
			s = s "}, /* " call " */"
			print s
		}
		f = "sys_" $1
		n = $1
		s = "\t{ -1,\t0,\t"
		s = s f ","
		s = s substr(tabs, 1, 24/8 - int((length(f) + 1)/8))
		s = s "\"" n "\""
		s = s substr(tabs, 1, 16/8 - int((length(n) + 2)/8))
		s = s "}, /* " call " */"
		print s
	}
	END {
		limit = call + 100
		while (++call < limit) {
			f = "printargs"
			n = "SYS_" call
			s = "\t{ -1,\t0,\t"
			s = s f ","
			s = s substr(tabs, 1, 24/8 - int((length(f) + 1)/8))
			s = s "\"" n "\""
			s = s substr(tabs, 1, 16/8 - int((length(n) + 2)/8))
			s = s "}, /* " call " */"
			print s
		}
	}
	'
