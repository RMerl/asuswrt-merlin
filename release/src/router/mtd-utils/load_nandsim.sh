#!/bin/bash

#
# This script inserts NAND simulator module to emulate NAND flash of specified
# size.
#
# Author: Artem Bityutskiy
#

# Check if nandsim module is loaded
function nandsim_loaded()
{
	local NANDSIM=`lsmod | grep nandsim`
	if [ -n "$NANDSIM" ]; then
		return 1
	fi
	return 0
}

nandsim_loaded
if (( $? != 0 )); then
	echo "Error: nandsim is already loaded"
	exit 1
fi

if (( $# < 1 )); then
	echo "Load NAND simulator to simulate flash of a specified size."
	echo ""
	echo "Usage: ./load_nandsim.sh <size in MiB> <eraseblock size in KiB>"
	echo "       <page size (512 or 2048)>"
	echo ""
	echo "Only the first parameter is mandatory. Default eraseblock size"
	echo "is 16KiB, default NAND page size is 512 bytes."
	echo ""
	echo "Only the following combinations are supported:"
	echo "--------------------------------------------------"
	echo "| size (MiB) | EB size (KiB) | Page size (bytes) |"
	echo "--------------------------------------------------"
	echo "| 16         | 16            | 512               |"
	echo "| 32         | 16            | 512               |"
	echo "| 64         | 16            | 512               |"
	echo "| 128        | 16            | 512               |"
	echo "| 256        | 16            | 512               |"
	echo "| 64         | 64            | 2048              |"
	echo "| 64         | 128           | 2048              |"
	echo "| 64         | 256           | 2048              |"
	echo "| 64         | 512           | 2048              |"
	echo "| 128        | 64            | 2048              |"
	echo "| 128        | 128           | 2048              |"
	echo "| 128        | 256           | 2048              |"
	echo "| 128        | 512           | 2048              |"
	echo "| 256        | 64            | 2048              |"
	echo "| 256        | 128           | 2048              |"
	echo "| 256        | 256           | 2048              |"
	echo "| 256        | 512           | 2048              |"
	echo "| 512        | 64            | 2048              |"
	echo "| 512        | 128           | 2048              |"
	echo "| 512        | 256           | 2048              |"
	echo "| 512        | 512           | 2048              |"
	echo "| 1024       | 64            | 2048              |"
	echo "| 1024       | 128           | 2048              |"
	echo "| 1024       | 256           | 2048              |"
	echo "| 1024       | 512           | 2048              |"
	echo "--------------------------------------------------"
	exit 1
fi

SZ=$1
EBSZ=$2
PGSZ=$3
if [[ $# == '1' ]]; then
	EBSZ=16
	PGSZ=512
elif [[ $# == '2' ]]; then
	PGSZ=512
fi

if (( $PGSZ == 512 && $EBSZ != 16 )); then
	echo "Error: only 16KiB eraseblocks are possible in case of 512 bytes page"
	exit 1
fi

if (( $PGSZ == 512 )); then
	case $SZ in
	16)  modprobe nandsim first_id_byte=0x20 second_id_byte=0x33 ;;
	32)  modprobe nandsim first_id_byte=0x20 second_id_byte=0x35 ;;
	64)  modprobe nandsim first_id_byte=0x20 second_id_byte=0x36 ;;
	128) modprobe nandsim first_id_byte=0x20 second_id_byte=0x78 ;;
	256) modprobe nandsim first_id_byte=0x20 second_id_byte=0x71 ;;
	*) echo "Flash size ${SZ}MiB is not supported, try 16, 32, 64 or 256"
	   exit 1 ;;
	esac
elif (( $PGSZ == 2048 )); then
	case $EBSZ in
	64)  FOURTH=0x05 ;;
	128) FOURTH=0x15 ;;
	256) FOURTH=0x25 ;;
	512) FOURTH=0x35 ;;
	*)   echo "Eraseblock ${EBSZ}KiB is not supported"
	     exit 1
	esac

	case $SZ in
	64)  modprobe nandsim first_id_byte=0x20 second_id_byte=0xa2 third_id_byte=0x00 fourth_id_byte=$FOURTH ;;
	128) modprobe nandsim first_id_byte=0xec second_id_byte=0xa1 third_id_byte=0x00 fourth_id_byte=$FOURTH ;;
	256) modprobe nandsim first_id_byte=0x20 second_id_byte=0xaa third_id_byte=0x00 fourth_id_byte=$FOURTH ;;
	512) modprobe nandsim first_id_byte=0x20 second_id_byte=0xac third_id_byte=0x00 fourth_id_byte=$FOURTH ;;
	1024) modprobe nandsim first_id_byte=0xec second_id_byte=0xd3 third_id_byte=0x51 fourth_id_byte=$FOURTH ;;
	*) echo "Unable to emulate ${SZ}MiB flash with ${EBSZ}KiB eraseblock"
	   exit 1
	esac
else
	echo "Error: bad NAND page size ${PGSZ}KiB, it has to be either 512 or 2048"
	exit 1
fi

if (( $? != 0 )); then
	echo "Error: cannot load nandsim"
	exit 1
fi

echo "Loaded NAND simulator (${SZ}MiB, ${EBSZ}KiB eraseblock, $PGSZ bytes NAND page)"
exit 0
