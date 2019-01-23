# Create a set
0 ipset create test hash:net,iface hashsize 128
# Add zero valued element
0 ipset add test 0.0.0.0/0,eth0
# Test zero valued element
0 ipset test test 0.0.0.0/0,eth0
# Delete zero valued element
0 ipset del test 0.0.0.0/0,eth0
# Add 1.1.1.1/0
0 ipset add test 1.1.1.1/0,eth0
# Test 1.1.1.1/0
0 ipset test test 1.1.1.1/0,eth0
# Delete 1.1.1.1/0
0 ipset del test 1.1.1.1/0,eth0
# Try to add /32
0 ipset add test 1.1.1.1/32,eth0
# Add almost zero valued element
0 ipset add test 0.0.0.0/1,eth0
# Test almost zero valued element
0 ipset test test 0.0.0.0/1,eth0
# Delete almost zero valued element
0 ipset del test 0.0.0.0/1,eth0
# Test deleted element
1 ipset test test 0.0.0.0/1,eth0
# Delete element not added to the set
1 ipset del test 0.0.0.0/1,eth0
# Add first random network
0 ipset add test 2.0.0.1/24,eth0
# Add second random network
0 ipset add test 192.168.68.69/27,eth1
# Test first random value
0 ipset test test 2.0.0.255,eth0
# Test second random value
0 ipset test test 192.168.68.95,eth1
# Test value not added to the set
1 ipset test test 2.0.1.0,eth0
# Test value not added to the set
1 ipset test test 2.0.0.255,eth1
# Test value not added to the set
1 ipset test test 192.168.68.95,eth0
# Try to add IP address
0 ipset add test 2.0.0.1,eth0
# List set
0 ipset list test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -u -I 'Size in memory.*' .foo hash:net,iface.t.list0
# Flush test set
0 ipset flush test
# Delete test set
0 ipset destroy test
# Create test set
0 ipset new test hash:net,iface
# Add networks in range notation
0 ipset add test 10.2.0.0-10.2.1.12,eth0
# List set
0 ipset -L test 2>/dev/null | grep -v Revision: > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -u -I 'Size in memory.*' .foo hash:net,iface.t.list2
# Flush test set
0 ipset flush test
# Add 0/0,eth0
0 ipset add test 0/0,eth0
# Add 10.0.0.0/16,eth1
0 ipset add test 10.0.0.0/16,eth1
# Add 10.0.0.0/24,eth0
0 ipset add test 10.0.0.0/24,eth0
# Add 10.0.0.0/16,eth2
0 ipset add test 10.0.0.0/16,eth2
# Check 10.0.1.1 with eth1
0 ipset test test 10.0.1.1,eth1
# Check 10.0.1.1 with eth2
0 ipset test test 10.0.1.1,eth2
# Check 10.0.1.1 with eth0
1 ipset test test 10.0.1.1,eth0
# Check 10.0.0.1 with eth1
1 ipset test test 10.0.0.1,eth1
# Check 10.0.0.1 with eth2
1 ipset test test 10.0.0.1,eth2
# Check 10.0.0.1 with eth0
0 ipset test test 10.0.0.1,eth0
# Check 1.0.1.1 with eth1
1 ipset test test 1.0.1.1,eth1
# Check 1.0.1.1 with eth2
1 ipset test test 1.0.1.1,eth2
# Check 1.0.1.1 with eth0
0 ipset test test 1.0.1.1,eth0
# Delete test set
0 ipset destroy test
# Create test set
0 ipset new test hash:net,iface
# Add a /16 network with eth0
0 ipset add test 10.0.0.0/16,eth0
# Add an overlapping /24 network with eth1
0 ipset add test 10.0.0.0/24,eth1
# Add an overlapping /28 network with eth2
0 ipset add test 10.0.0.0/28,eth2
# Check matching element: from /28, with eth2
0 ipset test test 10.0.0.1,eth2
# Check non-matching element: from /28, with eth1
1 ipset test test 10.0.0.2,eth1
# Check non-matching element: from /28, with eth0
1 ipset test test 10.0.0.3,eth0
# Check matching element from: /24, with eth1
0 ipset test test 10.0.0.16,eth1
# Check non-matching element: from /24, with eth2
1 ipset test test 10.0.0.17,eth2
# Check non-matching element: from /24, with eth0
1 ipset test test 10.0.0.18,eth0
# Check matching element: from /16, with eth0
0 ipset test test 10.0.1.1,eth0
# Check non-matching element: from /16, with eth1
1 ipset test test 10.0.1.2,eth1
# Check non-matching element: from /16, with eth2
1 ipset test test 10.0.1.3,eth2
# Flush test set
0 ipset flush test
# Add overlapping networks from /4 to /30
0 (set -e; for x in `seq 4 30`; do ipset add test 192.0.0.0/$x,eth$x; done)
# List test set
0 ipset -L test 2>/dev/null | grep -v Revision: > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -u -I 'Size in memory.*' .foo hash:net,iface.t.list1
# Test matching elements in all added networks from /30 to /24
0 (set -e; y=2; for x in `seq 24 30 | tac`; do ipset test test 192.0.0.$y,eth$x; y=$((y*2)); done)
# Test non-matching elements in all added networks from /30 to /24
0 (y=2; for x in `seq 24 30 | tac`; do z=$((x-1)); ipset test test 192.0.0.$y,eth$z; ret=$?; test $ret -eq 0 && exit 1; y=$((y*2)); done)
# Delete test set
0 ipset destroy test
# Create test set with minimal hash size
0 ipset create test hash:net,iface hashsize 64
# Add clashing elements
0 (set -e; for x in `seq 0 63`; do ipset add test 10.0.0.0/16,eth$x; done)
# Check listing
0 n=`ipset list test | grep -v Revision: | wc -l` && test $n -eq 71
# Delete test set
0 ipset destroy test
# Check all possible CIDR values
0 ./cidr.sh net,iface
# Create test set with timeout support
0 ipset create test hash:net,iface timeout 30
# Add a non-matching IP address entry
0 ipset -A test 1.1.1.1,eth0 nomatch
# Add an overlapping matching small net
0 ipset -A test 1.1.1.0/30,eth0
# Add an overlapping non-matching larger net
0 ipset -A test 1.1.1.0/28,eth0 nomatch
# Add an even larger matching net
0 ipset -A test 1.1.1.0/26,eth0
# Check non-matching IP
1 ipset -T test 1.1.1.1,eth0
# Check matching IP from non-matchin small net
0 ipset -T test 1.1.1.3,eth0
# Check non-matching IP from larger net
1 ipset -T test 1.1.1.4,eth0
# Check matching IP from even larger net
0 ipset -T test 1.1.1.16,eth0
# Update non-matching IP to matching one
0 ipset -! -A test 1.1.1.1,eth0
# Delete overlapping small net
0 ipset -D test 1.1.1.0/30,eth0
# Check matching IP
0 ipset -T test 1.1.1.1,eth0
# Add overlapping small net
0 ipset -A test 1.1.1.0/30,eth0
# Update matching IP as a non-matching one, with shorter timeout
0 ipset -! -A test 1.1.1.1,eth0 nomatch timeout 2
# Check non-matching IP
1 ipset -T test 1.1.1.1,eth0
# Sleep 3s so that element can time out
0 sleep 3
# Check non-matching IP
0 ipset -T test 1.1.1.1,eth0
# Check matching IP
0 ipset -T test 1.1.1.3,eth0
# Delete test set
0 ipset destroy test
# Timeout: Check that resizing keeps timeout values
0 ./resizet.sh -4 netiface
# Nomatch: Check that resizing keeps the nomatch flag
0 ./resizen.sh -4 netiface
# Counters: create set
0 ipset n test hash:net,iface counters
# Counters: add element with packet, byte counters
0 ipset a test 2.0.0.1/24,eth0 packets 5 bytes 3456
# Counters: check element
0 ipset t test 2.0.0.1/24,eth0
# Counters: check counters
0 ./check_counters test 2.0.0.0/24,eth0 5 3456
# Counters: delete element
0 ipset d test 2.0.0.1/24,eth0
# Counters: test deleted element
1 ipset t test 2.0.0.1/24,eth0
# Counters: add element with packet, byte counters
0 ipset a test 2.0.0.20/25,wlan0 packets 12 bytes 9876
# Counters: check counters
0 ./check_counters test 2.0.0.0/25,wlan0 12 9876
# Counters: update counters
0 ipset -! a test 2.0.0.20/25,wlan0 packets 13 bytes 12479
# Counters: check counters
0 ./check_counters test 2.0.0.0/25,wlan0 13 12479
# Counters: destroy set
0 ipset x test
# Counters and timeout: create set
0 ipset n test hash:net,iface counters timeout 600
# Counters and timeout: add element with packet, byte counters
0 ipset a test 2.0.0.1/24,eth0 packets 5 bytes 3456
# Counters and timeout: check element
0 ipset t test 2.0.0.1/24,eth0
# Counters and timeout: check counters
0 ./check_extensions test 2.0.0.0/24,eth0 600 5 3456
# Counters and timeout: delete element
0 ipset d test 2.0.0.1/24,eth0
# Counters and timeout: test deleted element
1 ipset t test 2.0.0.1/24,eth0
# Counters and timeout: add element with packet, byte counters
0 ipset a test 2.0.0.20/25,wlan0 packets 12 bytes 9876
# Counters and timeout: check counters
0 ./check_extensions test 2.0.0.0/25,wlan0 600 12 9876
# Counters and timeout: update counters
0 ipset -! a test 2.0.0.20/25,wlan0 packets 13 bytes 12479
# Counters and timeout: check counters
0 ./check_extensions test 2.0.0.0/25,wlan0 600 13 12479
# Counters and timeout: update timeout
0 ipset -! a test 2.0.0.20/25,wlan0 timeout 700
# Counters and timeout: check counters
0 ./check_extensions test 2.0.0.0/25,wlan0 700 13 12479
# Counters and timeout: destroy set
0 ipset x test
# eof
