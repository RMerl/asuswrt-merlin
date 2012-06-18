#!/bin/sh

# $Id: chpasswd.sh,v 1.1 2008-05-19 13:08:34 winfred Exp $
# usage: chpasswd.sh <user name> [<password>]

usage()
{
	echo "Usage:"
	echo "  $0 <user name> [<password>]"
	exit 1
}

if [ "$1" == "" ]; then
	echo "$0: insufficient arguments"
	usage $0
fi

echo "$1:$2" > /tmp/tmpchpw
chpasswd < /tmp/tmpchpw
rm -f /tmp/tmpchpw
