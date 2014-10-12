#!/bin/bash
# AD-Bench utility functions
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

source `dirname $0`/settings.sh

start_timer () {
	START_TIME=$( ${DATE} ${DATE_FORMATSTR} )
	echo "$START_TIME"
}

stop_timer () {
	STOP_TIME=$( ${DATE} ${DATE_FORMATSTR} )
	echo "$STOP_TIME"
}

total_time () {
	START_TIME=$1
	END_TIME=$2
	TOTAL_TIME=$( echo "scale=9;$STOP_TIME - $START_TIME" | ${BC} )
	echo "$TOTAL_TIME"
}

iterations_per_minute () {
	START_TIME=$1
	STOP_TIME=$2
	TOTAL_RUNS=$3

	TOTAL_TIME=$( total_time $START_TIME $STOP_TIME )

	ITER_PER_MIN=$( echo "scale=2; 60 * $TOTAL_RUNS / $TOTAL_TIME" | ${BC} )
	echo "$ITER_PER_MIN"
}

get_principal () {
	PRINCIPAL=$( echo $1 | ${SED} -e "s/\(.*\)%.*/\1/" )
	echo "$PRINCIPAL"
}

get_password () {
	PASSWORD=$( echo $1 | ${SED} -e "s/.*%\(.*\)/\1/" )
	echo "$PASSWORD"
}

get_realm () {
	REALM=$( echo $1 | ${SED} -e "s/.*@\(.*\)%.*/\1/" )
	echo "$REALM"
}

get_nt_dom () {
	NT_DOM=$( echo $1 | ${SED} -e "s/.*@\([A-Z1-9-]*\)\..*/\1/" )
	echo "$NT_DOM"
}

set_krb_env () {
	OLD_KRB5CCNAME="${KRB5CCNAME}"
	KRB5CCNAME="${NEW_KRB5CCNAME}"
	export KRB5CCNAME
}

restore_krb_env () {
	KRB5CCNAME="${OLD_KRB5CCNAME}"
	export KRB5CCNAME
}

setup_kinit () {
	${KINIT} --invalid 2>&1 | grep -q "password-file"
	if [ $? -eq 0 ]; then
		KINIT="${KINIT} ${KINIT_PARAM_OLD}"
	else
		KINIT="${KINIT} ${KINIT_PARAM_NEW}"
	fi
}

write_configfile () {
	REALM=$1
	NT_DOM=$2
	echo -e "[global]" > $CONFIG_FILE
	echo -e "\trealm = $REALM" >> $CONFIG_FILE
	echo -e "\tworkgroup = $NT_DOM" >> $CONFIG_FILE
	echo -e "\tsecurity = ADS" >> $CONFIG_FILE
}

call_kinit () {
	PRINCIPAL=$1
	PASSWORD=$2
	echo "${PASSWORD}" | ${KINIT} ${PRINCIPAL} > /dev/null
	RET=$?
	if [ $RET -ne 0 ]; then
		echo "kinit returned an error: $RET"
		exit 1
	fi
}

pad_number () {
	NUMBER=$1
	DIGITS=$2
	PADDED_NO=$(printf "%0${DIGITS}d" $NUMBER)
	echo $PADDED_NO
}
