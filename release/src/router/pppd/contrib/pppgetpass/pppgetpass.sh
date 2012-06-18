#!/bin/sh

if [ -z "$DISPLAY" ]; then
  exec pppgetpass.vt "$@"
else
  exec pppgetpass.gtk "$@"
fi
