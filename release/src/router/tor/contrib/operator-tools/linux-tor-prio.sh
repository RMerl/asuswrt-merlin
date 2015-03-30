#!/bin/bash
# Written by Marco Bonetti & Mike Perry
# Based on instructions from Dan Singletary's ADSL BW Management HOWTO:
# http://www.faqs.org/docs/Linux-HOWTO/ADSL-Bandwidth-Management-HOWTO.html
# This script is Public Domain.

############################### README #################################

# This script provides prioritization of Tor traffic below other
# traffic on a Linux server. It has two modes of operation: UID based 
# and IP based. 

# UID BASED PRIORITIZATION
#
# The UID based method requires that Tor be launched from 
# a specific user ID. The "User" Tor config setting is
# insufficient, as it sets the UID after the socket is created.
# Here is a C wrapper you can use to execute Tor and drop privs before 
# it creates any sockets. 
#
# Compile with:
# gcc -DUID=`id -u tor` -DGID=`id -g tor` tor_wrap.c -o tor_wrap
#
# #include <unistd.h>
# int main(int argc, char **argv) {
# if(initgroups("tor", GID) == -1) { perror("initgroups"); return 1; }
# if(setresgid(GID, GID, GID) == -1) { perror("setresgid"); return 1; }
# if(setresuid(UID, UID, UID) == -1) { perror("setresuid"); return 1; }
# execl("/bin/tor", "/bin/tor", "-f", "/etc/tor/torrc", NULL);
# perror("execl"); return 1;
# }

# IP BASED PRIORITIZATION
#
# The IP setting requires that a separate IP address be dedicated to Tor. 
# Your Torrc should be set to bind to this IP for "OutboundBindAddress", 
# "ListenAddress", and "Address".

# GENERAL USAGE
#
# You should also tune the individual connection rate parameters below
# to your individual connection. In particular, you should leave *some* 
# minimum amount of bandwidth for Tor, so that Tor users are not 
# completely choked out when you use your server's bandwidth. 30% is 
# probably a reasonable choice. More is better of course.
#
# To start the shaping, run it as: 
#   ./linux-tor-prio.sh 
#
# To get status information (useful to verify packets are getting marked
# and prioritized), run:
#   ./linux-tor-prio.sh status
#
# And to stop prioritization:
#   ./linux-tor-prio.sh stop
#
########################################################################

# BEGIN USER TUNABLE PARAMETERS

DEV=eth0

# NOTE! You must START Tor under this UID. Using the Tor User
# config setting is NOT sufficient. See above.
TOR_UID=$(id -u tor)

# If the UID mechanism doesn't work for you, you can set this parameter
# instead. If set, it will take precedence over the UID setting. Note that
# you need multiple IPs with one specifically devoted to Tor for this to
# work.
#TOR_IP="42.42.42.42"

# Average ping to most places on the net, milliseconds
RTT_LATENCY=40

# RATE_UP must be less than your connection's upload capacity in
# kbits/sec. If it is larger, then the bottleneck will be at your
# router's queue, which you do not control. This will cause congestion
# and a revert to normal TCP fairness no matter what the queing
# priority is.
RATE_UP=5000

# RATE_UP_TOR is the minimum speed your Tor connections will have in
# kbits/sec.  They will have at least this much bandwidth for upload.
# In general, you probably shouldn't set this too low, or else Tor
# users who use your node will be completely choked out whenever your
# machine does any other network activity. That is not very fun.
RATE_UP_TOR=1500

# RATE_UP_TOR_CEIL is the maximum rate allowed for all Tor trafic in
# kbits/sec.
RATE_UP_TOR_CEIL=5000

CHAIN=OUTPUT
#CHAIN=PREROUTING
#CHAIN=POSTROUTING

MTU=1500
AVG_PKT=900 # should be more like 600 for non-exit nodes

# END USER TUNABLE PARAMETERS



# The queue size should be no larger than your bandwidth-delay
# product. This is RT latency*bandwidth/MTU/2

BDP=$(expr $RTT_LATENCY \* $RATE_UP / $AVG_PKT)

# Further research indicates that the BDP calculations should use
# RTT/sqrt(n) where n is the expected number of active connections..

BDP=$(expr $BDP / 4)

if [ "$1" = "status" ]
then
	echo "[qdisc]"
	tc -s qdisc show dev $DEV
	tc -s qdisc show dev imq0
	echo "[class]"
	tc -s class show dev $DEV
	tc -s class show dev imq0
	echo "[filter]"
	tc -s filter show dev $DEV
	tc -s filter show dev imq0
	echo "[iptables]"
	iptables -t mangle -L TORSHAPER-OUT -v -x 2> /dev/null
	exit
fi


# Reset everything to a known state (cleared)
tc qdisc del dev $DEV root 2> /dev/null > /dev/null
tc qdisc del dev imq0 root 2> /dev/null > /dev/null
iptables -t mangle -D POSTROUTING -o $DEV -j TORSHAPER-OUT 2> /dev/null > /dev/null
iptables -t mangle -D PREROUTING -o $DEV -j TORSHAPER-OUT 2> /dev/null > /dev/null
iptables -t mangle -D OUTPUT -o $DEV -j TORSHAPER-OUT 2> /dev/null > /dev/null
iptables -t mangle -F TORSHAPER-OUT 2> /dev/null > /dev/null
iptables -t mangle -X TORSHAPER-OUT 2> /dev/null > /dev/null
ip link set imq0 down 2> /dev/null > /dev/null
rmmod imq 2> /dev/null > /dev/null

if [ "$1" = "stop" ]
then
	echo "Shaping removed on $DEV."
	exit
fi

# Outbound Shaping (limits total bandwidth to RATE_UP)

ip link set dev $DEV qlen $BDP

# Add HTB root qdisc, default is high prio
tc qdisc add dev $DEV root handle 1: htb default 20

# Add main rate limit class
tc class add dev $DEV parent 1: classid 1:1 htb rate ${RATE_UP}kbit

# Create the two classes, giving Tor at least RATE_UP_TOR kbit and capping
# total upstream at RATE_UP so the queue is under our control.
tc class add dev $DEV parent 1:1 classid 1:20 htb rate $(expr $RATE_UP - $RATE_UP_TOR)kbit ceil ${RATE_UP}kbit prio 0
tc class add dev $DEV parent 1:1 classid 1:21 htb rate $[$RATE_UP_TOR]kbit ceil ${RATE_UP_TOR_CEIL}kbit prio 10

# Start up pfifo
tc qdisc add dev $DEV parent 1:20 handle 20: pfifo limit $BDP
tc qdisc add dev $DEV parent 1:21 handle 21: pfifo limit $BDP

# filter traffic into classes by fwmark
tc filter add dev $DEV parent 1:0 prio 0 protocol ip handle 20 fw flowid 1:20
tc filter add dev $DEV parent 1:0 prio 0 protocol ip handle 21 fw flowid 1:21

# add TORSHAPER-OUT chain to the mangle table in iptables
iptables -t mangle -N TORSHAPER-OUT
iptables -t mangle -I $CHAIN -o $DEV -j TORSHAPER-OUT


# Set firewall marks
# Low priority to Tor
if [ ""$TOR_IP == "" ]
then
	echo "Using UID-based QoS. UID $TOR_UID marked as low priority."
	iptables -t mangle -A TORSHAPER-OUT -m owner --uid-owner $TOR_UID -j MARK --set-mark 21
else
	echo "Using IP-based QoS. $TOR_IP marked as low priority."
	iptables -t mangle -A TORSHAPER-OUT -s $TOR_IP -j MARK --set-mark 21
fi

# High prio for everything else
iptables -t mangle -A TORSHAPER-OUT -m mark --mark 0 -j MARK --set-mark 20

echo "Outbound shaping added to $DEV.  Rate for Tor upload at least: ${RATE_UP_TOR}Kbyte/sec."

