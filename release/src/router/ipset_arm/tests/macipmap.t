# Range: Try to create from an invalid range
1 ipset -N test macipmap --from 2.0.0.1 --to 2.1.0.1
# Range: Create a set from a valid range
0 ipset -N test macipmap --from 2.0.0.1 --to 2.1.0.0
# Range: Add lower boundary
0 ipset -A test 2.0.0.1
# Range: Add upper boundary
0 ipset -A test 2.1.0.0
# Range: Test lower boundary
0 ipset -T test 2.0.0.1
# Range: Test upper boundary
0 ipset -T test 2.1.0.0
# Range: Test value not added to the set
1 ipset -T test 2.0.0.2
# Range: Test value before lower boundary
1 ipset -T test 2.0.0.0
# Range: Test value after upper boundary
1 ipset -T test 2.1.0.1
# Range: Try to add value before lower boundary
1 ipset -A test 2.0.0.0
# Range: Try to add value after upper boundary
1 ipset -A test 2.1.0.1
# Range: Delete element not added to the set
1 ipset -D test 2.0.0.2
# Range: Try to add value with MAC
0 ipset -A test 2.0.0.2,00:11:22:33:44:55
# Range: Test value with invalid MAC
1 ipset -T test 2.0.0.2,00:11:22:33:44:56
# Range: Test value with valid MAC
0 ipset -T test 2.0.0.2,00:11:22:33:44:55
# Range: Add MAC to already added element
0 ipset -A test 2.0.0.1,00:11:22:33:44:56
# Range: Test value without supplying MAC
0 ipset -T test 2.0.0.1
# Range: Test value with valid MAC
0 ipset -T test 2.0.0.1,00:11:22:33:44:56
# Range: Add an element in the middle
0 ipset -A test 2.0.200.214,00:11:22:33:44:57
# Range: Delete the same element
0 ipset -D test 2.0.200.214
# Range: List set
0 ipset -L test | grep -v Revision: > .foo
# Range: Check listing
0 diff -u -I 'Size in memory.*' .foo macipmap.t.list0
# Range: Flush test set
0 ipset -F test
# Range: Catch invalid (too long) MAC
1 ipset -A test 2.0.0.2,00:11:22:33:44:55:66
# Range: Catch invalid (too short) MAC
1 ipset -A test 2.0.0.2,00:11:22:33:44
# Range: Add an element with MAC without leading zeros
0 ipset -A test 2.0.0.2,0:1:2:3:4:5
# Range: Check element with MAC without leading zeros
0 ipset -T test 2.0.0.2,0:1:2:3:4:5
# Range: Delete test set
0 ipset -X test
# Network: Try to create a set from an invalid network
1 ipset -N test macipmap --network 2.0.0.0/15
# Network: Create a set from a valid network
0 ipset -N test macipmap --network 2.0.0.1/16
# Network: Add lower boundary
0 ipset -A test 2.0.0.0
# Network: Add upper boundary
0 ipset -A test 2.0.255.255
# Network: Test lower boundary
0 ipset -T test 2.0.0.0
# Network: Test upper boundary
0 ipset -T test 2.0.255.255
# Network: Test value not added to the set
1 ipset -T test 2.0.0.1
# Network: Test value before lower boundary
1 ipset -T test 1.255.255.255
# Network: Test value after upper boundary
1 ipset -T test 2.1.0.0
# Network: Try to add value before lower boundary
1 ipset -A test 1.255.255.255
# Network: Try to add value after upper boundary
1 ipset -A test 2.1.0.0
# Network: Delete element not added to the set
1 ipset -D test 2.0.0.2
# Network: Try to add value with MAC
0 ipset -A test 2.0.0.2,00:11:22:33:44:55
# Network: Test value with invalid MAC
1 ipset -T test 2.0.0.2,00:11:22:33:44:56
# Network: Test value with valid MAC
0 ipset -T test 2.0.0.2,00:11:22:33:44:55
# Network: Add MAC to already added element
0 ipset -A test 2.0.255.255,00:11:22:33:44:56
# Network: List set
0 ipset -L test | grep -v Revision: > .foo
# Network: Check listing
0 diff -u -I 'Size in memory.*' .foo macipmap.t.list1
# Network: Flush test set
0 ipset -F test
# Network: Delete test set
0 ipset -X test
# Range: Create a set from a valid range with timeout
0 ipset -N test macipmap --from 2.0.0.1 --to 2.1.0.0 timeout 5
# Range: Add lower boundary
0 ipset -A test 2.0.0.1 timeout 4
# Range: Add upper boundary
0 ipset -A test 2.1.0.0 timeout 3
# Range: Test lower boundary
0 ipset -T test 2.0.0.1
# Range: Test upper boundary
0 ipset -T test 2.1.0.0
# Range: Test value not added to the set
1 ipset -T test 2.0.0.2
# Range: Test value before lower boundary
1 ipset -T test 2.0.0.0
# Range: Test value after upper boundary
1 ipset -T test 2.1.0.1
# Range: Try to add value before lower boundary
1 ipset -A test 2.0.0.0
# Range: Try to add value after upper boundary
1 ipset -A test 2.1.0.1
# Range: Try to add value with MAC
0 ipset -A test 2.0.0.2,00:11:22:33:44:55 timeout 4
# Range: Test value with invalid MAC
1 ipset -T test 2.0.0.2,00:11:22:33:44:56
# Range: Test value with valid MAC
0 ipset -T test 2.0.0.2,00:11:22:33:44:55
# Range: Add MAC to already added element
0 ipset -A test 2.0.0.1,00:11:22:33:44:56
# Range: Add an element in the middle
0 ipset -A test 2.0.200.214,00:11:22:33:44:57
# Range: Delete the same element
0 ipset -D test 2.0.200.214
# Range: List set
0 ipset -L test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo
# Range: Check listing
0 diff -u -I 'Size in memory.*' .foo macipmap.t.list3
# Range: sleep 5s so that elements can timeout
0 sleep 5
# Range: List set
0 ipset -L test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo
# Range: Check listing
0 diff -u -I 'Size in memory.*' .foo macipmap.t.list2
# Range: Flush test set
0 ipset -F test
# Range: add element with 1s timeout
0 ipset add test 2.0.200.214,00:11:22:33:44:57 timeout 1
# Range: readd element with 3s timeout
0 ipset add test 2.0.200.214,00:11:22:33:44:57 timeout 3 -exist
# Range: sleep 2s
0 sleep 2s
# Range: check readded element
0 ipset test test 2.0.200.214
# Range: Delete test set
0 ipset -X test
# Counters: create set
0 ipset n test bitmap:ip,mac range 2.0.0.1-2.1.0.0 counters
# Counters: add element with packet, byte counters
0 ipset a test 2.0.0.1,00:11:22:33:44:57 packets 5 bytes 3456
# Counters: check element
0 ipset t test 2.0.0.1,00:11:22:33:44:57
# Counters: check counters
0 ./check_counters test 2.0.0.1,00:11:22:33:44:57 5 3456
# Counters: delete element
0 ipset d test 2.0.0.1,00:11:22:33:44:57
# Counters: test deleted element
1 ipset t test 2.0.0.1,00:11:22:33:44:57
# Counters: add element with packet, byte counters
0 ipset a test 2.0.0.1,00:11:22:33:44:57 packets 12 bytes 9876
# Counters: check counters
0 ./check_counters test 2.0.0.1,00:11:22:33:44:57 12 9876
# Counters: update counters
0 ipset -! a test 2.0.0.1,00:11:22:33:44:57 packets 13 bytes 12479
# Counters: check counters
0 ./check_counters test 2.0.0.1,00:11:22:33:44:57 13 12479
# Counters: destroy set
0 ipset x test
# Counters and timeout: create set
0 ipset n test bitmap:ip,mac range 2.0.0.1-2.1.0.0 counters timeout 600
# Counters and timeout: add element with packet, byte counters
0 ipset a test 2.0.0.1,00:11:22:33:44:57 packets 5 bytes 3456
# Counters and timeout: check element
0 ipset t test 2.0.0.1,00:11:22:33:44:57
# Counters and timeout: check counters
0 ./check_extensions test 2.0.0.1,00:11:22:33:44:57 600 5 3456
# Counters and timeout: delete element
0 ipset d test 2.0.0.1,00:11:22:33:44:57
# Counters and timeout: test deleted element
1 ipset t test 2.0.0.1,00:11:22:33:44:57
# Counters and timeout: add element with packet, byte counters
0 ipset a test 2.0.0.10,00:11:22:33:44:88 packets 12 bytes 9876
# Counters and timeout: check counters
0 ./check_extensions test 2.0.0.10,00:11:22:33:44:88 600 12 9876
# Counters and timeout: update counters
0 ipset -! a test 2.0.0.10,00:11:22:33:44:88 packets 13 bytes 12479
# Counters and timeout: check counters
0 ./check_extensions test 2.0.0.10,00:11:22:33:44:88 600 13 12479
# Counters and timeout: update timeout
0 ipset -! a test 2.0.0.10,00:11:22:33:44:88 timeout 700
# Counters and timeout: check counters
0 ./check_extensions test 2.0.0.10,00:11:22:33:44:88 700 13 12479
# Counters and timeout: destroy set
0 ipset x test
# eof
