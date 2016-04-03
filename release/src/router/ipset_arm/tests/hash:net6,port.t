# Create a set with timeout
0 ipset create test hash:net,port family inet6 hashsize 128 timeout 5
# Add zero valued element
1 ipset add test ::/0,tcp:8
# Test zero valued element
1 ipset test test ::/0,tcp:8
# Delete zero valued element
1 ipset del test ::/0,tcp:8
# Try to add /0
1 ipset add test 1:1:1::1/0,tcp:8
# Try to add /128
0 ipset add test 1:1:1::1/128,tcp:8 timeout 0
# Add almost zero valued element
0 ipset add test 0:0:0::0/1,tcp:8
# Test almost zero valued element
0 ipset test test 0:0:0::0/1,tcp:8
# Test almost zero valued element with UDP
1 ipset test test 0:0:0::0/1,udp:8
# Delete almost zero valued element
0 ipset del test 0:0:0::0/1,tcp:8
# Test deleted element
1 ipset test test 0:0:0::0/1,tcp:8
# Delete element not added to the set
1 ipset del test 0:0:0::0/1,tcp:8
# Add first random network
0 ipset add test 2:0:0::1/24,tcp:8
# Add second random network
0 ipset add test 192:168:68::69/27,icmpv6:ping
# Test first random value
0 ipset test test 2:0:0::255,tcp:8
# Test second random value
0 ipset test test 192:168:68::95,icmpv6:ping
# Test value not added to the set
1 ipset test test 3:0:0::1,tcp:8
# Try to add IP address
0 ipset add test 3:0:0::1,tcp:8
# Add ICMPv6 by type/code
0 ipset add test 192:168:68::95,icmpv6:1/4
# Test ICMPv6 by type/code
0 ipset test test 192:168:68::95,icmpv6:1/4
# Test ICMPv6 by name
0 ipset test test 192:168:68::95,icmpv6:port-unreachable
# List set
0 ipset list test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo0 && ./sort.sh .foo0
# Save set
0 ipset save test > hash:net6,port.t.restore
# Check listing
0 diff -u -I 'Size in memory.*' .foo hash:net6,port.t.list0
# Sleep 5s so that element can time out
0 sleep 5
# IP: List set
0 ipset -L test 2>/dev/null | grep -v Revision: > .foo0 && ./sort.sh .foo0
# IP: Check listing
0 diff -u -I 'Size in memory.*' .foo hash:net6,port.t.list1
# Destroy set
0 ipset x test
# Restore set
0 ipset restore < hash:net6,port.t.restore && rm hash:net6,port.t.restore
# List set
0 ipset list test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -u -I 'Size in memory.*' .foo hash:net6,port.t.list0
# Flush test set
0 ipset flush test
# Add multiple elements in one step
0 ipset add test 1::1/64,80-84 timeout 0
# Delete multiple elements in one step
0 ipset del test 1::1/64,tcp:81-82
# Check number of elements after multi-add/multi-del
0 n=`ipset save test|wc -l` && test $n -eq 4
# Delete test set
0 ipset destroy test
# Create set to add a range
0 ipset new test hash:net,port -6 hashsize 64
# Add a range which forces a resizing
0 ipset add test 1::1/64,tcp:80-1105
# Check that correct number of elements are added
0 n=`ipset list test|grep 1::|wc -l` && test $n -eq 1026
# Destroy set
0 ipset -X test
# Create test set with timeout support
0 ipset create test hash:net,port family inet6 timeout 30
# Add a non-matching IP address entry
0 ipset -A test 1:1:1::1,80 nomatch
# Add an overlapping matching small net
0 ipset -A test 1:1:1::/124,80
# Add an overlapping non-matching larger net
0 ipset -A test 1:1:1::/120,80 nomatch
# Add an even larger matching net
0 ipset -A test 1:1:1::/116,80
# Check non-matching IP
1 ipset -T test 1:1:1::1,80
# Check matching IP from non-matchin small net
0 ipset -T test 1:1:1::F,80
# Check non-matching IP from larger net
1 ipset -T test 1:1:1::10,80
# Check matching IP from even larger net
0 ipset -T test 1:1:1::100,80
# Update non-matching IP to matching one
0 ipset -! -A test 1:1:1::1,80
# Delete overlapping small net
0 ipset -D test 1:1:1::/124,80
# Check matching IP
0 ipset -T test 1:1:1::1,80
# Add overlapping small net
0 ipset -A test 1:1:1::/124,80
# Update matching IP as a non-matching one, with shorter timeout
0 ipset -! -A test 1:1:1::1,80 nomatch timeout 2
# Check non-matching IP
1 ipset -T test 1:1:1::1,80
# Sleep 3s so that element can time out
0 sleep 3
# Check non-matching IP
0 ipset -T test 1:1:1::1,80
# Check matching IP
0 ipset -T test 1:1:1::F,80
# Delete test set
0 ipset destroy test
# Timeout: Check that resizing keeps timeout values
0 ./resizet.sh -6 netport
# Nomatch: Check that resizing keeps the nomatch flag
0 ./resizen.sh -6 netport
# Counters: create set
0 ipset n test hash:net,port -6 counters
# Counters: add element with packet, byte counters
0 ipset a test 2:0:0::1/64,80 packets 5 bytes 3456
# Counters: check element
0 ipset t test 2:0:0::1/64,80
# Counters: check counters
0 ./check_counters test 2:: 5 3456
# Counters: delete element
0 ipset d test 2:0:0::1/64,80
# Counters: test deleted element
1 ipset t test 2:0:0::1/64,80
# Counters: add element with packet, byte counters
0 ipset a test 2:0:0::20/54,453 packets 12 bytes 9876
# Counters: check counters
0 ./check_counters test 2:: 12 9876
# Counters: update counters
0 ipset -! a test 2:0:0::20/54,453 packets 13 bytes 12479
# Counters: check counters
0 ./check_counters test 2:: 13 12479
# Counters: destroy set
0 ipset x test
# Counters and timeout: create set
0 ipset n test hash:net,port -6 counters timeout 600
# Counters and timeout: add element with packet, byte counters
0 ipset a test 2:0:0::1/64,80 packets 5 bytes 3456
# Counters and timeout: check element
0 ipset t test 2:0:0::1/64,80
# Counters and timeout: check counters
0 ./check_extensions test 2:: 600 5 3456
# Counters and timeout: delete element
0 ipset d test 2:0:0::1/64,80
# Counters and timeout: test deleted element
1 ipset t test 2:0:0::1/64,80
# Counters and timeout: add element with packet, byte counters
0 ipset a test 2:0:0::20/54,453 packets 12 bytes 9876
# Counters and timeout: check counters
0 ./check_extensions test 2:: 600 12 9876
# Counters and timeout: update counters
0 ipset -! a test 2:0:0::20/54,453 packets 13 bytes 12479
# Counters and timeout: check counters
0 ./check_extensions test 2:: 600 13 12479
# Counters and timeout: update timeout
0 ipset -! a test 2:0:0::20/54,453 timeout 700
# Counters and timeout: check counters
0 ./check_extensions test 2:: 700 13 12479
# Counters and timeout: destroy set
0 ipset x test
# eof
