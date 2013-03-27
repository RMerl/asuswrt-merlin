#!/bin/sh
# Copyright (c) 1996 Rick Sladkey <jrs@world.std.com>
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

cat $* |
	sed -n -e 's/\/\*.*\*\// /' -e 's/^#[ 	]*define[ 	][ 	]*SIG\([^_ 	]*\)[ 	][ 	]*\([0-9][0-9]*\)[ 	]*$/\1 \2/p' |
	sort -k2n | uniq |
	awk '
	BEGIN {
		tabs = "\t\t\t\t\t\t\t\t"
		signal = -1;
	}
	$2 <= 256 {
		if (signal == $2)
			next
		while (++signal < $2) {
			n = "\"SIG_" signal "\""
			s = "\t" n ","
			s = s substr(tabs, 1, 16/8 - int((length(n) + 1)/8))
			s = s "/* " signal " */"
			print s
		}
		if (signal == $2)
			n = "\"SIG" $1 "\""
		n = "\"SIG" $1 "\""
		s = "\t" n ","
		s = s substr(tabs, 1, 16/8 - int((length(n) + 1)/8))
		s = s "/* " signal " */"
		print s
	}
	'
