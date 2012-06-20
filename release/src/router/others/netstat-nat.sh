#!/bin/sh
MODE=`nvram get webui_resolve_conn`

if [ "$MODE" = "1" ]; then
   OPTS="-r state"
else
   OPTS="-r state -n"
fi

/usr/sbin/netstat-nat $OPTS
