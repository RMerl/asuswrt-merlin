# Create a set with timeout
0 ipset create test hash:ip,mark timeout 5
# Add partly zero valued element
0 ipset add test 2.0.0.1,0x0
# Test partly zero valued element
0 ipset test test 2.0.0.1,0x0
# Delete partly zero valued element
0 ipset del test 2.0.0.1,0x0
# Add first random value
0 ipset add test 2.0.0.1,0x5
# Add second random value
0 ipset add test 2.1.0.0,0x80
# Test first random value
0 ipset test test 2.0.0.1,0x5
# Test second random value
0 ipset test test 2.1.0.0,0x80
# Test value not added to the set
1 ipset test test 2.0.0.1,0x4
# Delete value not added to the set
1 ipset del test 2.0.0.1,0x6
# Test value before first random value
1 ipset test test 2.0.0.0,0x5
# Test value after second random value
1 ipset test test 2.1.0.1,0x80
# Try to add value before first random value
0 ipset add test 2.0.0.0,0x5
# Try to add value after second random value
0 ipset add test 2.1.0.1,0x80
# List set
0 ipset list test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -u -I 'Size in memory.*' .foo hash:ip,mark.t.list0
# Sleep 5s so that elements can time out
0 sleep 5
# List set
0 ipset list test | grep -v Revision: > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -u -I 'Size in memory.*' .foo hash:ip,mark.t.list1
# Flush test set
0 ipset flush test
# Add multiple elements in one step
0 ipset add test 1.1.1.1-1.1.1.18,0x50
# Delete multiple elements in one step
0 ipset del test 1.1.1.2-1.1.1.3,0x50
# Check number of elements after multi-add/multi-del
0 n=`ipset save test|wc -l` && test $n -eq 17
# Delete test set
0 ipset destroy test
# Create set to add a range
0 ipset new test hash:ip,mark hashsize 64
# Add a range which forces a resizing
0 ipset add test 10.0.0.0-10.0.3.255,0x50
# Check that correct number of elements are added
0 n=`ipset list test|grep '^10.0'|wc -l` && test $n -eq 1024
# Flush set
0 ipset flush test
# Add an single element
0 ipset add test 10.0.0.1,0x50
# Check number of elements
0 n=`ipset save test|wc -l` && test $n -eq 2
# Delete the single element
0 ipset del test 10.0.0.1,0x50
# Check number of elements
0 n=`ipset save test|wc -l` && test $n -eq 1
# Add an IP range
0 ipset add test 10.0.0.1-10.0.0.10,0x50
# Check number of elements
0 n=`ipset save test|wc -l` && test $n -eq 11
# Delete the IP range
0 ipset del test 10.0.0.1-10.0.0.10,80
# Check number of elements
0 n=`ipset save test|wc -l` && test $n -eq 1
# Destroy set
0 ipset -X test
# Timeout: Check that resizing keeps timeout values
0 ./resizet.sh -4 ipmark
# Counters: create set
0 ipset n test hash:ip,mark counters
# Counters: add element with packet, byte counters
0 ipset a test 2.0.0.1,0x50 packets 5 bytes 3456
# Counters: check element
0 ipset t test 2.0.0.1,0x50
# Counters: check counters
0 ./check_counters test 2.0.0.1 5 3456
# Counters: delete element
0 ipset d test 2.0.0.1,0x50
# Counters: test deleted element
1 ipset t test 2.0.0.1,0x50
# Counters: add element with packet, byte counters
0 ipset a test 2.0.0.20,0x1c5 packets 12 bytes 9876
# Counters: check counters
0 ./check_counters test 2.0.0.20 12 9876
# Counters: update counters
0 ipset -! a test 2.0.0.20,0x1c5 packets 13 bytes 12479
# Counters: check counters
0 ./check_counters test 2.0.0.20 13 12479
# Counters: destroy set
0 ipset x test
# Counters and timeout: create set
0 ipset n test hash:ip,mark counters timeout 600
# Counters and timeout: add element with packet, byte counters
0 ipset a test 2.0.0.1,0x50 packets 5 bytes 3456
# Counters and timeout: check element
0 ipset t test 2.0.0.1,0x50
# Counters and timeout: check counters
0 ./check_extensions test 2.0.0.1 600 5 3456
# Counters and timeout: delete element
0 ipset d test 2.0.0.1,0x50
# Counters and timeout: test deleted element
1 ipset t test 2.0.0.1,0x50
# Counters and timeout: add element with packet, byte counters
0 ipset a test 2.0.0.20,0x1c5 packets 12 bytes 9876
# Counters and timeout: check counters
0 ./check_extensions test 2.0.0.20 600 12 9876
# Counters and timeout: update counters
0 ipset -! a test 2.0.0.20,0x1c5 packets 13 bytes 12479
# Counters and timeout: check counters
0 ./check_extensions test 2.0.0.20 600 13 12479
# Counters and timeout: update timeout
0 ipset -! a test 2.0.0.20,0x1c5 timeout 700
# Counters and timeout: check counters
0 ./check_extensions test 2.0.0.20 700 13 12479
# Counters and timeout: destroy set
0 ipset x test
# Create a set with 0x0000FFDE markmask
0 ipset create test hash:ip,mark markmask 0x0000FFDE
# Add first value with 0x0521F322 mark
0 ipset add test 19.16.1.254,0x0521F322
# Test last entry mark was modified to 0x0000F302
0 ipset test test 19.16.1.254,0x0000F302
# Test that mask is applied for tests as well
0 ipset test test 19.16.1.254,0x0521F322
# Destroy set
0 ipset x test
# eof
