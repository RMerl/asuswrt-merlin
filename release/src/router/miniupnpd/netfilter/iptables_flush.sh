#! /bin/sh
# $Id: iptables_flush.sh,v 1.4 2013/12/13 12:15:24 nanard Exp $
IPTABLES=/sbin/iptables

#flush all rules owned by miniupnpd
$IPTABLES -t nat -F MINIUPNPD
$IPTABLES -t nat -F MINIUPNPD-PCP-PEER
$IPTABLES -t filter -F MINIUPNPD
$IPTABLES -t mangle -F MINIUPNPD

