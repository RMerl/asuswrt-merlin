#! /bin/sh
# $Id: iptables_init.sh,v 1.5 2011/05/16 12:11:37 nanard Exp $
IPTABLES="`which iptables`" || exit 1
IP="`which ip`" || exit 1

#change this parameters :
#EXTIF=eth0
EXTIF="`LC_ALL=C $IP -4 route | grep 'default' | sed -e 's/.*dev[[:space:]]*//' -e 's/[[:space:]].*//'`" || exit 1
EXTIP="`LC_ALL=C $IP -4 addr show $EXTIF | awk '/inet/ { print $2 }' | cut -d "/" -f 1`"

echo "External IP = $EXTIP"

#adding the MINIUPNPD chain for nat
$IPTABLES -t nat -N MINIUPNPD
#adding the rule to MINIUPNPD
#$IPTABLES -t nat -A PREROUTING -d $EXTIP -i $EXTIF -j MINIUPNPD
$IPTABLES -t nat -A PREROUTING -i $EXTIF -j MINIUPNPD

#adding the MINIUPNPD chain for mangle
$IPTABLES -t mangle -N MINIUPNPD
$IPTABLES -t mangle -A PREROUTING -i $EXTIF -j MINIUPNPD

#adding the MINIUPNPD chain for filter
$IPTABLES -t filter -N MINIUPNPD
#adding the rule to MINIUPNPD
$IPTABLES -t filter -A FORWARD -i $EXTIF ! -o $EXTIF -j MINIUPNPD

#adding the MINIUPNPD chain for nat
$IPTABLES -t nat -N MINIUPNPD-PCP-PEER
$IPTABLES -t nat -A POSTROUTING -o $EXTIF -j MINIUPNPD-PCP-PEER
