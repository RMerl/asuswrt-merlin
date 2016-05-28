#!/bin/sh
#
# solaris_panic_action -- capture supporting information after a failure
#
ProgName=`basename $0`
LOGDIR=/usr/local/samba/var

main() {
	pid=$1

	if [ $# -lt 1 ]; then
		say "$ProgName error: you must supply a pid"
		say "Usage: $0 pid"
		exit 1
	fi
	cat >>$LOGDIR/log.solaris_panic_action <<!

`date`
State information and vountary core dump for process $pid

Related processes were:
`/usr/bin/ptree $pid`

Stack(s) were:
`/usr/bin/pstack $pid`

Flags were:
`/usr/bin/pflags $pid`

Credentials were:
`/usr/bin/pcred $pid`

Libraries used were:
`/usr/bin/pldd $pid`

Signal-handler settings were:
`/usr/bin/psig $pid`

Files and devices in use were:
`/usr/bin/pfiles $pid`

Directory in use was:
`/usr/bin/pwdx $pid`


A voluntary core dump was placed in /var/tmp/samba_solaris_panic_action_gcore.$pid
`gcore -o /var/tmp/samba_solaris_panic_action_gcore $pid`
!
}

say() {
	echo "$@" 1>&2
}

main "$@"
