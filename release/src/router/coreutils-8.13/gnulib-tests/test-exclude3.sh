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

# Test include

cat > $LIST <<EOT
foo*
bar
Baz
EOT

cat > $TMP <<EOT
foo: 1
foo*: 0
bar: 0
foobar: 1
baz: 1
bar/qux: 1
EOT

./test-exclude$EXEEXT -include $LIST -- foo 'foo*' bar foobar baz bar/qux |
 tr -d '\015' |
 diff -c $TMP - || ERR=1

rm -f $TMP $LIST
exit $ERR
