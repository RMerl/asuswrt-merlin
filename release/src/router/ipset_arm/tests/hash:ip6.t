# IP: Create a set with timeout
0 ipset -N test iphash -6 --hashsize 128 timeout 5
# IP: Add zero valued element
1 ipset -A test ::
# IP: Test zero valued element
1 ipset -T test ::
# IP: Delete zero valued element
1 ipset -D test ::
# IP: Add first random value
0 ipset -A test 2:0:0::1 timeout 5
# IP: Add second random value
0 ipset -A test 192:168:68::69 timeout 0
# IP: Test first random value
0 ipset -T test 2:0:0::1
# IP: Test second random value
0 ipset -T test 192:168:68::69
# IP: Test value not added to the set
1 ipset -T test 2:0:0::2
# IP: Add third random value
0 ipset -A test 200:100:0::12
# IP: Delete the same value
0 ipset -D test 200:100:0::12
# IP: List set
0 ipset -L test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo0 && ./sort.sh .foo0
# IP: Check listing
0 diff -u -I 'Size in memory.*' .foo hash:ip6.t.list2
# IP: Save set
0 ipset save test > hash:ip6.t.restore
# Sleep 5s so that element can time out
0 sleep 5
# IP: List set
0 ipset -L test 2>/dev/null | grep -v Revision: > .foo0 && ./sort.sh .foo0
# IP: Check listing
0 diff -u -I 'Size in memory.*' .foo hash:ip6.t.list0
# IP: Destroy set
0 ipset x test
# IP: Restore saved set
0 ipset restore < hash:ip6.t.restore && rm hash:ip6.t.restore
# IP: List set
0 ipset -L test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo0 && ./sort.sh .foo0
# IP: Check listing
0 diff -u -I 'Size in memory.*' .foo hash:ip6.t.list2
# IP: Flush test set
0 ipset -F test
# IP: Try to add multiple elements in one step
1 ipset -A test 1::1-1::10
# IP: Delete test set
0 ipset -X test
# Network: Create a set with timeout
0 ipset -N test iphash -6 --hashsize 128 --netmask 64 timeout 5
# Network: Add zero valued element
1 ipset -A test ::
# Network: Test zero valued element
1 ipset -T test ::
# Network: Delete zero valued element
1 ipset -D test ::
# Network: Add first random network
0 ipset -A test 2:0:0::1
# Network: Add second random network
0 ipset -A test 192:168:68::69
# Network: Test first random value
0 ipset -T test 2:0:0::255
# Network: Test second random value
0 ipset -T test 192:168:68::95
# Network: Test value not added to the set
1 ipset -T test 4:0:1::0
# Network: Add third element
0 ipset -A test 200:100:10::1 timeout 0
# Network: Add third random network
0 ipset -A test 200:101:0::12
# Network: Delete the same network
0 ipset -D test 200:101:0::12
# Network: Test the deleted network
1 ipset -T test 200:101:0::12
# Network: List set
0 ipset -L test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo0 && ./sort.sh .foo0
# Network: Check listing
0 diff -u -I 'Size in memory.*' .foo hash:ip6.t.list3
# Sleep 5s so that elements can time out
0 sleep 5
# Network: List set
0 ipset -L test | grep -v Revision: > .foo
# Network: Check listing
0 diff -u -I 'Size in memory.*' .foo hash:ip6.t.list1
# Network: Flush test set
0 ipset -F test
# Network: Delete test set
0 ipset -X test
# Check more complex restore commands
0 ipset restore < restore.t.restore
# List restored set a
0 ipset l a | grep -v Revision: > .foo0 && ./sort.sh .foo0
# Check listing of set a
0 diff -u -I 'Size in memory.*' .foo restore.t.list0
# List restored set b
0 ipset l b | grep -v Revision: > .foo0 && ./sort.sh .foo0
# Check listing of set b
0 diff -u -I 'Size in memory.*' .foo restore.t.list1
# Destroy by restore
0 ipset restore < restore.t.destroy
# Timeout: Check that resizing keeps timeout values
0 ./resizet.sh -6 ip
# Counters: create set
0 ipset n test hash:ip -6 counters
# Counters: add element with packet, byte counters
0 ipset a test 2:0:0::1 packets 5 bytes 3456
# Counters: check element
0 ipset t test 2:0:0::1
# Counters: check counters
0 ./check_counters test 2::1 5 3456
# Counters: delete element
0 ipset d test 2:0:0::1
# Counters: test deleted element
1 ipset t test 2:0:0::1
# Counters: add element with packet, byte counters
0 ipset a test 2:0:0::20 packets 12 bytes 9876
# Counters: check counters
0 ./check_counters test 2::20 12 9876
# Counters: update counters
0 ipset -! a test 2:0:0::20 packets 13 bytes 12479
# Counters: check counters
0 ./check_counters test 2::20 13 12479
# Counters: destroy set
0 ipset x test
# Counters and timeout: create set
0 ipset n test hash:ip -6 counters timeout 600
# Counters and timeout: add element with packet, byte counters
0 ipset a test 2:0:0::1 packets 5 bytes 3456
# Counters and timeout: check element
0 ipset t test 2:0:0::1
# Counters and timeout: check counters
0 ./check_extensions test 2::1 600 5 3456
# Counters and timeout: delete element
0 ipset d test 2:0:0::1
# Counters and timeout: test deleted element
1 ipset t test 2:0:0::1
# Counters and timeout: add element with packet, byte counters
0 ipset a test 2:0:0::20 packets 12 bytes 9876
# Counters and timeout: check counters
0 ./check_extensions test 2::20 600 12 9876
# Counters and timeout: update counters
0 ipset -! a test 2:0:0::20 packets 13 bytes 12479
# Counters and timeout: check counters
0 ./check_extensions test 2::20 600 13 12479
# Counters and timeout: update timeout
0 ipset -! a test 2:0:0::20 timeout 700
# Counters and timeout: check counters
0 ./check_extensions test 2::20 700 13 12479
# Counters and timeout: destroy set
0 ipset x test
# eof
