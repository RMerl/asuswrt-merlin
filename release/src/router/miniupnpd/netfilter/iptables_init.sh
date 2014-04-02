#! /bin/sh
# $Id: iptables_init.sh,v 1.7 2013/12/13 12:15:24 nanard Exp $
IPTABLES=/sbin/iptables

#change this parameters :
EXTIF=eth0
EXTIP="`LC_ALL=C /sbin/ifconfig $EXTIF | grep 'inet ' | awk '{print $2}' | sed -e 's/.*://'`"
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
