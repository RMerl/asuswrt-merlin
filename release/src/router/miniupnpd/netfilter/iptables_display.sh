#! /bin/sh
# $Id: iptables_display.sh,v 1.3 2006/11/23 12:32:56 nanard Exp $
IPTABLES=iptables

#display all chains relative to miniupnpd
$IPTABLES -v -n -t nat -L PREROUTING
$IPTABLES -v -n -t nat -L MINIUPNPD
$IPTABLES -v -n -t filter -L FORWARD
$IPTABLES -v -n -t filter -L MINIUPNPD

