# Create a set without timeout
0 ipset -N test iptreemap
# Add first random IP entry
0 ipset -A test 2.0.0.1
# Add second random IP entry
0 ipset -A test 192.168.68.69
# Test first random IP entry
0 ipset -T test 2.0.0.1
# Test second random IP entry
0 ipset -T test 192.168.68.69
# Test value not added to the set
1 ipset -T test 2.0.0.2
# Test value not added to the set
1 ipset -T test 192.168.68.70
# Add IP range
0 ipset -A test 3.0.0.0-3.0.0.2
# Test the three members of the range: first
0 ipset -T test 3.0.0.0
# Test the three members of the range: second
0 ipset -T test 3.0.0.1
# Test the three members of the range: third
0 ipset -T test 3.0.0.2
# Delete the middle of the range
0 ipset -D test 3.0.0.1
# Test the range: first
0 ipset -T test 3.0.0.0
# Test the range: second
1 ipset -T test 3.0.0.1
# Test the range: third
0 ipset -T test 3.0.0.2
# Delete second random IP entry
0 ipset -D test 192.168.68.69
# Add a network block
0 ipset -A test 192.168.68.69/27
# Test the lower bound of the network
0 ipset -T test 192.168.68.64
# Test the upper bound of the network
0 ipset -T test 192.168.68.95
# Test element from the middle
0 ipset -T test 192.168.68.71
# Delete a network from the middle
0 ipset -D test 192.168.68.70/30
# Test lower bound of deleted network
1 ipset -T test 192.168.68.68
# Test upper bound of deleted network
1 ipset -T test 192.168.68.71
# Test element before lower bound of deleted network
0 ipset -T test 192.168.68.67
# Test element after upper bound of deleted network
0 ipset -T test 192.168.68.72
# List set
0 ipset -L test | grep -v Revision: > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -u -I 'Size in memory.*' .foo iptreemap.t.list0
# Flush test set
0 ipset -F test
# Delete test set
0 ipset -X test
# eof
