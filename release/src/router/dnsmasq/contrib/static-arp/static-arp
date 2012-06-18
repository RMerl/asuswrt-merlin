#!/bin/sh

# Contributed by Darren Hoo <darren.hoo@gmail.com>

# If you use dnsmasq as DHCP server on a router, you may have
# met with attackers trying ARP Poison Routing (APR) on your
# local area network. This script will setup a 'permanent' entry
# in the router's ARP table upon each DHCP transaction so as to
# make the attacker's efforts less successful.

# Usage:
# edit /etc/dnsmasq.conf and specify the path of this script
# to  dhcp-script, for example:
#  dhcp-script=/usr/sbin/static-arp

# if $1 is add or old, update the static arp table entry.
# if $1 is del, then delete the entry from the table
# if $1 is init which is called by dnsmasq at startup, it's ignored

ARP=/usr/sbin/arp

# Arguments.
# $1 is action (add, del, old)
# $2 is MAC
# $3 is address
# $4 is hostname (optional, may be unset)

if [ ${1} = del ] ; then
         ${ARP} -d $3
fi

if [ ${1} = old ] || [ ${1} = add ] ; then
         ${ARP} -s $3 $2
fi

