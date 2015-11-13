# iptables for haproxy and keepalived
IPTABLES="/usr/sbin/iptables"
INTERNAL="ens192"
EXTERNAL="ens224"
LOCAL="lo"

#Sanity Check
echo Where is $IPTABLES

echo firewall rules
echo "***************** filter **************"
$IPTABLES -nvL -t filter
echo "***************** nat **************"
$IPTABLES -nvL -t nat
echo "***************** mangle **************"
$IPTABLES -nvL -t mangle
echo
ip rule

