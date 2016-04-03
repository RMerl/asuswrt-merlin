# Create a set from a range (range ignored)
0 ipset -N test ipmarkhash --from 2.0.0.1 --to 2.1.0.0
# Destroy set
0 ipset -X test
# Create a set
0 ipset -N test ipmarkhash
# Add partly zero valued element
0 ipset -A test 2.0.0.1,0x0
# Test partly zero valued element
0 ipset -T test 2.0.0.1,0x0
# Delete partly zero valued element
0 ipset -D test 2.0.0.1,0x0
# Add first random value
0 ipset -A test 2.0.0.1,0x5
# Add second random value
0 ipset -A test 2.1.0.0,0x80
# Test first random value
0 ipset -T test 2.0.0.1,0x5
# Test second random value
0 ipset -T test 2.1.0.0,0x80
# Test value not added to the set
1 ipset -T test 2.0.0.1,0x4
# Delete value not added to the set
1 ipset -D test 2.0.0.1,0x6
# Test value before first random value
1 ipset -T test 2.0.0.0,0x5
# Test value after second random value
1 ipset -T test 2.1.0.1,0x80
# Try to add value before first random value
0 ipset -A test 2.0.0.0,0x5
# Try to add value after second random value
0 ipset -A test 2.1.0.1,0x80
# List set
0 ipset -L test | grep -v Revision: > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -u -I 'Size in memory.*' .foo ipmarkhash.t.list0
# Flush test set
0 ipset -F test
# Delete test set
0 ipset -X test
# Create a set from a network (network ignored)
0 ipset -N test ipmarkhash --network 2.0.0.0/16
# Add first random value
0 ipset -A test 2.0.0.0,0x5
# Add second random value
0 ipset -A test 2.0.255.255,0x80
# Test first random value
0 ipset -T test 2.0.0.0,0x5
# Test second random value
0 ipset -T test 2.0.255.255,0x80
# Test value not added to the set
1 ipset -T test 2.0.0.0,0x4
# Delete value not added to the set
1 ipset -D test 2.0.0.0,0x6
# Test value before first random value
1 ipset -T test 1.255.255.255,0x5
# Test value after second random value
1 ipset -T test 2.1.0.0,0x80
# Try to add value before first random value
0 ipset -A test 1.255.255.255,0x5
# Try to add value after second random value
0 ipset -A test 2.1.0.0,0x80
# List set
0 ipset -L test | grep -v Revision: > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -u -I 'Size in memory.*' .foo ipmarkhash.t.list1
# Flush test set
0 ipset -F test
# Delete test set
0 ipset -X test
# eof
