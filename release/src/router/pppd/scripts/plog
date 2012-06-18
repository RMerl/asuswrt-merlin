#!/bin/sh

if [ -s /var/log/ppp.log ]; then
  exec tail "$@" /var/log/ppp.log
else
  exec tail "$@" /var/log/syslog | grep ' \(pppd\|chat\)\['
fi
