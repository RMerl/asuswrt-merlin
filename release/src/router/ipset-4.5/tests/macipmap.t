# Range: Try to create from an invalid range
2 ipset -N test macipmap --from 2.0.0.1 --to 2.1.0.1
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
# Range: Try to add value with MAC
0 ipset -A test 2.0.0.2,00:11:22:33:44:55
# Range: Test value with invalid MAC
1 ipset -T test 2.0.0.2,00:11:22:33:44:56
# Range: Test value with valid MAC
0 ipset -T test 2.0.0.2,00:11:22:33:44:55
# Range: List set
0 ipset -L test > .foo
# Range: Check listing
0 diff .foo macipmap.t.list0 && rm .foo
# Range: Flush test set
0 ipset -F test
# Range: Delete test set
0 ipset -X test
# Network: Try to create a set from an invalid network
2 ipset -N test macipmap --network 2.0.0.0/15
# Network: Create a set from a valid network
0 ipset -N test macipmap --network 2.0.0.0/16
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
# Network: List set
0 ipset -L test > .foo
# Network: Check listing
0 diff .foo macipmap.t.list1 && rm .foo
# Network: Flush test set
0 ipset -F test
# Network: Delete test set
0 ipset -X test
# eof
