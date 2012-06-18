#!/bin/sh
#
# Convert a Samba 1.9.18 smbpasswd file format into
# a Samba 2.0 smbpasswd file format.
# Read from stdin and write to stdout for simplicity.
# Set the last change time to 0x363F96AD to avoid problems
# with trying to work out how to get the seconds since 1970
# in awk or the shell. JRA.
#
nawk 'BEGIN {FS=":"} 
{
	if( $0 ~ "^#" ) {
		print $0
	} else {
		printf( "%s:%s:%s:%s:[U          ]:LCT-363F96AD:\n", $1, $2, $3, $4);
	}
}'
