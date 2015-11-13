# iptables for gpnrd
IPTABLES="/usr/sbin/iptables"
INTERNAL="br0"
EXTERNAL="eth0"
LOCAL="lo"

#Sanity Check
echo Where is $IPTABLES

echo Set up mangle-based gpnrd specific stuff
$IPTABLES -t mangle -N gpnrd
$IPTABLES -t mangle -A PREROUTING -p tcp -m socket -j gpnrd
$IPTABLES -t mangle -A PREROUTING -p udp -m socket -j gpnrd
$IPTABLES -t mangle -A gpnrd -j MARK --set-mark 1
$IPTABLES -t mangle -A gpnrd -j ACCEPT

ip rule add fwmark 1 lookup 100
ip route add local 0.0.0.0/0 dev lo table 100

echo configure capture rule
$IPTABLES -t mangle -A PREROUTING --in-interface $INTERNAL -p tcp --dport 4096:65535 -j TPROXY --tproxy-mark 0x1/0x1 --on-port 12345
$IPTABLES -t mangle -A PREROUTING --in-interface $INTERNAL -p udp --dport 4096:65535 -j TPROXY --tproxy-mark 0x1/0x1 --on-port 12345

