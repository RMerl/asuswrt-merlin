#!/bin/sh
#
# Copyright (C) Guenther Deschner 2008
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.

tempdir=`mktemp -d /tmp/wb_padXXXXXX`
test -n "$tempdir" || exit 1
cat >> $tempdir/wb_pad.c << _EOF
#include "nsswitch/winbind_client.h"

int main(int argc, const char **argv)
{
	struct winbindd_request req;
	struct winbindd_response resp;

	if (argc != 2) {
		printf("usage: %s [req|resp]\n", argv[0]);
		return 0;
	}

	if (strcmp(argv[1], "req") == 0) {
		printf("%d\n", (uint32_t)sizeof(req));
	}
	if (strcmp(argv[1], "resp") == 0) {
		printf("%d\n", (uint32_t)sizeof(resp));
	}

	return 0;
}
_EOF

cleanup() {
	rm -f $tempdir/wb_pad_32 $tempdir/wb_pad_64 $tempdir/wb_pad.c
	rmdir $tempdir
}

cflags="-I. -I../ -I./../lib/replace -Iinclude"
${CC:-gcc} -m32 $RPM_OPT_FLAGS $CFLAGS -o $tempdir/wb_pad_32 $cflags $tempdir/wb_pad.c
if [ $? -ne 0 ]; then
	cleanup
	exit 1
fi
${CC:-gcc} -m64 $RPM_OPT_FLAGS $CFLAGS -o $tempdir/wb_pad_64 $cflags $tempdir/wb_pad.c
if [ $? -ne 0 ]; then
	cleanup
	exit 1
fi

out_64_req=`$tempdir/wb_pad_64 req`
out_64_resp=`$tempdir/wb_pad_64 resp`
out_32_req=`$tempdir/wb_pad_32 req`
out_32_resp=`$tempdir/wb_pad_32 resp`

cleanup

if test "$out_64_req" != "$out_32_req"; then
	echo "winbind request size differs!"
	echo "64bit: $out_64_req"
	echo "32bit: $out_32_req"
	exit 1
fi

if test "$out_64_resp" != "$out_32_resp"; then
	echo "winbind response size differs!"
	echo "64bit: $out_64_resp"
	echo "32bit: $out_32_resp"
	exit 1
fi

exit 0
