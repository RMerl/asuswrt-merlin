#!/bin/bash
# AD-Bench Kerberos ticket benchmark
#
# Copyright (C) 2009  Kai Blin  <kai@samba.org>
#
# This file is part of AD-Bench, an Active Directory benchmark tool.
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

# Iterations are set per test, so more time-consuming tests can be run less
# often
ITERATIONS=100

source `dirname $0`/utils.sh

set_up () {
	set_krb_env
	setup_kinit
}

tear_down () {
	restore_krb_env
}

set_up

PRINCIPAL=$( get_principal $1)
PASSWORD=$( get_password $1)

echo -e "\tKINIT ${PRINCIPAL}"

START_TIME=$( start_timer )

echo -en "\t"
for i in $(${SEQ} 1 $ITERATIONS); do
	call_kinit "${PRINCIPAL}" "${PASSWORD}"
	${KDESTROY}
	echo -n "."
done
echo "done"

STOP_TIME=$( stop_timer )

TOTAL_TIME=$( total_time $START_TIME $STOP_TIME )

echo -e "\t\ttotal time:\t\t${TOTAL_TIME}s"

LOGINS_PER_MINUTE=$(iterations_per_minute $START_TIME $STOP_TIME $ITERATIONS)

echo -e "\t\titerations/min:\t\t$LOGINS_PER_MINUTE"

tear_down
