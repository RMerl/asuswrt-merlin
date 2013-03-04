# Range: Try to create from an invalid range
2 ipset -N test ipmap --from 2.0.0.1 --to 2.1.0.1
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
# Range: List set
0 ipset -L test > .foo
# Range: Check listing
0 diff .foo ipmap.t.list0 && rm .foo
# Range: Flush test set
0 ipset -F test
# Range: Delete test set
0 ipset -X test
# Network: Try to create a set from an invalid network
2 ipset -N test ipmap --network 2.0.0.0/15
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
0 diff .foo ipmap.t.list1 && rm .foo
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
# Subnets: Test value not added to the set
1 ipset -T test 10.1.0.0
# Subnets: Test value before lower boundary
1 ipset -T test 9.255.255.255
# Subnets: Test value after upper boundary
1 ipset -T test 11.0.0.0
# Subnets: Try to add value before lower boundary
1 ipset -A test 9.255.255.255
# Subnets: Try to add value after upper boundary
1 ipset -A test 11.0.0.0
# Subnets: List set
0 ipset -L test > .foo
# Subnets: Check listing
0 diff .foo ipmap.t.list2 && rm .foo
# Subnets: FLush test set
0 ipset -F test
# Subnets: Delete test set
0 ipset -X test
# Full: Create full IPv4 space with /16 networks
0 ipset -N test ipmap --network 0.0.0.0/0 --netmask 16
# Full: Add lower boundary
0 ipset -A test 0.0.255.255
# Full: Add upper boundary
0 ipset -A test 255.255.0.0
# Full: Test lower boundary
0 ipset -T test 0.0.0.0
# Full: Test upper boundary
0 ipset -T test 255.255.255.255
# Full: Test value not added to the set
1 ipset -T test 0.1.0.0
# Full: List set
0 ipset -L test > .foo
# Full: Check listing
0 diff .foo ipmap.t.list3 && rm .foo
# Full: Delete test set
0 ipset -X test
# eof
