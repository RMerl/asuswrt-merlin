#! /bin/sh
# $Id: iptables_flush.sh,v 1.3 2011/05/16 12:11:37 nanard Exp $
IPTABLES=/sbin/iptables

#flush all rules owned by miniupnpd
$IPTABLES -t nat -F MINIUPNPD
$IPTABLES -t filter -F MINIUPNPD

