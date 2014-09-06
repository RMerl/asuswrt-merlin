#!/bin/sh
#
# snmpd-ucd.sh
#
# Start UCD SNMP daemon and trap catcher.  Backup the log file *first*
# since currently the daemon truncates and overwrites any pre-existing file.
#
# killproc() and pidofproc() lifted from Linux's /etc/init.d/functions.
#
# NOTE: Solaris users must uncomment the proper PSARGS definition below.  XXX
#

USAGE="Usage: `basename $0` start|stop|restart"



#------------------------------------ -o- 
# Globals.
#
DAEMONLOG=/var/log/snmpd.log
  TRAPLOG=/var/log/snmptrapd.log
   LOGDIR=/var/log/SNMPDLOGS

D=".`date '+%h%d_%H%M' | sed 's/\([a-z]\)0/\1/' | tr 'A-Z' 'a-z'`"

PSARGS=auwwx
#PSARGS=-ef		# Solaris.

DEBUGFLAG=		# -D	# Toggles use of debugging




#------------------------------------ -o- 
# Function definitions.
#
killproc() {	# <program> [signal]
	base= 
	killlevel="-9" 
	notset=1 
	pid=


	#
	# Parse.
	#
	[ $# = 0 ] && {
		echo "`basename $0`: Wrong arguments to killproc()." 1>&2
		return 1
	}
	base="`basename $1`"
	[ -n "$2" ] && {
		killlevel=$2
		notset=0
	}


	#
	# Kill process.
	#
        pid=`pidofproc $base 2>/dev/null`
	[ -z "$pid" ] && {
		pid=`ps $PSARGS | egrep $base | egrep -v egrep | egrep -v $0 | awk '{ print $2 }'`;
	}
	[ -z "$pid" ] && {
		echo "`basename $0`: killproc: Could not find process ID."
	}

        [ -n "$pid" ] && {
                echo -n "$base "

		#
		# Kill with -TERM then -KILL by default.  Use given
		# instead if one was passed in.
		#
		[ "$notset" = 1 ] && {
			kill -TERM $pid
			sleep 1

			[ -n "`ps $PSARGS |
					awk '{print $2}' | grep $pid`" ] && {
				sleep 3
				kill -KILL $pid
			}

			true
		} || {
	                kill $killlevel $pid
		}
	}

        rm -f /var/run/$base.pid

}  # end killproc()


pidofproc() {	# <program>
	pid=

	[ $# = 0 ] && {
		echo "`basename $0`: Wrong argument to pidofproc()."  1>&2
		return 1
	}

	#
	# Try looking for a /var/run file.
	#
	[ -f /var/run/$1.pid ] && {
	        pid=`head -1 /var/run/$1.pid`

	        [ -n "$pid" ] && {
	                echo $pid
	                return 0
		}
	}

	#
	# Try pidof.  (Linux offering.)
	#
	pid=`pidof $1`
	[ -n "$pid" ] && {
	        echo $pid
	        return 0
	}

	#
	# Try ps.
	#
	ps $PSARGS | awk '	BEGIN	{ prog=ARGV[1]; ARGC=1 } 
			{	if ((prog == $11) ||
					(("(" prog ")") == $11) ||
						((prog ":") == $11))
				{
					print $2
				}
			}' $1
}  # end pidofproc()



#------------------------------------ -o- 
# Action.
#
case "$1" in
  start)
	echo "Starting SNMP. "

	cp $DAEMONLOG ${DAEMONLOG}$D
	cp $TRAPLOG ${TRAPLOG}$D
	cat /dev/null >$TRAPLOG

	[ ! -e $LOGDIR ] && mkdir $LOGDIR
	mv ${DAEMONLOG}$D ${TRAPLOG}$D $LOGDIR
	gzip -r $LOGDIR	2>/dev/null &

	snmpd -a -d -V $DEBUGFLAG
	snmptrapd -Lf "$TRAPLOG"

	echo
	;;

  stop)
	echo -n "Shutting down SNMP: "

	killproc snmpd
	killproc snmptrapd

	echo
	;;

  restart)
        $0 stop
        $0 start
        ;;

  *)
	echo $USAGE	1>&2
	exit 1
esac



#------------------------------------ -o- 
#
exit 0


