#!/bin/bash
# AD-Bench user add/remove benchmark
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

ITERATIONS=100

source `dirname $0`/utils.sh

PRINCIPAL=$(get_principal $1)
PASSWORD=$(get_password $1)
REALM=$(get_realm $1)
NT_DOM=$(get_nt_dom $1)
SERVER=$2

add_user () {
	USER=$1
	${NET} ads user add "${USER}" 'Sup3rS3cr3T!' -k -s $CONFIG_FILE -S ${SERVER} > /dev/null
	RET=$?
	if [ $RET -ne 0 ]; then
		echo "${NET} ads user add returned error: $RET"
		exit 1
	fi
}

del_user () {
	USER=$1
	${NET} ads user delete "${USER}" -k -s $CONFIG_FILE -S ${SERVER} > /dev/null
	RET=$?
	if [ $RET -ne 0 ]; then
		echo "${NET} returned error: $RET"
		exit 1
	fi
}

enum_user () {
	${NET} ads user -k -s $CONFIG_FILE -S $SERVER > /dev/null
	RET=$?
	if [ $RET -ne 0 ]; then
		echo "${NET} returned error: $RET"
		exit 1
	fi
}

info_user () {
	USER=$1
	${NET} ads user info "${USER}" -k -s $CONFIG_FILE -S ${SERVER} > /dev/null
	RET=$?
	if [ $RET -ne 0 ]; then
		echo "${NET} returned error: $RET"
		exit 1
	fi
}

set_up () {
	set_krb_env
	setup_kinit
	call_kinit "${PRINCIPAL}" "${PASSWORD}"
	write_configfile "${REALM}" "${NT_DOM}"
}

tear_down () {
	${KDESTROY}
	restore_krb_env
}

set_up

echo -e "\tUSER $SERVER"

START_TIME=$(start_timer)

echo -en "\t"
for i in $( ${SEQ} 1 $ITERATIONS ); do
	USER=$( echo "ad_test_$(pad_number $i 3)" )
	add_user $USER
	echo -n "."
done
echo "done"

enum_user

# Requires winbind, which requires root perms to start. Skip this for now
#echo -en "\t"
#for i in $( ${SEQ} 1 $ITERATIONS ); do
#	USER=$( echo "ad_test_$(pad_number $i 3)" )
#	info_user $USER
#	echo -n "."
#done
#echo "done"

echo -en "\t"
for i in $( ${SEQ} 1 $ITERATIONS ); do
	USER=$( echo "ad_test_$(pad_number $i 3)" )
	del_user $USER
	echo -n "."
done
echo "done"

STOP_TIME=$(stop_timer)

TOTAL_TIME=$( total_time $START_TIME $STOP_TIME )

echo -e "\t\ttotal time:\t\t${TOTAL_TIME}s"

LOGINS_PER_MINUTE=$(iterations_per_minute $START_TIME $STOP_TIME $ITERATIONS)

echo -e "\t\titerations/min:\t\t$LOGINS_PER_MINUTE"

tear_down
