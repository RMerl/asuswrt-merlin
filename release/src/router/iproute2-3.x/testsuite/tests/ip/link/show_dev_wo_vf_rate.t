#!/bin/sh

source lib/generic.sh

NL_FILE="tests/ip/link/dev_wo_vf_rate.nl"
ts_ip "$0" "Show VF devices w/o VF rate info" -d monitor file $NL_FILE
