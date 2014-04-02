#! /bin/sh
# $Id: iptables_display.sh,v 1.5 2013/12/13 12:15:24 nanard Exp $
IPTABLES=/sbin/iptables

#display all chains relative to miniupnpd
$IPTABLES -v -n -t nat -L PREROUTING
$IPTABLES -v -n -t nat -L MINIUPNPD
$IPTABLES -v -n -t nat -L POSTROUTING
$IPTABLES -v -n -t nat -L MINIUPNPD-PCP-PEER
$IPTABLES -v -n -t mangle -L MINIUPNPD
$IPTABLES -v -n -t filter -L FORWARD
$IPTABLES -v -n -t filter -L MINIUPNPD

