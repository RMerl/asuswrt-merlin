# Create dummy set
0 ipset -N dummy list:set
# Create base set foo
0 ipset -N foo ipmap --from 2.0.0.1 --to 2.1.0.0
# Create base set bar
0 ipset -N bar iphash
# Create setlist kind of set
0 ipset -N test setlist
# Swap test and dumy sets
0 ipset -W test dummy
# Destroy dummy set
0 ipset -X dummy
# Add foo set to setlist
0 ipset -A test foo
# Test foo set in setlist
0 ipset -T test foo
# Test nonexistent set in setlist
1 ipset -T test nonexistent
# Try to delete foo set
1 ipset -X foo
# Add bar set to setlist, after foo
0 ipset -A test bar
# Test bar,after,foo
0 ipset -T test bar,after,foo
# Test foo,before,bar
0 ipset -T test foo,before,bar
# Test bar,before,foo
1 ipset -T test bar,before,foo
# Test foo,after,bar
1 ipset -T test foo,after,bar
# Save sets
0 ipset -S > setlist.t.r
# Delete bar,before,foo
1 ipset -D test bar,before,foo
# Delete foo,after,bar
1 ipset -D test foo,after,bar
# Delete bar,after,foo
0 ipset -D test bar,after,foo
# Flush test set
0 ipset -F test
# Delete test set
0 ipset -X test
# Delete all sets
0 ipset -X
# Restore saved sets
0 ipset -R < setlist.t.r
# List set
0 ipset -L test | grep -v Revision: > .foo
# Check listing
0 diff -u -I 'Size in memory.*' .foo setlist.t.list0
# Flush all sets
0 ipset -F
# Delete all sets
0 ipset -X && rm setlist.t.r
# Create sets a, b, c to check before/after in all combinations
0 ipset restore < setlist.t.before
# Add set b to test set
0 ipset add test b
# Add set c after b
0 ipset add test c after b
# Add set a before b
0 ipset add test a before b
# List test set
0 ipset list test | grep -v Revision: > .foo
# Check listing
0 diff -u -I 'Size in memory.*' .foo setlist.t.list1
# Test a set before b
0 ipset test test a before b
# Test c set after b
0 ipset test test c after b
# Delete b set before c
0 ipset del test b before c
# List test set
0 ipset list test | grep -v Revision: > .foo
# Check listing
0 diff -u -I 'Size in memory.*' .foo setlist.t.list2
# Delete c set after a
0 ipset del test c after a
# List test set
0 ipset list test | grep -v Revision: > .foo
# Check listing
0 diff -u -I 'Size in memory.*' .foo setlist.t.list3
# List all sets
0 ipset list | grep -v Revision: > .foo
# Check listing
0 diff -u -I 'Size in memory.*' .foo setlist.t.list4
# Flush sets
0 ipset flush
# Destroy sets
0 ipset destroy
# Restore list:set with timeout
0 ipset -R < setlist.t.restore
# Add set "before" last one
0 ipset add test e before d
# Try to add already added set
1 ipset add test a
# Check reference number of added set
0 ref=`ipset list a | grep References | sed 's/References: //'` && test $ref -eq 1
# Try to add already added set with exist flag
0 ipset add test a -!
# Check reference number of added set
0 ref=`ipset list a | grep References | sed 's/References: //'` && test $ref -eq 1
# Delete set from the set
0 ipset del test a
# Check reference number of deleted set
0 ref=`ipset list a | grep References | sed 's/References: //'` && test $ref -eq 0
# Add element to set a
0 ipset add a 1.1.1.1
# Swap sets
0 ipset swap a b
# Check reference number of deleted set
0 ref=`ipset list a | grep References | sed 's/References: //'` && test $ref -eq 0
# Check reference number of member set
0 ref=`ipset list b | grep References | sed 's/References: //'` && test $ref -eq 1
# Check element in member set
0 ipset test b 1.1.1.1
# Sleep 10s so that entries can time out
0 sleep 10
# Check reference numbers of the sets
0 ref=`ipset list | grep 'References: 1' | wc -l` && test $ref -eq 0
# Flush test set
0 ipset flush test
# Add element with 1s timeout
0 ipset add test a timeout 1
# Readd element with 3s timeout
0 ipset add test a timeout 3 -exist
# Sleep 2s
# Check readded element
0 ipset test test a
# Flush all sets
0 ipset flush
# Delete test set
0 ipset x test
# Counters: create set
0 ipset n test list:set counters
# Counters: add element with packet, byte counters
0 ipset a test a packets 5 bytes 3456
# Counters: check counters
0 ./check_counters test a 5 3456
# Counters: delete element
0 ipset d test a
# Counters: add element with packet, byte counters
0 ipset a test b packets 12 bytes 9876
# Counters: check counters
0 ./check_counters test b 12 9876
# Counters: update counters
0 ipset -! a test b packets 13 bytes 12479
# Counters: check counters
0 ./check_counters test b 13 12479
# Counters: flush test set
0 ipset f test
# Counters: destroy set
0 ipset x test
# Counters and timeout: create set
0 ipset n test list:set counters timeout 600
# Counters and timeout: add element with packet, byte counters
0 ipset a test a packets 5 bytes 3456
# Counters and timeout: check counters
0 ./check_extensions test a 600 5 3456
# Counters and timeout: add element with packet, byte counters
0 ipset a test b packets 12 bytes 9876
# Counters and timeout: check counters
0 ./check_extensions test b 600 12 9876
# Counters and timeout: update counters
0 ipset -! a test b packets 13 bytes 12479
# Counters and timeout: check counters
0 ./check_extensions test b 600 13 12479
# Counters and timeout: update timeout
0 ipset -! a test b timeout 700
# Counters and timeout: check counters
0 ./check_extensions test b 700 13 12479
# Counters and timeout: flush
0 ipset f test
# Counters and timeout: destroy sets
0 ipset x
# Counters: require sendip
skip which sendip >/dev/null
# Counters: create set
0 ipset n a hash:ip counters
# Counters: create list set
0 ipset n test list:set counters
# Counters: add elemet with zero counters
0 ipset a a 10.255.255.64
# Counters: add set to test set
0 ipset a test a
# Counters: generate packets
0 ./check_sendip_packets -4 src 5
# Counters: check counters
0 ./check_counters a 10.255.255.64 5 $((5*40))
# Counters: check counters in list set
0 ./check_counters test a 5 $((5*40))
# Counters: flush sets
0 ipset f
# Counters: destroy sets
0 ipset x
# Counters and timeout: create set
0 ipset n a hash:ip counters timeout 600
# Counters and timeout: create list set
0 ipset n test list:set counters timeout 600
# Counters and timeout: add elemet with zero counters
0 ipset a a 10.255.255.64
# Counters: add set to test set
0 ipset a test a
# Counters and timeout: generate packets
0 ./check_sendip_packets -4 src 6
# Counters and timeout: check counters
0 ./check_extensions a 10.255.255.64 600 6 $((6*40))
# Counters and timeout: check counters in list set
0 ./check_extensions test a 600 6 $((6*40))
# Counters and timeout: flush sets
0 ipset f
# Counters and timeout: destroy sets
0 ipset x
# eof
