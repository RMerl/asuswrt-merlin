# Create a set with timeout
0 ipset create test hash:net,net family inet6 hashsize 128 timeout 4
# Add zero valued element
1 ipset add test ::/0,::/0
# Test zero valued element
1 ipset test test ::/0,::/0
# Delete zero valued element
1 ipset del test ::/0,::/0
# Try to add /0
1 ipset add test 1:1:1::1/0,1:1:1::1/0
# Try to add /128
0 ipset add test 1:1:1::1/128,2:2:2::2/128
# Add almost zero valued element
0 ipset add test 0:0:0::0/1,0:0:0::0/1
# Test almost zero valued element
0 ipset test test 0:0:0::0/1,0:0:0::0/1
# Delete almost zero valued element
0 ipset del test 0:0:0::0/1,0:0:0::0/1
# Test deleted element
1 ipset test test 0:0:0::0/1,0:0:0::0/1
# Delete element not added to the set
1 ipset del test 0:0:0::0/1,0:0:0::0/1
# Add first random network
0 ipset add test 2:0:0::1/24,4:0:0::1/32
# Add second random network
0 ipset add test 192:168:68::69/27,172:16:68::69/48
# Test first random value
0 ipset test test 2:0:0::255,4:0:0::54
# Test second random value
0 ipset test test 192:168:68::95,172:16:68::68
# Test value not added to the set
1 ipset test test 3:0:0::1,172:255:24::1
# Try to add IP address
0 ipset add test 3:0:0::1,8:0:0::1
# List set
0 ipset list test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -u -I 'Size in memory.*' .foo hash:net6,net6.t.list0
# Sleep 5s so that element can time out
0 sleep 5
# IP: List set
0 ipset -L test 2>/dev/null | grep -v Revision: > .foo0 && ./sort.sh .foo0
# IP: Check listing
0 diff -u -I 'Size in memory.*' .foo hash:net6,net6.t.list1
# Flush test set
0 ipset flush test
# Add matching IP address entry
0 ipset -A test 2001:db8::1/64,2001:db8:fe9::1/126
# Add more-specific non-matching IP address entry
0 ipset -A test 2001:db8::1/68,2001:db8:fe9::1/64 nomatch
# Add even-more-specific matching IP address entry
0 ipset -A test 2001:db8::1/72,2001:db8:fe9::1/64
# Check non-matching IP
1 ipset -T test 2001:db8:0:0:0f00::,2001:db8:fe9::1
# Check non-matching IP with nomatch
0 ipset -T test 2001:db8:0:0:0f00::,2001:db8:fe9::1 nomatch
# Check matching IP
0 ipset -T test 2001:db8::1,2001:db8:fe9::1
# Delete test set
0 ipset destroy test
# Create test set with timeout support
0 ipset create test hash:net,net family inet6 timeout 30
# Add a non-matching IP address entry
0 ipset -A test 1:1:1::1,2:2:2::2 nomatch
# Add an overlapping matching small net
0 ipset -A test 1:1:1::/124,2:2:2::/124
# Add an overlapping non-matching larger net
0 ipset -A test 1:1:1::/120,2:2:2::/120 nomatch
# Add an even larger matching net
0 ipset -A test 1:1:1::/116,2:2:2::/116
# Check non-matching IP
1 ipset -T test 1:1:1::1,2:2:2::2
# Check matching IP from non-matchin small net
0 ipset -T test 1:1:1::f,2:2:2::f
# Check non-matching IP from larger net
1 ipset -T test 1:1:1::10,2:2:2::10
# Check matching IP from even larger net
0 ipset -T test 1:1:1::100,2:2:2::100
# Update non-matching IP to matching one
0 ipset -! -A test 1:1:1::1,2:2:2::2
# Delete overlapping small net
0 ipset -D test 1:1:1::/124,2:2:2::/124
# Check matching IP
0 ipset -T test 1:1:1::1,2:2:2::2
# Add overlapping small net
0 ipset -A test 1:1:1::/124,2:2:2::/124
# Update matching IP as a non-matching one, with shorter timeout
0 ipset -! -A test 1:1:1::1,2:2:2::2 nomatch timeout 2
# Check non-matching IP
1 ipset -T test 1:1:1::1,2:2:2::2
# Sleep 3s so that element can time out
0 sleep 3
# Check non-matching IP
0 ipset -T test 1:1:1::1,2:2:2::2
# Check matching IP
0 ipset -T test 1:1:1::f,2:2:2::f
# Delete test set
0 ipset destroy test
# Timeout: Check that resizing keeps timeout values
0 ./resizet.sh -6 netnet
# Nomatch: Check that resizing keeps the nomatch flag
0 ./resizen.sh -6 netnet
# Counters: create set
0 ipset n test hash:net,net -6 counters
# Counters: add element with packet, byte counters
0 ipset a test 2:0:0::1/64,3:0:0::1/64 packets 5 bytes 3456
# Counters: check element
0 ipset t test 2:0:0::1/64,3:0:0::1/64
# Counters: check counters
0 ./check_counters test 2:: 5 3456
# Counters: delete element
0 ipset d test 2:0:0::1/64,3:0:0::1/64
# Counters: test deleted element
1 ipset t test 2:0:0::1/64,3:0:0::1/64
# Counters: add element with packet, byte counters
0 ipset a test 2:0:0::20/54,3:0:0::20/54 packets 12 bytes 9876
# Counters: check counters
0 ./check_counters test 2:: 12 9876
# Counters: update counters
0 ipset -! a test 2:0:0::20/54,3:0:0::20/54 packets 13 bytes 12479
# Counters: check counters
0 ./check_counters test 2:: 13 12479
# Counters: destroy set
0 ipset x test
# Counters and timeout: create set
0 ipset n test hash:net,net -6 counters timeout 600
# Counters and timeout: add element with packet, byte counters
0 ipset a test 2:0:0::1/64,3:0:0::1/64 packets 5 bytes 3456
# Counters and timeout: check element
0 ipset t test 2:0:0::1/64,3:0:0::1/64
# Counters and timeout: check counters
0 ./check_extensions test 2:: 600 5 3456
# Counters and timeout: delete element
0 ipset d test 2:0:0::1/64,3:0:0::1/64
# Counters and timeout: test deleted element
1 ipset t test 2:0:0::1/64,3:0:0::1/64
# Counters and timeout: add element with packet, byte counters
0 ipset a test 2:0:0::20/54,3:0:0::20/54 packets 12 bytes 9876
# Counters and timeout: check counters
0 ./check_extensions test 2:: 600 12 9876
# Counters and timeout: update counters
0 ipset -! a test 2:0:0::20/54,3:0:0::20/54 packets 13 bytes 12479
# Counters and timeout: check counters
0 ./check_extensions test 2:: 600 13 12479
# Counters and timeout: update timeout
0 ipset -! a test 2:0:0::20/54,3:0:0::20/54 timeout 700
# Counters and timeout: check counters
0 ./check_extensions test 2:: 700 13 12479
# Counters and timeout: destroy set
0 ipset x test
# eof
