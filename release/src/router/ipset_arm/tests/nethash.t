# Create a set
0 ipset -N test nethash --hashsize 128
# Add zero valued element
1 ipset -A test 0.0.0.0/0
# Test zero valued element
1 ipset -T test 0.0.0.0/0
# Delete zero valued element
1 ipset -D test 0.0.0.0/0
# Try to add /0
1 ipset -A test 1.1.1.1/0
# Try to add /32
0 ipset -A test 1.1.1.1/32
# Add almost zero valued element
0 ipset -A test 0.0.0.0/1
# Test almost zero valued element
0 ipset -T test 0.0.0.0/1
# Delete almost zero valued element
0 ipset -D test 0.0.0.0/1
# Test deleted element
1 ipset -T test 0.0.0.0/1
# Delete element not added to the set
1 ipset -D test 0.0.0.0/1
# Add first random network
0 ipset -A test 2.0.0.1/24
# Add second random network
0 ipset -A test 192.168.68.69/27
# Test first random value
0 ipset -T test 2.0.0.255
# Test second random value
0 ipset -T test 192.168.68.95
# Test value not added to the set
1 ipset -T test 2.0.1.0
# Try to add IP address
0 ipset -A test 2.0.0.1
# List set
0 ipset -L test | grep -v Revision: > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -u -I 'Size in memory.*' .foo nethash.t.list0
# Flush test set
0 ipset -F test
# Add a non-matching IP address entry
0 ipset -A test 1.1.1.1 nomatch
# Add an overlapping matching small net
0 ipset -A test 1.1.1.0/30
# Add an overlapping non-matching larger net
0 ipset -A test 1.1.1.0/28 nomatch
# Add an even larger matching net
0 ipset -A test 1.1.1.0/26
# Check non-matching IP
1 ipset -T test 1.1.1.1
# Check matching IP from non-matchin small net
0 ipset -T test 1.1.1.3
# Check non-matching IP from larger net
1 ipset -T test 1.1.1.4
# Check matching IP from even larger net
0 ipset -T test 1.1.1.16
# Update non-matching IP to matching one
0 ipset -! -A test 1.1.1.1
# Delete overlapping small net
0 ipset -D test 1.1.1.0/30
# Check matching IP
0 ipset -T test 1.1.1.1
# Update matching IP as a non-matching one
0 ipset -! -A test 1.1.1.1 nomatch
# Check non-matching IP
1 ipset -T test 1.1.1.1
# Delete test set
0 ipset -X test
# eof
