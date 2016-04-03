# Create a set with timeout
0 ipset create test hash:ip,port family inet6 timeout 5
# Add partly zero valued element
0 ipset add test 2:0:0::1,0
# Test partly zero valued element
0 ipset test test 2:0:0::1,0
# Delete partly zero valued element
0 ipset del test 2:0:0::1,0
# Add first random value
0 ipset add test 2:0:0::1,5
# Add second random value
0 ipset add test 2:1:0::0,128
# Test first random value
0 ipset test test 2:0:0::1,5
# Test second random value
0 ipset test test 2:1:0::0,128
# Test value not added to the set
1 ipset test test 2:0:0::1,4
# Delete value not added to the set
1 ipset del test 2:0:0::1,6
# Test value before first random value
1 ipset test test 2:0:0::0,5
# Test value after second random value
1 ipset test test 2:1:0::1,128
# Try to add value before first random value
0 ipset add test 2:0:0::0,5
# Try to add value after second random value
0 ipset add test 2:1:0::1,128
# List set
0 ipset list test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -u -I 'Size in memory.*' .foo hash:ip6,port.t.list0
# Sleep 5s so that elements can time out
0 sleep 5
# List set
0 ipset list test | grep -v Revision: > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -u -I 'Size in memory.*' .foo hash:ip6,port.t.list1
# Flush test set
0 ipset flush test
# Add multiple elements in one step
0 ipset add test 1::1,udp:80-85
# Delete multiple elements in one step
0 ipset del test 1::1,udp:81-84
# Check elements after multiple add/del
0 ipset test test 1::1,udp:80
# Delete test set
0 ipset destroy test
# Create set to add a range
0 ipset new test hash:ip,port -6 hashsize 64
# Add a range which forces a resizing
0 ipset add test 1::1,tcp:80-1105
# Check that correct number of elements are added
0 n=`ipset list test|grep 1::1|wc -l` && test $n -eq 1026
# Destroy set
0 ipset -X test
# Timeout: Check that resizing keeps timeout values
0 ./resizet.sh -6 ipport
# Counters: create set
0 ipset n test hash:ip,port -6 counters
# Counters: add element with packet, byte counters
0 ipset a test 2:0:0::1,80 packets 5 bytes 3456
# Counters: check element
0 ipset t test 2:0:0::1,80
# Counters: check counters
0 ./check_counters test 2::1 5 3456
# Counters: delete element
0 ipset d test 2:0:0::1,80
# Counters: test deleted element
1 ipset t test 2:0:0::1,80
# Counters: add element with packet, byte counters
0 ipset a test 2:0:0::20,453 packets 12 bytes 9876
# Counters: check counters
0 ./check_counters test 2::20 12 9876
# Counters: update counters
0 ipset -! a test 2:0:0::20,453 packets 13 bytes 12479
# Counters: check counters
0 ./check_counters test 2::20 13 12479
# Counters: destroy set
0 ipset x test
# Counters and timeout: create set
0 ipset n test hash:ip,port -6 counters timeout 600
# Counters and timeout: add element with packet, byte counters
0 ipset a test 2:0:0::1,80 packets 5 bytes 3456
# Counters and timeout: check element
0 ipset t test 2:0:0::1,80
# Counters and timeout: check counters
0 ./check_extensions test 2::1 600 5 3456
# Counters and timeout: delete element
0 ipset d test 2:0:0::1,80
# Counters and timeout: test deleted element
1 ipset t test 2:0:0::1,80
# Counters and timeout: add element with packet, byte counters
0 ipset a test 2:0:0::20,453 packets 12 bytes 9876
# Counters and timeout: check counters
0 ./check_extensions test 2::20 600 12 9876
# Counters and timeout: update counters
0 ipset -! a test 2:0:0::20,453 packets 13 bytes 12479
# Counters and timeout: check counters
0 ./check_extensions test 2::20 600 13 12479
# Counters and timeout: update timeout
0 ipset -! a test 2:0:0::20,453 timeout 700
# Counters and timeout: check counters
0 ./check_extensions test 2::20 700 13 12479
# Counters and timeout: destroy set
0 ipset x test
# eof
