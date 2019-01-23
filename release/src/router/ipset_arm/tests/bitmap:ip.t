# Range: Try to create from an invalid range with timeout
1 ipset create test bitmap:ip range 2.0.0.1-2.1.0.1 timeout 5
# Range: Create a set from a valid range with timeout
0 ipset create test bitmap:ip range 2.0.0.1-2.1.0.0 timeout 5
# Range: Add lower boundary
0 ipset add test 2.0.0.1 timeout 4
# Range: Add upper boundary
0 ipset add test 2.1.0.0 timeout 0
# Range: Test lower boundary
0 ipset test test 2.0.0.1
# Range: Test upper boundary
0 ipset test test 2.1.0.0
# Range: Test element not added to the set
1 ipset test test 2.0.0.2
# Range: Test element before lower boundary
1 ipset test test 2.0.0.0
# Range: Test element after upper boundary
1 ipset test test 2.1.0.1
# Range: Try to add element before lower boundary
1 ipset add test 2.0.0.0
# Range: Try to add element after upper boundary
1 ipset add test 2.1.0.1
# Range: Delete element not added to the set
1 ipset -D test 2.0.0.2
# Range: Delete element not added to the set, with exist flag
0 ipset -! -D test 2.0.0.2
# Range: Add element in the middle
0 ipset -A test 2.0.0.128
# Range: Add element in the middle again
1 ipset -A test 2.0.0.128
# Range: Add element in the middle again, with exist flag
0 ipset -! -A test 2.0.0.128
# Range: Delete the same element
0 ipset -D test 2.0.0.128
# Range: Add a range of elements
0 ipset -A test 2.0.0.128-2.0.0.131 timeout 4
# Range: List set
0 ipset list test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo
# Range: Check listing
0 diff -u -I 'Size in memory.*' .foo bitmap:ip.t.list4
# Sleep 5s so that entries can time out
0 sleep 5s
# Range: List set after timeout
0 ipset list test | grep -v Revision: > .foo
# Range: Check listing
0 diff -u -I 'Size in memory.*' .foo bitmap:ip.t.list0
# Range: Flush test set
0 ipset flush test
# Range: Delete test set
0 ipset destroy test
# Network: Try to create a set from an invalid network with timeout
1 ipset create test bitmap:ip range 2.0.0.0/15 timeout 5
# Network: Create a set from a valid network with timeout
0 ipset create test bitmap:ip range 2.0.0.1/16 timeout 5
# Network: Add lower boundary
0 ipset add test 2.0.0.0 timeout 0
# Network: Add upper boundary
0 ipset add test 2.0.255.255 timeout 4
# Network: Test lower boundary
0 ipset test test 2.0.0.0
# Network: Test upper boundary
0 ipset test test 2.0.255.255
# Network: Test element not added to the set
1 ipset test test 2.0.0.1
# Network: Test element before lower boundary
1 ipset test test 1.255.255.255
# Network: Test element after upper boundary
1 ipset test test 2.1.0.0
# Network: Try to add element before lower boundary
1 ipset add test 1.255.255.255
# Network: Try to add element after upper boundary
1 ipset add test 2.1.0.0
# Network: Delete element not added to the set
1 ipset -D test 2.0.0.2
# Network: Add element in the middle
0 ipset -A test 2.0.0.128 timeout 4
# Network: Delete the same element
0 ipset -D test 2.0.0.128
# Network: List set
0 ipset list test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo
# Network: Check listing
0 diff -u -I 'Size in memory.*' .foo bitmap:ip.t.list5
# Sleep 5s so that entries can time out
0 sleep 5s
# Network: List set
0 ipset list test | grep -v Revision: > .foo
# Network: Check listing
0 diff -u -I 'Size in memory.*' .foo bitmap:ip.t.list1
# Network: Flush test set
0 ipset flush test
# Network: Delete test set
0 ipset destroy test
# Subnets: Create a set to store networks with timeout
0 ipset create test bitmap:ip range 10.0.0.0/8 netmask 24 timeout 5
# Subnets: Add lower boundary
0 ipset add test 10.0.0.0 timeout 4
# Subnets: Add upper boundary
0 ipset add test 10.255.255.255 timeout 0
# Subnets: Test lower boundary
0 ipset test test 10.0.0.255
# Subnets: Test upper boundary
0 ipset test test 10.255.255.0
# Subnets: Test element not added to the set
1 ipset test test 10.1.0.0
# Subnets: Test element before lower boundary
1 ipset test test 9.255.255.255
# Subnets: Test element after upper boundary
1 ipset test test 11.0.0.0
# Subnets: Try to add element before lower boundary
1 ipset add test 9.255.255.255
# Subnets: Try to add element after upper boundary
1 ipset add test 11.0.0.0
# Subnets: Try to delete element not added to the set
1 ipset -D test 10.2.0.0
# Subnets: Add element to the set
0 ipset -A test 10.2.0.0
# Subnets: Delete the same element from the set
0 ipset -D test 10.2.0.0
# Subnets: Add a subnet of subnets
0 ipset -A test 10.8.0.0/16 timeout 4
# Subnets: List set
0 ipset list test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo
# Subnets: Check listing
0 diff -u -I 'Size in memory.*' .foo bitmap:ip.t.list6
# Sleep 5s so that entries can time out
0 sleep 5s
# Subnets: List set
0 ipset list test | grep -v Revision: > .foo
# Subnets: Check listing
0 diff -u -I 'Size in memory.*' .foo bitmap:ip.t.list2
# Subnets: Flush test set
0 ipset flush test
# Subnets: Delete test set
0 ipset destroy test
# Full: Create full IPv4 space with /16 networks and timeout
0 ipset create test bitmap:ip range 0.0.0.0/0 netmask 16 timeout 5
# Full: Add lower boundary
0 ipset add test 0.0.255.255 timeout 0
# Full: Add upper boundary
0 ipset add test 255.255.0.0 timeout 0
# Full: Test lower boundary
0 ipset test test 0.0.0.0
# Full: Test upper boundary
0 ipset test test 255.255.255.255
# Full: Test element not added to the set
1 ipset test test 0.1.0.0
# Full: List set
0 ipset list test | grep -v Revision: > .foo
# Full: Check listing
0 diff -u -I 'Size in memory.*' .foo bitmap:ip.t.list3
# Full: flush set
0 ipset flush test
# Full: add element with 1s timeout
0 ipset add test 1.1.1.1 timeout 1
# Full: readd element with 3s timeout
0 ipset add test 1.1.1.1 timeout 3 -exist
# Full: sleep 2s
0 sleep 2s
# Full: check readded element
0 ipset test test 1.1.1.1
# Full: Delete test set
0 ipset destroy test
# Counters: create set
0 ipset n test bitmap:ip range 2.0.0.1-2.1.0.0 counters
# Counters: add element with packet, byte counters
0 ipset a test 2.0.0.1 packets 5 bytes 3456
# Counters: check element
0 ipset t test 2.0.0.1
# Counters: check counters
0 ./check_counters test 2.0.0.1 5 3456
# Counters: delete element
0 ipset d test 2.0.0.1
# Counters: test deleted element
1 ipset t test 2.0.0.1
# Counters: add element with packet, byte counters
0 ipset a test 2.0.0.10 packets 12 bytes 9876
# Counters: check counters
0 ./check_counters test 2.0.0.10 12 9876
# Counters: update counters
0 ipset -! a test 2.0.0.10 packets 13 bytes 12479
# Counters: check counters
0 ./check_counters test 2.0.0.10 13 12479
# Counters: destroy set
0 ipset x test
# Counters and timeout: create set
0 ipset n test bitmap:ip range 2.0.0.1-2.1.0.0 counters timeout 600
# Counters and timeout: add element with packet, byte counters
0 ipset a test 2.0.0.1 packets 5 bytes 3456
# Counters and timeout: check element
0 ipset t test 2.0.0.1
# Counters and timeout: check counters
0 ./check_extensions test 2.0.0.1 600 5 3456
# Counters and timeout: delete element
0 ipset d test 2.0.0.1
# Counters and timeout: test deleted element
1 ipset t test 2.0.0.1
# Counters and timeout: add element with packet, byte counters
0 ipset a test 2.0.0.10 packets 12 bytes 9876
# Counters and timeout: check counters
0 ./check_extensions test 2.0.0.10 600 12 9876
# Counters and timeout: update counters
0 ipset -! a test 2.0.0.10 packets 13 bytes 12479
# Counters and timeout: check counters
0 ./check_extensions test 2.0.0.10 600 13 12479
# Counters and timeout: update timeout
0 ipset -! a test 2.0.0.10 timeout 700
# Counters and timeout: check counters
0 ./check_extensions test 2.0.0.10 700 13 12479
# Counters and timeout: destroy set
0 ipset x test
# Counters: require sendip
skip which sendip
# Counters: create set
0 ipset n test bitmap:ip range 10.255.0.0/16 counters
# Counters: add elemet with zero counters
0 ipset a test 10.255.255.64
# Counters: generate packets
0 ./check_sendip_packets -4 src 5
# Counters: check counters
0 ./check_counters test 10.255.255.64 5 $((5*40))
# Counters: destroy set
0 ipset x test
# Counters and timeout: create set
0 ipset n test bitmap:ip range 10.255.0.0/16 counters timeout 600
# Counters and timeout: add elemet with zero counters
0 ipset a test 10.255.255.64
# Counters and timeout: generate packets
0 ./check_sendip_packets -4 src 6
# Counters and timeout: check counters
0 ./check_extensions test 10.255.255.64 600 6 $((6*40))
# Counters and timeout: destroy set
0 ipset x test
# eof
