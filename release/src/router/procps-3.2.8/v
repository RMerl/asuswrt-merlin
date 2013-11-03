#!/bin/sh
#
# Wow, using $* causes great pain with embedded spaces in arguments.
# The "$@" won't break that into 2 arguments.
#
LD_LIBRARY_PATH=proc exec ./vmstat "$@"
