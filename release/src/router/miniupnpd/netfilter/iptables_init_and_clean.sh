#! /bin/sh
# $Id: iptables_init_and_clean.sh,v 1.1 2011/05/13 09:58:47 nanard Exp $
# Improved Miniupnpd iptables init script.
# Checks for state of filter before doing anything..

IPTABLES="`which iptables`" || exit 1
IP="`which ip`" || exit 1

#EXTIF=eth0
EXTIF="`LC_ALL=C $IP -4 route | grep 'default' | sed -e 's/.*dev[[:space:]]*//' -e 's/[[:space:]].*//'`" || exit 1
EXTIP="`LC_ALL=C $IP -4 addr show $EXTIF | awk '/inet/ { print $2 }' | cut -d "/" -f 1`"

NDIRTY="`LC_ALL=C $IPTABLES -t nat -L -n | awk '/MINIUPNPD/ {printf $1}'`"
FDIRTY="`LC_ALL=C $IPTABLES -t filter -L -n | awk '/MINIUPNPD/ {printf $1}'`"
echo "External IP = $EXTIP"

if [[ $NDIRTY = "MINIUPNPDChain" ]]; then
        echo "Nat table dirty; Cleaning..."
        $IPTABLES -t nat -F MINIUPNPD
elif [[ $NDIRTY = "Chain" ]]; then
        echo "Dirty NAT chain but no reference..? Fixsted."
        $IPTABLES -t nat -A PREROUTING -d $EXTIP -i $EXTIF -j MINIUPNPD
        $IPTABLES -t nat -F MINIUPNPD
else
        echo "NAT table clean..initalizing.."
        $IPTABLES -t nat -N MINIUPNPD
        $IPTABLES -t nat -A PREROUTING -d $EXTIP -i $EXTIF -j MINIUPNPD
fi
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

