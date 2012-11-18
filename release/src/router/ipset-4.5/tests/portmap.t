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
# Range: List set
0 ipset -L test > .foo
# Range: Check listing
0 diff .foo portmap.t.list0 && rm .foo
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
0 ipset -L test > .foo
# Full: Check listing
0 diff .foo portmap.t.list1 && rm .foo
# Full: Flush test set
0 ipset -F test
# Full: Delete test set
0 ipset -X test
# eof
