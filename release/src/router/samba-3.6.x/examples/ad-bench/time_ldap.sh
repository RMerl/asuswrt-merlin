#!/bin/bash

ITERATIONS=100

source `dirname $0`/utils.sh

PRINCIPAL=$(get_principal $1)
PASSWORD=$(get_password $1)
REALM=$(get_realm $1)
NT_DOM=$(get_nt_dom $1)
SERVER=$2

search_users () {
	${NET} ads search '(objectCategory=user)' sAMAccountName -k -s $CONFIG_FILE -S ${SERVER} > /dev/null
	RET=$?
	if [ $RET -ne 0 ]; then
		echo "${NET} returned error: $RET"
		exit 1
	fi
}

search_groups () {
	${NET} ads search '(objectCategory=group)' sAMAccountName -k -s $CONFIG_FILE -S ${SERVER} > /dev/null
	if [ $RET -ne 0 ]; then
		echo "${NET} returned error: $RET"
		exit 1
	fi
}

search_computers () {
	${NET} ads search '(objectCategory=computer)' sAMAccountName -k -s $CONFIG_FILE -S ${SERVER} > /dev/null
	if [ $RET -ne 0 ]; then
		echo "${NET} returned error: $RET"
		exit 1
	fi
}

search_wildcard () {
	${NET} ads search '(objectCategory=*)' sAMAccountName -k -s $CONFIG_FILE -S ${SERVER} > /dev/null
	if [ $RET -ne 0 ]; then
		echo "${NET} returned error: $RET"
		exit 1
	fi
}

search_unindexed () {
	${NET} ads search '(description=Built-in account for adminstering the computer/domain)' sAMAccountName -k -s $CONFIG_FILE -S ${SERVER} > /dev/null
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

echo -e "\tSEARCH INDEXED $2"

START_TIME=$(start_timer)

echo -en "\t"
for i in $( ${SEQ} 1 $ITERATIONS ); do
	search_users
	search_groups
	search_computers
	echo -n "."
done
echo "done"

STOP_TIME=$(stop_timer)

TOTAL_TIME=$( total_time $START_TIME $STOP_TIME )

echo -e "\t\ttotal time:\t\t${TOTAL_TIME}s"

LOGINS_PER_MINUTE=$(iterations_per_minute $START_TIME $STOP_TIME $ITERATIONS)

echo -e "\t\titerations/min:\t\t$LOGINS_PER_MINUTE"

########################

echo -e "\tSEARCH WILDCARD $2"

START_TIME=$(start_timer)

echo -en "\t"
for i in $( ${SEQ} 1 $ITERATIONS ); do
	search_wildcard
	echo -n "."
done
echo "done"

STOP_TIME=$(stop_timer)

TOTAL_TIME=$( total_time $START_TIME $STOP_TIME )

echo -e "\t\ttotal time:\t\t${TOTAL_TIME}s"

LOGINS_PER_MINUTE=$(iterations_per_minute $START_TIME $STOP_TIME $ITERATIONS)

echo -e "\t\titerations/min:\t\t$LOGINS_PER_MINUTE"

########################

echo -e "\tSEARCH UNINDEXED $2"

START_TIME=$(start_timer)

echo -en "\t"
for i in $( ${SEQ} 1 $ITERATIONS ); do
	search_unindexed
	echo -n "."
done
echo "done"

STOP_TIME=$(stop_timer)

TOTAL_TIME=$( total_time $START_TIME $STOP_TIME )

echo -e "\t\ttotal time:\t\t${TOTAL_TIME}s"

LOGINS_PER_MINUTE=$(iterations_per_minute $START_TIME $STOP_TIME $ITERATIONS)

echo -e "\t\titerations/min:\t\t$LOGINS_PER_MINUTE"

tear_down
