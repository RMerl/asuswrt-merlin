# iptables for gpnrd
IPTABLES="/usr/sbin/iptables"
INTERNAL="eth0"
EXTERNAL="eth1"
LOCAL="lo"

#Sanity Check
echo Where is $IPTABLES

echo configure capture rule
$IPTABLES -t mangle -D PREROUTING --in-interface $INTERNAL -p tcp --dport 4096:65535 -j TPROXY --tproxy-mark 0x1/0x1 --on-port 12345
$IPTABLES -t mangle -D PREROUTING --in-interface $INTERNAL -p udp --dport 4096:65535 -j TPROXY --tproxy-mark 0x1/0x1 --on-port 12345

echo Set up mangle-based gpnrd specific stuff
$IPTABLES -t mangle -D PREROUTING -p tcp -m socket -j gpnrd
$IPTABLES -t mangle -D PREROUTING -p udp -m socket -j gpnrd
$IPTABLES -t mangle -F gpnrd
$IPTABLES -t mangle -X gpnrd

echo clean up routes and marks
ip rule del fwmark 1 lookup 100
ip route del local 0.0.0.0/0 dev lo table 100



