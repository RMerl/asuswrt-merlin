# Create a set with timeout
0 ipset create test nethash hashsize 128 timeout 5
# Add zero valued element
1 ipset add test 0.0.0.0/0
# Test zero valued element
1 ipset test test 0.0.0.0/0
# Delete zero valued element
1 ipset del test 0.0.0.0/0
# Try to add /0
1 ipset add test 1.1.1.1/0
# Try to add /32
0 ipset add test 1.1.1.1/32
# Add almost zero valued element
0 ipset add test 0.0.0.0/1
# Test almost zero valued element
0 ipset test test 0.0.0.0/1
# Delete almost zero valued element
0 ipset del test 0.0.0.0/1
# Test deleted element
1 ipset test test 0.0.0.0/1
# Delete element not added to the set
1 ipset del test 0.0.0.0/1
# Add first random network
0 ipset add test 2.0.0.1/24
# Add second random network
0 ipset add test 192.168.68.69/27
# Test first random value
0 ipset test test 2.0.0.255
# Test second random value
0 ipset test test 192.168.68.95
# Test value not added to the set
1 ipset test test 2.0.1.0
# Try to add IP address
0 ipset add test 2.0.0.1
# List set
0 ipset list test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -u -I 'Size in memory.*' .foo hash:net.t.list0
# Sleep 5s so that element can time out
0 sleep 5
# List set
0 ipset -L test 2>/dev/null | grep -v Revision: > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -u -I 'Size in memory.*' .foo hash:net.t.list1
# Flush test set
0 ipset flush test
# Delete test set
0 ipset destroy test
# Create test set
0 ipset new test hash:net
# Add networks in range notation
0 ipset add test 10.2.0.0-10.2.1.12
# List set
0 ipset -L test 2>/dev/null | grep -v Revision: > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -u -I 'Size in memory.*' .foo hash:net.t.list2
# Delete test set
0 ipset destroy test
# Stress test with range notation
0 ./netgen.sh | ipset restore
# List set and check the number of elements
0 n=`ipset -L test|grep '^10.'|wc -l` && test $n -eq 43520
# Destroy test set
0 ipset destroy test
# Verify keeping track cidrs: create set
0 ipset n test hash:net
# Verify keeping track cidrs: add /16 net
0 ipset a test 1.1.0.0/16
# Verify keeping track cidrs: add /24 net
0 ipset a test 2.2.2.0/24
# Verify keeping track cidrs: del /24 net
0 ipset d test 2.2.2.0/24
# Verify keeping track cidrs: check address in /16
0 ipset t test 1.1.1.1
# Destroy test set
0 ipset x test
# Create test set with timeout support
0 ipset create test hash:net timeout 30
# Add a non-matching IP address entry
0 ipset -A test 1.1.1.1 nomatch
# Add an overlapping matching small net
0 ipset -A test 1.1.1.0/30
# Add an overlapping non-matching larger net
0 ipset -A test 1.1.1.0/28 nomatch
# Add an even larger matching net
0 ipset -A test 1.1.1.0/26
# Check non-matching IP
1 ipset -T test 1.1.1.1
# Check matching IP from non-matchin small net
0 ipset -T test 1.1.1.3
# Check non-matching IP from larger net
1 ipset -T test 1.1.1.4
# Check matching IP from even larger net
0 ipset -T test 1.1.1.16
# Update non-matching IP to matching one
0 ipset -! -A test 1.1.1.1
# Delete overlapping small net
0 ipset -D test 1.1.1.0/30
# Check matching IP
0 ipset -T test 1.1.1.1
# Add overlapping small net
0 ipset -A test 1.1.1.0/30
# Update matching IP as a non-matching one, with shorter timeout
0 ipset -! -A test 1.1.1.1 nomatch timeout 2
# Check non-matching IP
1 ipset -T test 1.1.1.1
# Sleep 3s so that element can time out
0 sleep 3
# Check non-matching IP
0 ipset -T test 1.1.1.1
# Check matching IP
0 ipset -T test 1.1.1.3
# Delete test set
0 ipset destroy test
# Check CIDR book-keeping
0 ./check_cidrs.sh
# Check all possible CIDR values
0 ./cidr.sh net
# Timeout: Check that resizing keeps timeout values
0 ./resizet.sh -4 net
# Nomatch: Check that resizing keeps the nomatch flag
0 ./resizen.sh -4 net
# Counters: create set
0 ipset n test hash:net counters
# Counters: add element with packet, byte counters
0 ipset a test 2.0.0.1/24 packets 5 bytes 3456
# Counters: check element
0 ipset t test 2.0.0.1/24
# Counters: check counters
0 ./check_counters test 2.0.0.0/24 5 3456
# Counters: delete element
0 ipset d test 2.0.0.1/24
# Counters: test deleted element
1 ipset t test 2.0.0.1/24
# Counters: add element with packet, byte counters
0 ipset a test 2.0.0.20/25 packets 12 bytes 9876
# Counters: check counters
0 ./check_counters test 2.0.0.0/25 12 9876
# Counters: update counters
0 ipset -! a test 2.0.0.20/25 packets 13 bytes 12479
# Counters: check counters
0 ./check_counters test 2.0.0.0/25 13 12479
# Counters: destroy set
0 ipset x test
# Counters and timeout: create set
0 ipset n test hash:net counters timeout 600
# Counters and timeout: add element with packet, byte counters
0 ipset a test 2.0.0.1/24 packets 5 bytes 3456
# Counters and timeout: check element
0 ipset t test 2.0.0.1/24
# Counters and timeout: check counters
0 ./check_extensions test 2.0.0.0/24 600 5 3456
# Counters and timeout: delete element
0 ipset d test 2.0.0.1/24
# Counters and timeout: test deleted element
1 ipset t test 2.0.0.1/24
# Counters and timeout: add element with packet, byte counters
0 ipset a test 2.0.0.20/25 packets 12 bytes 9876
# Counters and timeout: check counters
0 ./check_extensions test 2.0.0.0/25 600 12 9876
# Counters and timeout: update counters
0 ipset -! a test 2.0.0.20/25 packets 13 bytes 12479
# Counters and timeout: check counters
0 ./check_extensions test 2.0.0.0/25 600 13 12479
# Counters and timeout: update timeout
0 ipset -! a test 2.0.0.20/25 timeout 700
# Counters and timeout: check counters
0 ./check_extensions test 2.0.0.0/25 700 13 12479
# Counters and timeout: destroy set
0 ipset x test
# eof
