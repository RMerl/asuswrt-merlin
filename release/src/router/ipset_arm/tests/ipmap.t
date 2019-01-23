# Range: Check syntax error: missing range/from-to
2 ipset -N test ipmap
# Range: Check syntax error: missing --from
2 ipset -N test ipmap --to 2.1.0.1
# Range: Check syntax error: missing --to
2 ipset -N test ipmap --from 2.1.0.1
# Range: Catch invalid IPv4 address
1 ipset -N test ipmap --from 2.0.0.256 --to 2.1.0.1
# Range: Try to create from an invalid range
1 ipset -N test ipmap --from 2.0.0.1 --to 2.1.0.1
# Range: Create a set from a valid range
0 ipset -N test ipmap --from 2.0.0.1 --to 2.1.0.0
# Range: Add lower boundary
0 ipset -A test 2.0.0.1
# Range: Add upper boundary
0 ipset -A test 2.1.0.0
# Range: Test lower boundary
0 ipset -T test 2.0.0.1
# Range: Test upper boundary
0 ipset -T test 2.1.0.0
# Range: Test element not added to the set
1 ipset -T test 2.0.0.2
# Range: Test element before lower boundary
1 ipset -T test 2.0.0.0
# Range: Test element after upper boundary
1 ipset -T test 2.1.0.1
# Range: Try to add element before lower boundary
1 ipset -A test 2.0.0.0
# Range: Try to add element after upper boundary
1 ipset -A test 2.1.0.1
# Range: Delete element not added to the set
1 ipset -D test 2.0.0.2
# Range: Add element in the middle
0 ipset -A test 2.0.0.128
# Range: Delete the same element
0 ipset -D test 2.0.0.128
# Range: Add a range of elements
0 ipset -A test 2.0.0.128-2.0.0.131
# Range: Save set
0 ipset -S test > ipmap.t.restore
# Range: Destroy set
0 ipset -X test
# Range: Restore set and catch error
1 sed 's/2.0.0.131/222.0.0.131/' < ipmap.t.restore | ipset -R
# Range: Check returned error line number
0 num=`grep 'in line' < .foo.err | sed 's/.* in line //' | cut -d : -f 1` && test $num -eq 6
# Range: Destroy set
0 ipset -X test
# Range: Restore set
0 ipset -R < ipmap.t.restore && rm ipmap.t.restore
# Range: List set
0 ipset -L test | grep -v Revision: > .foo
# Range: Check listing
0 diff -u -I 'Size in memory.*' .foo ipmap.t.list0
# Range: Delete a range of elements
0 ipset -! -D test 2.0.0.128-2.0.0.132
# Range: List set
0 ipset -L test | grep -v Revision: > .foo
# Range: Check listing
0 diff -u -I 'Size in memory.*' .foo ipmap.t.list1
# Range: Flush test set
0 ipset -F test
# Range: Delete test set
0 ipset -X test
# Network: Try to create a set from an invalid network
1 ipset -N test ipmap --network 2.0.0.0/15
# Network: Create a set from a valid network
0 ipset -N test ipmap --network 2.0.0.0/16
# Network: Add lower boundary
0 ipset -A test 2.0.0.0
# Network: Add upper boundary
0 ipset -A test 2.0.255.255
# Network: Test lower boundary
0 ipset -T test 2.0.0.0
# Network: Test upper boundary
0 ipset -T test 2.0.255.255
# Network: Test element not added to the set
1 ipset -T test 2.0.0.1
# Network: Test element before lower boundary
1 ipset -T test 1.255.255.255
# Network: Test element after upper boundary
1 ipset -T test 2.1.0.0
# Network: Try to add element before lower boundary
1 ipset -A test 1.255.255.255
# Network: Try to add element after upper boundary
1 ipset -A test 2.1.0.0
# Network: Delete element not added to the set
1 ipset -D test 2.0.0.2
# Network: Add element in the middle
0 ipset -A test 2.0.0.128
# Network: Delete the same element
0 ipset -D test 2.0.0.128
# Network: List set
0 ipset -L test | grep -v Revision: > .foo
# Network: Check listing
0 diff -u -I 'Size in memory.*' .foo ipmap.t.list2
# Network: Flush test set
0 ipset -F test
# Network: Delete test set
0 ipset -X test
# Subnets: Create a set to store networks
0 ipset -N test ipmap --network 10.0.0.0/8 --netmask 24
# Subnets: Add lower boundary
0 ipset -A test 10.0.0.0
# Subnets: Add upper boundary
0 ipset -A test 10.255.255.255
# Subnets: Test lower boundary
0 ipset -T test 10.0.0.255
# Subnets: Test upper boundary
0 ipset -T test 10.255.255.0
# Subnets: Test element not added to the set
1 ipset -T test 10.1.0.0
# Subnets: Test element before lower boundary
1 ipset -T test 9.255.255.255
# Subnets: Test element after upper boundary
1 ipset -T test 11.0.0.0
# Subnets: Try to add element before lower boundary
1 ipset -A test 9.255.255.255
# Subnets: Try to add element after upper boundary
1 ipset -A test 11.0.0.0
# Subnets: Try to delete element not added to the set
1 ipset -D test 10.2.0.0
# Subnets: Add element to the set
0 ipset -A test 10.2.0.0
# Subnets: Delete the same element from the set
0 ipset -D test 10.2.0.0
# Subnets: Add a subnet of subnets
0 ipset -A test 10.8.0.0/16
# Subnets: List set
0 ipset -L test | grep -v Revision: > .foo
# Subnets: Check listing
0 diff -u -I 'Size in memory.*' .foo ipmap.t.list3
# Subnets: FLush test set
0 ipset -F test
# Subnets: Delete test set
0 ipset -X test
# Full: Create full IPv4 space with /16 networks
0 ipset -N test ipmap --network 0.0.0.0/0 --netmask 16
# Full: Add lower boundary
0 ipset -A test 0.0.0.0
# Full: Add upper boundary
0 ipset -A test 255.255.0.0
# Full: Test lower boundary
0 ipset -T test 0.0.0.0
# Full: Test upper boundary
0 ipset -T test 255.255.255.255
# Full: Test element not added to the set
1 ipset -T test 0.1.0.0
# Full: Delete element not added to the set
1 ipset -T test 0.1.0.0
# Full: Add element to the set
0 ipset -A test 0.1.0.0
# Full: Delete same element
0 ipset -D test 0.1.0.0
# Full: List set
0 ipset -L test | grep -v Revision: > .foo
# Full: Check listing
0 diff -u -I 'Size in memory.*' .foo ipmap.t.list4
# Full: Delete test set
0 ipset -X test
# eof
