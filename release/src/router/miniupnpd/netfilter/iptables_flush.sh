#! /bin/sh
# $Id: iptables_flush.sh,v 1.2 2006/11/23 12:32:57 nanard Exp $
IPTABLES=iptables

#flush all rules owned by miniupnpd
$IPTABLES -t nat -F MINIUPNPD
$IPTABLES -t filter -F MINIUPNPD

