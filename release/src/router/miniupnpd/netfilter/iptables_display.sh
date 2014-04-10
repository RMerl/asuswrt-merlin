#! /bin/sh
# $Id: iptables_display.sh,v 1.4 2011/05/16 12:11:37 nanard Exp $
IPTABLES=/sbin/iptables

#display all chains relative to miniupnpd
$IPTABLES -v -n -t nat -L PREROUTING
$IPTABLES -v -n -t nat -L MINIUPNPD
$IPTABLES -v -n -t nat -L POSTROUTING
$IPTABLES -v -n -t nat -L MINIUPNPD-PCP-PEER
$IPTABLES -v -n -t mangle -L MINIUPNPD
$IPTABLES -v -n -t filter -L FORWARD
$IPTABLES -v -n -t filter -L MINIUPNPD

