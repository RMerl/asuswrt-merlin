# Range: Try to create from an invalid range
2 ipset -N test ipporthash --from 2.0.0.1 --to 2.1.0.1
# Range: Create a set from a valid range
0 ipset -N test ipporthash --from 2.0.0.1 --to 2.1.0.0
# Range: Add zero valued element
1 ipset -A test 2.0.0.1,0
# Range: Test zero valued element
1 ipset -T test 2.0.0.1,0
# Range: Delete zero valued element
1 ipset -D test 2.0.0.1,0
# Range: Add lower boundary
0 ipset -A test 2.0.0.1,5
# Range: Add upper boundary
0 ipset -A test 2.1.0.0,128
# Range: Test lower boundary
0 ipset -T test 2.0.0.1,5
# Range: Test upper boundary
0 ipset -T test 2.1.0.0,128
# Range: Test value not added to the set
1 ipset -T test 2.0.0.1,4
# Range: Test value not added to the set
1 ipset -T test 2.0.0.1,6
# Range: Test value before lower boundary
1 ipset -T test 2.0.0.0,5
# Range: Test value after upper boundary
1 ipset -T test 2.1.0.1,128
# Range: Try to add value before lower boundary
1 ipset -A test 2.0.0.0,5
# Range: Try to add value after upper boundary
1 ipset -A test 2.1.0.1,128
# Range: List set
0 ipset -L test > .foo0 && ./sort.sh .foo0
# Range: Check listing
0 diff .foo ipporthash.t.list0 && rm .foo
# Range: Flush test set
0 ipset -F test
# Range: Delete test set
0 ipset -X test
# Network: Try to create a set from an invalid network
2 ipset -N test ipporthash --network 2.0.0.0/15
# Network: Create a set from a valid network
0 ipset -N test ipporthash --network 2.0.0.0/16
# Network: Add lower boundary
0 ipset -A test 2.0.0.0,5
# Network: Add upper boundary
0 ipset -A test 2.0.255.255,128
# Network: Test lower boundary
0 ipset -T test 2.0.0.0,5
# Network: Test upper boundary
0 ipset -T test 2.0.255.255,128
# Network: Test value not added to the set
1 ipset -T test 2.0.0.0,4
# Network: Test value not added to the set
1 ipset -T test 2.0.0.0,6
# Network: Test value before lower boundary
1 ipset -T test 1.255.255.255,5
# Network: Test value after upper boundary
1 ipset -T test 2.1.0.0,128
# Network: Try to add value before lower boundary
1 ipset -A test 1.255.255.255,5
# Network: Try to add value after upper boundary
1 ipset -A test 2.1.0.0,128
# Network: List set
0 ipset -L test > .foo0 && ./sort.sh .foo0
# Network: Check listing
0 diff .foo ipporthash.t.list1 && rm .foo
# Network: Flush test set
0 ipset -F test
# Network: Delete test set
0 ipset -X test
# eof
