# Range: Try to create from an invalid range
2 ipset -N test ipportnethash --from 2.0.0.1 --to 2.1.0.1
# Range: Create a set from a valid range
0 ipset -N test ipportnethash --from 2.0.0.1 --to 2.1.0.0
# Range: Add zero valued element
2 ipset -A test 2.0.0.1,0,0.0.0.0/0
# Range: Test zero valued element
2 ipset -T test 2.0.0.1,0,0.0.0.0/0
# Range: Delete zero valued element
2 ipset -D test 2.0.0.1,0,0.0.0.0/0
# Range: Add almost zero valued element
0 ipset -A test 2.0.0.1,0,0.0.0.0/24
# Range: Test almost zero valued element
0 ipset -T test 2.0.0.1,0,0.0.0.0/24
# Range: Delete almost zero valued element
0 ipset -D test 2.0.0.1,0,0.0.0.0/24
# Range: Add lower boundary
0 ipset -A test 2.0.0.1,5,1.1.1.1/24
# Range: Add upper boundary
0 ipset -A test 2.1.0.0,128,2.2.2.2/12
# Range: Test lower boundary
0 ipset -T test 2.0.0.1,5,1.1.1.2
# Range: Test upper boundary
0 ipset -T test 2.1.0.0,128,2.2.2.0
# Range: Test value not added to the set
1 ipset -T test 2.0.0.1,5,1.1.0.255
# Range: Test value not added to the set
1 ipset -T test 2.0.0.1,6,1.1.1.1
# Range: Test value not added to the set
1 ipset -T test 2.0.0.2,6,1.1.1.1
# Range: Test value before lower boundary
1 ipset -T test 2.0.0.0,5,1.1.1.1
# Range: Test value after upper boundary
1 ipset -T test 2.1.0.1,128,2.2.2.2
# Range: Try to add value before lower boundary
1 ipset -A test 2.0.0.0,5,1.1.1.1/24
# Range: Try to add value after upper boundary
1 ipset -A test 2.1.0.1,128,2.2.2.2/12
# Range: List set
0 ipset -L test > .foo0 && ./sort.sh .foo0
# Range: Check listing
0 diff .foo ipportnethash.t.list0 && rm .foo
# Range: Flush test set
0 ipset -F test
# Range: Delete test set
0 ipset -X test
# Network: Try to create a set from an invalid network
2 ipset -N test ipportnethash --network 2.0.0.0/15
# Network: Create a set from a valid network
0 ipset -N test ipportnethash --network 2.0.0.0/16
# Network: Add lower boundary
0 ipset -A test 2.0.0.0,5,1.1.1.1/24
# Network: Add upper boundary
0 ipset -A test 2.0.255.255,128,2.2.2.2/12
# Network: Test lower boundary
0 ipset -T test 2.0.0.0,5,1.1.1.2
# Network: Test upper boundary
0 ipset -T test 2.0.255.255,128,2.2.2.0
# Network: Test value not added to the set
1 ipset -T test 2.0.0.0,5,1.1.0.255
# Network: Test value not added to the set
1 ipset -T test 2.0.0.0,6,1.1.1.1
# Network: Test value before lower boundary
1 ipset -T test 1.255.255.255,5,1.1.1.1
# Network: Test value after upper boundary
1 ipset -T test 2.1.0.0,128,2.2.2.2
# Network: Try to add value before lower boundary
1 ipset -A test 1.255.255.255,5,1.1.1.1/24
# Network: Try to add value after upper boundary
1 ipset -A test 2.1.0.0,128,2.2.2.2/12
# Network: List set
0 ipset -L test > .foo0 && ./sort.sh .foo0
# Network: Check listing
0 diff .foo ipportnethash.t.list1 && rm .foo
# Network: Flush test set
0 ipset -F test
# Network: Delete test set
0 ipset -X test
# eof
