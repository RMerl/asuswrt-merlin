# Range: Create a set from a range (range ignored)
0 ipset -N test ipportnethash --from 2.0.0.1 --to 2.1.0.0
# Range: Add zero valued element
1 ipset -A test 2.0.0.1,0,0.0.0.0/0
# Range: Test zero valued element
1 ipset -T test 2.0.0.1,0,0.0.0.0/0
# Range: Delete zero valued element
1 ipset -D test 2.0.0.1,0,0.0.0.0/0
# Range: Add almost zero valued element
0 ipset -A test 2.0.0.1,0,0.0.0.0/24
# Range: Test almost zero valued element
0 ipset -T test 2.0.0.1,0,0.0.0.0/24
# Range: Delete almost zero valued element
0 ipset -D test 2.0.0.1,0,0.0.0.0/24
# Range: Add first random value
0 ipset -A test 2.0.0.1,5,1.1.1.1/24
# Range: Add second random value
0 ipset -A test 2.1.0.0,128,2.2.2.2/12
# Range: Test first random value
0 ipset -T test 2.0.0.1,5,1.1.1.2
# Range: Test second random value
0 ipset -T test 2.1.0.0,128,2.2.2.0
# Range: Test value not added to the set
1 ipset -T test 2.0.0.1,5,1.1.0.255
# Range: Test value not added to the set
1 ipset -T test 2.0.0.1,6,1.1.1.1
# Range: Test value not added to the set
1 ipset -T test 2.0.0.2,6,1.1.1.1
# Range: Test value before first random value
1 ipset -T test 2.0.0.0,5,1.1.1.1
# Range: Test value after second random value
1 ipset -T test 2.1.0.1,128,2.2.2.2
# Range: Try to add value before first random value
0 ipset -A test 2.0.0.0,5,1.1.1.1/24
# Range: Try to add value after second random value
0 ipset -A test 2.1.0.1,128,2.2.2.2/12
# Range: List set
0 ipset -L test | grep -v Revision: > .foo0 && ./sort.sh .foo0
# Range: Check listing
0 diff -u -I 'Size in memory.*' .foo ipportnethash.t.list0
# Range: Flush test set
0 ipset -F test
# Range: Delete test set
0 ipset -X test
# Network: Create a set from a valid network (network ignored)
0 ipset -N test ipportnethash --network 2.0.0.0/16
# Network: Add first random value
0 ipset -A test 2.0.0.0,5,1.1.1.1/24
# Network: Add second random value
0 ipset -A test 2.0.255.255,128,2.2.2.2/12
# Network: Test first random value
0 ipset -T test 2.0.0.0,5,1.1.1.2
# Network: Test second random value
0 ipset -T test 2.0.255.255,128,2.2.2.0
# Network: Test value not added to the set
1 ipset -T test 2.0.0.0,5,1.1.0.255
# Network: Test value not added to the set
1 ipset -T test 2.0.0.0,6,1.1.1.1
# Network: Test value before first random value
1 ipset -T test 1.255.255.255,5,1.1.1.1
# Network: Test value after second random value
1 ipset -T test 2.1.0.0,128,2.2.2.2
# Network: Try to add value before first random value
0 ipset -A test 1.255.255.255,5,1.1.1.1/24
# Network: Try to add value after second random value
0 ipset -A test 2.1.0.0,128,2.2.2.2/12
# Network: List set
0 ipset -L test | grep -v Revision: > .foo0 && ./sort.sh .foo0
# Network: Check listing
0 diff -u -I 'Size in memory.*' .foo ipportnethash.t.list1
# Network: Flush test set
0 ipset -F test
# Add a non-matching IP address entry
0 ipset -A test 2.1.0.0,80,1.1.1.1 nomatch
# Add an overlapping matching small net
0 ipset -A test 2.1.0.0,80,1.1.1.0/30
# Add an overlapping non-matching larger net
0 ipset -A test 2.1.0.0,80,1.1.1.0/28 nomatch
# Add an even larger matching net
0 ipset -A test 2.1.0.0,80,1.1.1.0/26
# Check non-matching IP
1 ipset -T test 2.1.0.0,80,1.1.1.1
# Check matching IP from non-matchin small net
0 ipset -T test 2.1.0.0,80,1.1.1.3
# Check non-matching IP from larger net
1 ipset -T test 2.1.0.0,80,1.1.1.4
# Check matching IP from even larger net
0 ipset -T test 2.1.0.0,80,1.1.1.16
# Update non-matching IP to matching one
0 ipset -! -A test 2.1.0.0,80,1.1.1.1
# Delete overlapping small net
0 ipset -D test 2.1.0.0,80,1.1.1.0/30
# Check matching IP
0 ipset -T test 2.1.0.0,80,1.1.1.1
# Update matching IP as a non-matching one
0 ipset -! -A test 2.1.0.0,80,1.1.1.1 nomatch
# Check non-matching IP
1 ipset -T test 2.1.0.0,80,1.1.1.1
# Delete test set
0 ipset -X test
# eof
