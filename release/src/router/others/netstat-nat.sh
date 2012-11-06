#!/bin/sh
MODE=`nvram get webui_resolve_conn`

if [ "$MODE" = "1" ]; then
   OPTS="-r state -x"
else
   OPTS="-r state -x -n"
fi

/usr/sbin/netstat-nat $OPTS
