#!/bin/sh
# 
# usage: rtpw_test <rtpw_commands>
# 
# tests the rtpw sender and receiver functions

RTPW=rtpw
DEST_PORT=9999
DURATION=3

key=2b2edc5034f61a72345ca5986d7bfd0189aa6dc2ecab32fd9af74df6dfc6

ARGS="-k $key -ae"

# First, we run "killall" to get rid of all existing rtpw processes.
# This step also enables this script to clean up after itself; if this
# script is interrupted after the rtpw processes are started but before
# they are killed, those processes will linger.  Re-running the script
# will get rid of them.

killall rtpw 2&>/dev/null

if test -x $RTPW; then

echo  $0 ": starting rtpw receiver process... "

$RTPW $* $ARGS -r 0.0.0.0 $DEST_PORT  &

receiver_pid=$!

echo $0 ": receiver PID = $receiver_pid"

sleep 1 

# verify that the background job is running
ps | grep -q $receiver_pid
retval=$?
echo $retval
if [ $retval != 0 ]; then
    echo $0 ": error"
    exit 254
fi

echo  $0 ": starting rtpw sender process..."

$RTPW $* $ARGS -s 127.0.0.1 $DEST_PORT  &

sender_pid=$!

echo $0 ": sender PID = $sender_pid"

# verify that the background job is running
ps | grep -q $sender_pid
retval=$?
echo $retval
if [ $retval != 0 ]; then
    echo $0 ": error"
    exit 255
fi

sleep $DURATION

kill $receiver_pid
kill $sender_pid

echo $0 ": done (test passed)"

else 

echo "error: can't find executable" $RTPW
exit 1

fi

# EOF


