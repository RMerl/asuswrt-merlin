#! /bin/sh
# Test suite for exclude.
# Copyright (C) 2009-2011 Free Software Foundation, Inc.
# This file is part of the GNUlib Library.
#
# This program is free software: you can redistribute it and/or modify
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
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

TMP=excltmp.$$
LIST=flist.$$
ERR=0

# Test exclude precedence

cat > $LIST <<EOT
foo*
bar
Baz
EOT

cat > $TMP <<EOT
bar: 1
bar: 0
EOT

./test-exclude$EXEEXT $LIST -include $LIST -- bar |
 tr -d '\015' >$TMP.1
./test-exclude$EXEEXT -include $LIST -no-include $LIST -- bar |
 tr -d '\015' >>$TMP.1

diff -c $TMP $TMP.1 || ERR=1

rm -f $TMP $TMP.1 $LIST
exit $ERR
