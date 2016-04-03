# Create test set
0 ipset create test hash:ip
# Check that iptables set match catches invalid number of dir parameters
2 iptables -m set --match-set test src,dst,src,dst,src,dst,src
# Check reference number of test set
0 ref=`ipset list test|grep References|sed 's/References: //'` && test $ref -eq 0
# Check that iptables SET target catches invalid number of dir parameters
2 iptables -j SET --add-set test src,dst,src,dst,src,dst,src
# Check reference number of test set
0 ref=`ipset list test|grep References|sed 's/References: //'` && test $ref -eq 0
# Destroy test set
0 ipset destroy test
# Create sets and inet rules which call set match and SET target
0 ./iptables.sh inet start
# Check that 10.255.255.64,tcp:1025 is not in ipport set
1 ipset test ipport 10.255.255.64,tcp:1025
# Send probe packet from 10.255.255.64,tcp:1025
0 sendip -p ipv4 -id 127.0.0.1 -is 10.255.255.64 -p tcp -td 80 -ts 1025 127.0.0.1
# Check that proper sets matched and target worked
0 ./check_klog.sh 10.255.255.64 tcp 1025 ipport list
# Check that 10.255.255.64,tcp:1025 is in ipport set now
0 ipset test ipport 10.255.255.64,tcp:1025
# Check that 10.255.255.64,udp:1025 is not in ipport set
1 ipset test ipport 10.255.255.64,udp:1025
# Send probe packet from 10.255.255.64,udp:1025
0 sendip -p ipv4 -id 127.0.0.1 -is 10.255.255.64 -p udp -ud 80 -us 1025 127.0.0.1
# Check that proper sets matched and target worked
0 ./check_klog.sh 10.255.255.64 udp 1025 ipport list
# Check that 10.255.255.64,udp:1025 is in ipport set now
0 ipset test ipport 10.255.255.64,udp:1025
# Check that 10.255.255.1,tcp:1025 is not in ipport set
1 ipset test ipport 10.255.255.1,tcp:1025
# Send probe packet from 10.255.255.1,tcp:1025
0 sendip -p ipv4 -id 127.0.0.1 -is 10.255.255.1 -p tcp -td 80 -ts 1025 127.0.0.1
# Check that proper sets matched and target worked
0 ./check_klog.sh 10.255.255.1 tcp 1025 ip1 list
# Check that 10.255.255.1,tcp:1025 is not in ipport set
1 ipset test ipport 10.255.255.1,tcp:1025
# Check that 10.255.255.32,tcp:1025 is not in ipport set
1 ipset test ipport 10.255.255.32,tcp:1025
# Send probe packet from 10.255.255.32,tcp:1025
0 sendip -p ipv4 -id 127.0.0.1 -is 10.255.255.32 -p tcp -td 80 -ts 1025 127.0.0.1
# Check that proper sets matched and target worked
0 ./check_klog.sh 10.255.255.32 tcp 1025 ip2
# Check that 10.255.255.32,tcp:1025 is not in ipport set
1 ipset test ipport 10.255.255.32,tcp:1025
# Check that 10.255.255.64,icmp:host-prohibited is not in ipport set
1 ipset test ipport 10.255.255.64,icmp:host-prohibited
# Send probe packet 10.255.255.64,icmp:host-prohibited
0 sendip -d r10 -p ipv4 -id 127.0.0.1 -is 10.255.255.64 -p icmp -ct 3 -cd 10 127.0.0.1
# Check that 10.255.255.64,icmp:3/10 is in ipport set now
0 ipset test ipport 10.255.255.64,icmp:host-prohibited
# Modify rules to check target and deletion
0 ./iptables.sh inet del
# Send probe packet 10.255.255.64,icmp:host-prohibited
0 sendip -d r10 -p ipv4 -id 127.0.0.1 -is 10.255.255.64 -p icmp -ct 3 -cd 10 127.0.0.1
# Check that 10.255.255.64,icmp:3/10 isn't in ipport
1 ipset test ipport 10.255.255.64,icmp:host-prohibited
# Destroy sets and rules
0 ./iptables.sh inet stop
# Create set and rules to check --exist and --timeout flags of SET target
0 ./iptables.sh inet timeout
# Add 10.255.255.64,icmp:host-prohibited to the set
0 ipset add test 10.255.255.64,icmp:host-prohibited
# Check that 10.255.255.64,icmp:3/10 is in ipport set
0 ipset test test 10.255.255.64,icmp:host-prohibited
# Sleep 3s so that entry can time out
0 sleep 3s
# Check that 10.255.255.64,icmp:3/10 is not in ipport set
1 ipset test test 10.255.255.64,icmp:host-prohibited
# Add 10.255.255.64,icmp:host-prohibited to the set again
0 ipset add test 10.255.255.64,icmp:host-prohibited
# Sleep 1s
0 sleep 1s
# Send probe packet 10.255.255.64,icmp:host-prohibited
0 sendip -d r10 -p ipv4 -id 127.0.0.1 -is 10.255.255.64 -p icmp -ct 3 -cd 10 127.0.0.1
# Sleep 5s, so original entry could time out
0 sleep 5s
# Check that 10.255.255.64,icmp:3/10 is not in ipport set
0 ipset test test 10.255.255.64,icmp:host-prohibited
# Destroy sets and rules
0 ./iptables.sh inet stop
# Create test set and iptables rules
0 ./iptables.sh inet mangle
# Send probe packet from 10.255.255.64,udp:1025
0 sendip -p ipv4 -id 127.0.0.1 -is 10.255.255.64 -p udp -ud 80 -us 1025 127.0.0.1
# Check that proper sets matched and target worked
0 ./check_klog.sh 10.255.255.64 udp 1025 mark
# Destroy sets and rules
0 ./iptables.sh inet stop
# Create test set and iptables rules
0 ./iptables.sh inet add
# Send probe packet from 10.255.255.64,udp:1025
0 sendip -p ipv4 -id 127.0.0.1 -is 10.255.255.64 -p udp -ud 80 -us 1025 127.0.0.1
# Check that 10.255.255.64 is added to the set
0 ipset t test 10.255.255.64
# Flush set
0 ipset f test
# Add a /24 network to the set
0 ipset a test 1.1.1.0/24
# Send probe packet from 10.255.255.64,udp:1025 again
0 sendip -p ipv4 -id 127.0.0.1 -is 10.255.255.64 -p udp -ud 80 -us 1025 127.0.0.1
# Check that 10.255.255.0/24 is added to the set
0 ipset t test 10.255.255.0/24
# Destroy sets and rules
0 ./iptables.sh inet stop
# Create set and rules for 0.0.0.0/0 check in hash:net,iface
0 ./iptables.sh inet netiface
# Send probe packet
0 sendip -p ipv4 -id 10.255.255.254 -is 10.255.255.64 -p udp -ud 80 -us 1025 10.255.255.254 >/dev/null 2>&1
# Check kernel log that the packet matched the set
0 ./check_klog.sh 10.255.255.64 udp 1025 netiface
# Destroy sets and rules
0 ./iptables.sh inet stop
# eof
