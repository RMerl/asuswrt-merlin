#! /bin/sh
# $Id: ip6tables_init_and_clean.sh,v 1.1 2012/04/24 22:13:41 nanard Exp $
# Improved Miniupnpd iptables init script.
# Checks for state of filter before doing anything..

EXTIF=eth0
IPTABLES=/sbin/ip6tables
FDIRTY="`LC_ALL=C /sbin/ip6tables -t filter -L -n | grep 'MINIUPNPD' | awk '{printf $1}'`"

if [[ $FDIRTY = "MINIUPNPDChain" ]]; then
        echo "Filter table dirty; Cleaning..."
        $IPTABLES -t filter -F MINIUPNPD
elif [[ $FDIRTY = "Chain" ]]; then
        echo "Dirty filter chain but no reference..? Fixsted."
        $IPTABLES -t filter -I FORWARD 4 -i $EXTIF ! -o $EXTIF -j MINIUPNPD
        $IPTABLES -t filter -F MINIUPNPD
else
        echo "Filter table clean..initalizing.."
        $IPTABLES -t filter -N MINIUPNPD
        $IPTABLES -t filter -I FORWARD 4 -i $EXTIF ! -o $EXTIF -j MINIUPNPD
fi

