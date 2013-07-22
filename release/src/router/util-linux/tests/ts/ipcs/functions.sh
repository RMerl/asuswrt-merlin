#!/bin/bash

#
# Copyright (C) 2007 Karel Zak <kzak@redhat.com>
#
# This file is part of util-linux.
#
# This file is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This file is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#

PAGE_SIZE=$($TS_HELPER_SYSINFO pagesize)

# kernel files
IPCS_PROCFILES=(
	/proc/sys/kernel/shmmni
	/proc/sys/kernel/shmall
	/proc/sys/kernel/shmmax
)

# raw data converted to ipcs-like format
#	shmmni = same
#	shmall = from pages to KBytes
#	shmmax = from bytes to KBytes
#
IPCS_KERNEL_CMD=(
	"cat /proc/sys/kernel/shmmni"
	"echo \$(cat /proc/sys/kernel/shmall) / 1024 \* $PAGE_SIZE | bc -l | sed 's/\..*//'"
	"echo \$(cat /proc/sys/kernel/shmmax) / 1024 | bc -l | sed 's/\..*//'"
)

# data from the ipcs command
IPCS_CMD=(
	"$TS_CMD_IPCS -m -l | awk '/max number of segments/ { print \$6 }'"
	"$TS_CMD_IPCS -m -l | awk '/max total shared memory/ { print \$7 }'"
	"$TS_CMD_IPCS -m -l | awk '/max seg size/ { print \$6 }'"
)


# The linux kernel accepts ULONG_MAX, but this value is same like ULLONG_MAX on
# 64-bit archs. So the ipcs command has to always overflow on 64-bit archs when
# shmall (=num of pages!) is same or almost same like ULONG_MAX. This is reason
# why we for the test uses 32-bit limits on all archs.
#
# (Don't worry that 64-bit ULONG_MAX makes ipcs useless ...
#  ... it's a problem for admins who want to use 75557863725TB of RAM for shm)
#
IPCS_LIMITS=(
	$($TS_HELPER_SYSINFO INT_MAX)
	$($TS_HELPER_SYSINFO ULONG_MAX32)
	$($TS_HELPER_SYSINFO ULONG_MAX32)
)

# list of indexes = 0..(sizeof Array - 1)
IPCS_IDX=$(seq 0 $(( ${#IPCS_PROCFILES[*]} - 1 )))

# checker
function ipcs_limits_check {
	for i in $IPCS_IDX; do
		echo -n ${IPCS_PROCFILES[$i]}

		a=$(eval ${IPCS_KERNEL_CMD[$i]})
		b=$(eval ${IPCS_CMD[$i]})

		#echo -n " RAW: "
		#cat ${IPCS_PROCFILES[$i]}
		#echo "CMD: ${ICPS_KERNEL_CMD[$i]}"

		if [ x"$a" == x"$b" ]; then
			echo " OK"
		else
			echo " kernel=$a, ipcs=$b"
		fi
	done
}

