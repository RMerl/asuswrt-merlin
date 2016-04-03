# Create a set from a range (range ignored)
0 ipset -N test ipporthash --from 2.0.0.1 --to 2.1.0.0
# Destroy set
0 ipset -X test
# Create a set
0 ipset -N test ipporthash
# Add partly zero valued element
0 ipset -A test 2.0.0.1,0
# Test partly zero valued element
0 ipset -T test 2.0.0.1,0
# Delete partly zero valued element
0 ipset -D test 2.0.0.1,0
# Add first random value
0 ipset -A test 2.0.0.1,5
# Add second random value
0 ipset -A test 2.1.0.0,128
# Test first random value
0 ipset -T test 2.0.0.1,5
# Test second random value
0 ipset -T test 2.1.0.0,128
# Test value not added to the set
1 ipset -T test 2.0.0.1,4
# Delete value not added to the set
1 ipset -D test 2.0.0.1,6
# Test value before first random value
1 ipset -T test 2.0.0.0,5
# Test value after second random value
1 ipset -T test 2.1.0.1,128
# Try to add value before first random value
0 ipset -A test 2.0.0.0,5
# Try to add value after second random value
0 ipset -A test 2.1.0.1,128
# List set
0 ipset -L test | grep -v Revision: > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -u -I 'Size in memory.*' .foo ipporthash.t.list0
# Flush test set
0 ipset -F test
# Delete test set
0 ipset -X test
# Create a set from a network (network ignored)
0 ipset -N test ipporthash --network 2.0.0.0/16
# Add first random value
0 ipset -A test 2.0.0.0,5
# Add second random value
0 ipset -A test 2.0.255.255,128
# Test first random value
0 ipset -T test 2.0.0.0,5
# Test second random value
0 ipset -T test 2.0.255.255,128
# Test value not added to the set
1 ipset -T test 2.0.0.0,4
# Delete value not added to the set
1 ipset -D test 2.0.0.0,6
# Test value before first random value
1 ipset -T test 1.255.255.255,5
# Test value after second random value
1 ipset -T test 2.1.0.0,128
# Try to add value before first random value
0 ipset -A test 1.255.255.255,5
# Try to add value after second random value
0 ipset -A test 2.1.0.0,128
# List set
0 ipset -L test | grep -v Revision: > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -u -I 'Size in memory.*' .foo ipporthash.t.list1
# Flush test set
0 ipset -F test
# Delete test set
0 ipset -X test
# eof
