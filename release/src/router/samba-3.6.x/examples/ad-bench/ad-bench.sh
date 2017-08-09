#!/bin/bash
# AD-Bench main program, runs all the benchmarks
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

if [ ! -f $RUNS ]; then
	echo "Error: please fill in $RUNS"
	echo "Sambple entries are"
	echo "user@REALM.EXAMPLE.COM%password:domain_controller"
	exit 1
fi

for run in `cat $RUNS`; do
	echo "START RUN"
	bash `dirname $0`/time_kinit.sh `echo $run|cut -d ":" -f 1`
	bash `dirname $0`/time_join.sh `echo $run|cut -d ":" -f 1` `echo $run|cut -d ":" -f 2`
	bash `dirname $0`/time_user.sh `echo $run|cut -d ":" -f 1` `echo $run|cut -d ":" -f 2`
	bash `dirname $0`/time_group.sh `echo $run|cut -d ":" -f 1` `echo $run|cut -d ":" -f 2`
	bash `dirname $0`/time_ldap.sh `echo $run|cut -d ":" -f 1` `echo $run|cut -d ":" -f 2`
	echo "END RUN"
done
