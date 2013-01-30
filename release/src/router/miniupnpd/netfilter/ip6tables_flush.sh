#! /bin/sh
# $Id: ip6tables_flush.sh,v 1.1 2012/04/24 22:13:41 nanard Exp $
IPTABLES=/sbin/ip6tables

#flush all rules owned by miniupnpd
$IPTABLES -t filter -F MINIUPNPD

