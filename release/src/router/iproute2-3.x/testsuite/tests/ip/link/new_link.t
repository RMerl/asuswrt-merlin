#!/bin/sh

source lib/generic.sh

ts_log "[Testing add/del virtual links]"

NEW_DEV="$(rand_dev)"

ts_ip "$0" "Add $NEW_DEV dummy interface"  link add dev $NEW_DEV type dummy
ts_ip "$0" "Show $NEW_DEV dummy interface" link show dev $NEW_DEV
ts_ip "$0" "Del $NEW_DEV dummy interface"  link del dev $NEW_DEV
