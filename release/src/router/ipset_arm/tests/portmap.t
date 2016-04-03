# Range: Create a set from a valid range
0 ipset -N test portmap --from 1 --to 1024
# Range: Add lower boundary
0 ipset -A test 1
# Range: Add upper boundary
0 ipset -A test 1024
# Range: Test lower boundary
0 ipset -T test 1
# Range: Test upper boundary
0 ipset -T test 1024
# Range: Test value not added to the set
1 ipset -T test 1023
# Range: Test value before lower boundary
1 ipset -T test 0
# Range: Test value after upper boundary
1 ipset -T test 1025
# Range: Try to add value before lower boundary
1 ipset -A test 0
# Range: Try to add value after upper boundary
1 ipset -A test 1025
# Range: Delete element not added to the set
1 ipset -D test 567
# Range: Add element in the middle
0 ipset -A test 567
# Range: Delete the same element
0 ipset -D test 567
# Range: List set
0 ipset -L test | grep -v Revision: > .foo
# Range: Check listing
0 diff -u -I 'Size in memory.*' .foo portmap.t.list0
# Range: Flush test set
0 ipset -F test
# Range: Delete test set
0 ipset -X test
# Full: Create a full set of ports
0 ipset -N test portmap --from 0 --to 65535
# Full: Add lower boundary
0 ipset -A test 0
# Full: Add upper boundary
0 ipset -A test 65535
# Full: Test lower boundary
0 ipset -T test 0
# Full: Test upper boundary
0 ipset -T test 65535
# Full: Test value not added to the set
1 ipset -T test 1
# Full: List set
0 ipset -L test | grep -v Revision: > .foo
# Full: Check listing
0 diff -u -I 'Size in memory.*' .foo portmap.t.list1
# Full: Flush test set
0 ipset -F test
# Full: Delete test set
0 ipset -X test
# Full: Create a full set of ports and timeout
0 ipset -N test portmap --from 0 --to 65535 timeout 5
# Full: Add lower boundary
0 ipset -A test 0 timeout 5
# Full: Add upper boundary
0 ipset -A test 65535 timeout 0
# Full: Test lower boundary
0 ipset -T test 0
# Full: Test upper boundary
0 ipset -T test 65535
# Full: Test value not added to the set
1 ipset -T test 1
# Full: Add element in the middle
0 ipset -A test 567
# Full: Delete the same element
0 ipset -D test 567
# Full: List set
0 ipset -L test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo
# Full: Check listing
0 diff -u -I 'Size in memory.*' .foo portmap.t.list3
# Full: sleep 5s so that elements can timeout
0 sleep 5
# Full: List set
0 ipset -L test | grep -v Revision: > .foo
# Full: Check listing
# 0 diff -u -I 'Size in memory.*' .foo portmap.t.list2
# Full: Flush test set
0 ipset -F test
# Full: add element with 1s timeout
0 ipset add test 567 timeout 1
# Full: readd element with 3s timeout
0 ipset add test 567 timeout 3 -exist
# Full: sleep 2s
0 sleep 2s
# Full: check readded element
0 ipset test test 567
# Full: Delete test set
0 ipset -X test
# Counters: create set
0 ipset n test bitmap:port range 1024-65535 counters
# Counters: add element with packet, byte counters
0 ipset a test 12345 packets 5 bytes 3456
# Counters: check element
0 ipset t test 12345
# Counters: check counters
0 ./check_counters test 12345 5 3456
# Counters: delete element
0 ipset d test 12345
# Counters: test deleted element
1 ipset t test 12345
# Counters: add element with packet, byte counters
0 ipset a test 48310 packets 12 bytes 9876
# Counters: check counters
0 ./check_counters test 48310 12 9876
# Counters: update counters
0 ipset -! a test 48310 packets 13 bytes 12479
# Counters: check counters
0 ./check_counters test 48310 13 12479
# Counters: destroy set
0 ipset x test
# Counters and timeout: create set
0 ipset n test bitmap:port range 1024-65535 counters timeout 600
# Counters and timeout: add element with packet, byte counters
0 ipset a test 12345 packets 5 bytes 3456
# Counters and timeout: check element
0 ipset t test 12345
# Counters and timeout: check counters
0 ./check_extensions test 12345 600 5 3456
# Counters and timeout: delete element
0 ipset d test 12345
# Counters and timeout: test deleted element
1 ipset t test 12345
# Counters and timeout: add element with packet, byte counters
0 ipset a test 48310 packets 12 bytes 9876
# Counters and timeout: check counters
0 ./check_extensions test 48310 600 12 9876
# Counters and timeout: update counters
0 ipset -! a test 48310 packets 13 bytes 12479
# Counters and timeout: check counters
0 ./check_extensions test 48310 600 13 12479
# Counters and timeout: update timeout
0 ipset -! a test 48310 timeout 700
# Counters and timeout: check counters
0 ./check_extensions test 48310 700 13 12479
# Counters and timeout: destroy set
0 ipset x test
# eof
