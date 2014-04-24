#! /bin/sh
# $Id: iptables_removeall.sh,v 1.5 2011/05/16 12:11:37 nanard Exp $
IPTABLES="`which iptables`" || exit 1
IP="`which ip`" || exit 1

#change this parameters :
#EXTIF=eth0
EXTIF="`LC_ALL=C $IP -4 route | grep 'default' | sed -e 's/.*dev[[:space:]]*//' -e 's/[[:space:]].*//'`" || exit 1
EXTIP="`LC_ALL=C $IP -4 addr show $EXTIF | awk '/inet/ { print $2 }' | cut -d "/" -f 1`"

#removing the MINIUPNPD chain for nat
$IPTABLES -t nat -F MINIUPNPD
#rmeoving the rule to MINIUPNPD
#$IPTABLES -t nat -D PREROUTING -d $EXTIP -i $EXTIF -j MINIUPNPD
$IPTABLES -t nat -D PREROUTING -i $EXTIF -j MINIUPNPD
$IPTABLES -t nat -X MINIUPNPD

#removing the MINIUPNPD chain for mangle
$IPTABLES -t mangle -F MINIUPNPD
$IPTABLES -t mangle -D PREROUTING -i $EXTIF -j MINIUPNPD
$IPTABLES -t mangle -X MINIUPNPD

#removing the MINIUPNPD chain for filter
$IPTABLES -t filter -F MINIUPNPD
#adding the rule to MINIUPNPD
$IPTABLES -t filter -D FORWARD -i $EXTIF ! -o $EXTIF -j MINIUPNPD
$IPTABLES -t filter -X MINIUPNPD

#removing the MINIUPNPD-PCP-PEER chain for nat
$IPTABLES -t nat -F MINIUPNPD-PCP-PEER
#removing the rule to MINIUPNPD-PCP-PEER
$IPTABLES -t nat -D POSTROUTING -o $EXTIF -j MINIUPNPD-PCP-PEER
$IPTABLES -t nat -X MINIUPNPD-PCP-PEER
