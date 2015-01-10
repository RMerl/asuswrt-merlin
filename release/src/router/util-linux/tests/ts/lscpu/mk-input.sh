#!/bin/bash
#
# Copyright (C) 2008-2009 Karel Zak <kzak@redhat.com>
#
# This script makes a copy of relevant files from /sys and /proc.
# The files are usefull for lscpu(1) regression tests.
#
progname=$(basename $0)

if [ -z "$1" ]; then
	echo -e "\nusage: $progname <testname>\n"
	exit 1
fi

TS_NAME="$1"
TS_DUMP="$TS_NAME"
CP="cp -r --parents"

mkdir -p $TS_DUMP/{proc,sys}

$CP /proc/cpuinfo $TS_DUMP

mkdir -p $TS_DUMP/proc/bus/pci
$CP /proc/bus/pci/devices $TS_DUMP

if [ -d "/proc/xen" ]; then
	mkdir -p $TS_DUMP/proc/xen
	if [ -f "/proc/xen/capabilities" ]; then
		$CP /proc/xen/capabilities $TS_DUMP
	fi
fi

if [ -e "/proc/sysinfo" ]; then
	$CP /proc/sysinfo $TS_DUMP
fi

$CP /sys/devices/system/cpu/* $TS_DUMP
$CP /sys/devices/system/node/*/cpumap $TS_DUMP

tar zcvf $TS_NAME.tar.gz $TS_DUMP
rm -rf $TS_DUMP


