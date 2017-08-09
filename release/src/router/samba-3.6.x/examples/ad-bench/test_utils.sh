#!/bin/bash
# AD-Bench utility function tests
#
# Copyright (C) 2009  Kai Blin  <kai@samba.org>
#
# This file is part of AD-Bench, an Active Directory benchmark tool
#
# AD-Bench is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# AD-Bench is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with AD-Bench.  If not, see <http://www.gnu.org/licenses/>.


source `dirname $0`/utils.sh

INPUT="administrator@AD.EXAMPLE.COM%secret"
echo "Principal for $INPUT is " $( get_principal $INPUT )
echo "Password  for $INPUT is " $( get_password $INPUT )
echo "Realm     for $INPUT is " $( get_realm $INPUT )
echo "NT_DOM    for $INPUT is " $( get_nt_dom $INPUT )


echo "Padding 2: " $( pad_number 1 2 ) " 4: " $(pad_number 23 4)
