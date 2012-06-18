#!/bin/sh
# Emacs settings: -*- tab-width: 4 -*-
#
# Copyright (c) 2002-2006 Apple Computer, Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Linux /etc/init.d script to start/stop the mdnsd daemon.
#
# $Log: mdnsd.sh,v $
# Revision 1.9  2006/09/05 20:00:14  cheshire
# Moved Emacs settings to second line of file
#
# Revision 1.8  2006/08/29 16:42:01  mkrochma
# Fix POSIX startup script
#
# Revision 1.7  2006/08/14 23:24:47  cheshire
# Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0
#
# Revision 1.6  2004/12/07 20:30:45  cheshire
# Fix start-stop-daemon for Suse Linux (don't use -s TERM)
#
# Revision 1.5  2004/06/29 22:13:45  cheshire
# Fix from Andrew White at NICTA
#
# Revision 1.4  2004/02/05 20:23:10  cheshire
# Fix mdnsd.sh to work on *BSD distributions
#
# Revision 1.3  2004/01/19 22:47:17  cheshire
# Define killprocterm() to do "killproc $1 -TERM" for Linux
#
# Revision 1.2  2003/12/11 19:42:13  cheshire
# Change name "mDNSResponderd" to "mdnsd" for consistency with standard Linux (Unix) naming conventions
#
# Revision 1.1  2003/12/08 20:47:02  rpantos
# Add support for mDNSResponder on Linux.
#
# The following lines are used by the *BSD rcorder system to decide
# the order it's going to run the rc.d scripts at startup time.
# PROVIDE: mdnsd
# REQUIRE: NETWORKING

if [ -r /usr/sbin/mdnsd ]; then
    DAEMON=/usr/sbin/mdnsd
else
    DAEMON=/usr/local/sbin/mdnsd
fi

test -r $DAEMON || exit 0

# Some systems have start-stop-daemon, some don't. 
if [ -r /sbin/start-stop-daemon ]; then
	START="start-stop-daemon --start --quiet --exec"
	# Suse Linux doesn't work with symbolic signal names, but we really don't need
	# to specify "-s TERM" since SIGTERM (15) is the default stop signal anway
	# STOP="start-stop-daemon --stop -s TERM --quiet --oknodo --exec"
	STOP="start-stop-daemon --stop --quiet --oknodo --exec"
else
	killmdnsd() {
		kill -TERM `cat /var/run/mdnsd.pid`
	}
	START=
	STOP=killmdnsd
fi

case "$1" in
    start)
	echo -n "Starting Apple Darwin Multicast DNS / DNS Service Discovery daemon:"
	echo -n " mdnsd"
        $START $DAEMON
	echo "."
	;;
    stop)
        echo -n "Stopping Apple Darwin Multicast DNS / DNS Service Discovery daemon:"
        echo -n " mdnsd" ; $STOP $DAEMON
        echo "."
	;;
    reload|restart|force-reload)
		echo -n "Restarting Apple Darwin Multicast DNS / DNS Service Discovery daemon:"
		$STOP $DAEMON
		sleep 1
		$START $DAEMON
		echo -n " mdnsd"
	;;
    *)
	echo "Usage: /etc/init.d/mDNS {start|stop|reload|restart}"
	exit 1
	;;
esac

exit 0
