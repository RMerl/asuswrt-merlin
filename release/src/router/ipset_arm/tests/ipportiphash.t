# Create a set from a range (range ignored)
0 ipset -N test ipportiphash --from 2.0.0.1 --to 2.1.0.0
# Destroy set
0 ipset -X test
# Create a set
0 ipset -N test ipportiphash
# Add partly zero valued element
0 ipset -A test 2.0.0.1,0,0.0.0.0
# Test partly zero valued element
0 ipset -T test 2.0.0.1,0,0.0.0.0
# Delete party zero valued element
0 ipset -D test 2.0.0.1,0,0.0.0.0
# Add almost zero valued element
0 ipset -A test 2.0.0.1,0,0.0.0.1
# Test almost zero valued element
0 ipset -T test 2.0.0.1,0,0.0.0.1
# Delete almost zero valued element
0 ipset -D test 2.0.0.1,0,0.0.0.1
# Add first random value
0 ipset -A test 2.0.0.1,5,1.1.1.1
# Add second random value
0 ipset -A test 2.1.0.0,128,2.2.2.2
# Test first random value
0 ipset -T test 2.0.0.1,5,1.1.1.1
# Test second random value
0 ipset -T test 2.1.0.0,128,2.2.2.2
# Test value not added to the set
1 ipset -T test 2.0.0.1,5,1.1.1.2
# Test value not added to the set
1 ipset -T test 2.0.0.1,6,1.1.1.1
# Test value not added to the set
1 ipset -T test 2.0.0.2,6,1.1.1.1
# Test value before first random value
1 ipset -T test 2.0.0.0,5,1.1.1.1
# Test value after second random value
1 ipset -T test 2.1.0.1,128,2.2.2.2
# Try to add value before first random value
0 ipset -A test 2.0.0.0,5,1.1.1.1
# Try to add value after second random value
0 ipset -A test 2.1.0.1,128,2.2.2.2
# List set
0 ipset -L test | grep -v Revision: > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -u -I 'Size in memory.*' .foo ipportiphash.t.list0
# Flush test set
0 ipset -F test
# Delete test set
0 ipset -X test
# Create a set from a valid network (network ignored)
0 ipset -N test ipportiphash --network 2.0.0.0/16
# Add first random value
0 ipset -A test 2.0.0.0,5,1.1.1.1
# Add second random value
0 ipset -A test 2.0.255.255,128,2.2.2.2
# Test first random value
0 ipset -T test 2.0.0.0,5,1.1.1.1
# Test second random value
0 ipset -T test 2.0.255.255,128,2.2.2.2
# Test value not added to the set
1 ipset -T test 2.0.0.0,5,1.1.1.2
# Test value not added to the set
1 ipset -T test 2.0.0.0,6,1.1.1.1
# Test value before first random value
1 ipset -T test 1.255.255.255,5,1.1.1.1
# Test value after second random value
1 ipset -T test 2.1.0.0,128,2.2.2.2
# Try to add value before first random value
0 ipset -A test 1.255.255.255,5,1.1.1.1
# Try to test value before first random value
0 ipset -T test 1.255.255.255,5,1.1.1.1
# Try to del value before first random value
0 ipset -D test 1.255.255.255,5,1.1.1.1
# List set
0 ipset -L test | grep -v Revision: > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -u -I 'Size in memory.*' .foo ipportiphash.t.list1
# Flush test set
0 ipset -F test
# Delete test set
0 ipset -X test
# eof
