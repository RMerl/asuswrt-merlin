
SUCCESS=0
FAILURE=1
exitval=$SUCCESS


KillProcess()
{
  proc=$1
  sig=$2

  # Determine PID of process(es) to stop and kill it.  This routine
  # is designed to work with bourne shell, ksh and posix shell.

  Command=`basename $proc | cut -c1-8`     # Solaris ps limited to 8 chars.

  pid=`ps -e | awk "\\$NF~/$Command/ {print \\$1}"`

  if [ "X$pid" != "X" ]; then
    kill -$sig $pid
  fi
}

if [ ! -f /etc/rc.config.d/samba ]
then
    echo "ERROR: Config file /etc/rc.config.d/samba missing."
    exit $FAILURE
fi

. /etc/rc.config.d/samba

case $1 in
    'start_msg')
        echo "Start Samba Services"
        ;;

    'stop_msg')
        echo "Stop Samba Services"
        ;;

    'start')
        # Starting Samba is easy ...
        if [ "$SAMBA_START" -eq 1 ]
        then
            if [ -x /opt/samba/bin/smbd ]
            then
                /opt/samba/bin/smbd -D -d $SAMBA_DEBUG
            fi

            if [ -x /opt/samba/bin/nmbd ]
            then
                /opt/samba/bin/nmbd -D -d $SAMBA_DEBUG
            fi
	fi
        ;;

    'stop')
        #
        # ... stopping it, however, is another story
        #
        KillProcess nmbd TERM
        KillProcess smbd TERM
        ;;

    *)
        echo "usage: $0 {start|stop|start_msg|stop_msg}"
        exitval=$FAILURE
        ;;
esac

exit $exitval

