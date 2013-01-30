#! /bin/sh
# $Id: iptables_removeall.sh,v 1.6 2012/01/02 09:55:20 nanard Exp $
IPTABLES=/sbin/iptables

#change this parameters :
EXTIF=eth0
EXTIP="`LC_ALL=C /sbin/ifconfig $EXTIF | grep 'inet ' | awk '{print $2}' | sed -e 's/.*://'`"

#removing the MINIUPNPD chain for nat
$IPTABLES -t nat -F MINIUPNPD
#rmeoving the rule to MINIUPNPD
#$IPTABLES -t nat -D PREROUTING -d $EXTIP -i $EXTIF -j MINIUPNPD
$IPTABLES -t nat -D PREROUTING -i $EXTIF -j MINIUPNPD
$IPTABLES -t nat -X MINIUPNPD

#removing the MINIUPNPD chain for filter
$IPTABLES -t filter -F MINIUPNPD
#adding the rule to MINIUPNPD
$IPTABLES -t filter -D FORWARD -i $EXTIF ! -o $EXTIF -j MINIUPNPD
$IPTABLES -t filter -X MINIUPNPD

