# IP: Create a set
0 ipset -N test iphash --hashsize 128
# Range: Add zero valued element
1 ipset -A test 0.0.0.0
# Range: Test zero valued element
1 ipset -T test 0.0.0.0
# IP: Add first random value
0 ipset -A test 2.0.0.1
# IP: Add second random value
0 ipset -A test 192.168.68.69
# IP: Test first random value
0 ipset -T test 2.0.0.1
# IP: Test second random value
0 ipset -T test 192.168.68.69
# IP: Test value not added to the set
1 ipset -T test 2.0.0.2
# IP: Add third random value
0 ipset -A test 200.100.0.12
# IP: Delete the same value
0 ipset -D test 200.100.0.12
# IP: Delete element not added to the set
1 ipset -D test 200.100.0.12
# IP: Delete element not added to the set, ignoring error
0 ipset -! -D test 200.100.0.12
# IP: List set
0 ipset -L test 2>/dev/null | grep -v Revision: > .foo0 && ./sort.sh .foo0
# IP: Check listing
0 diff -u -I 'Size in memory.*' .foo iphash.t.list0
# IP: Flush test set
0 ipset -F test
# IP: Delete test set
0 ipset -X test
# IP: Restore values so that rehashing is triggered, old format
0 ipset -R < iphash.t.restore.old
# IP: Check that all values are restored
0 (grep add iphash.t.restore | sort > .foo.1) && (ipset -S test | grep add | sort > .foo.2) && cmp .foo.1 .foo.2
# IP: Delete test set
0 ipset -X test
# IP: Restore values so that rehashing is triggered
0 ipset -R < iphash.t.restore
# IP: Check that all values are restored
0 (grep add iphash.t.restore | sort > .foo.1) && (ipset -S test | grep add | sort > .foo.2) && cmp .foo.1 .foo.2
# IP: Flush test set
0 ipset -F test
# IP: Delete test set
0 ipset -X test
# IP: Restore, which requires multiple messages
0 ipset restore < iphash.t.large
# IP: Save the restored set
0 ipset save test | sort > .foo.1
# IP: Compare save and restore
0 (sort iphash.t.large > .foo.2) && (cmp .foo.1 .foo.2)
# IP: Delete test set
0 ipset x test
# Network: Create a set
0 ipset -N test iphash --hashsize 128 --netmask 24
# Network: Add zero valued element
1 ipset -A test 0.0.0.0
# Network: Test zero valued element
1 ipset -T test 0.0.0.0
# Network: Delete zero valued element
1 ipset -D test 0.0.0.0
# Network: Add first random network
0 ipset -A test 2.0.0.1
# Network: Add second random network
0 ipset -A test 192.168.68.69
# Network: Test first random value
0 ipset -T test 2.0.0.255
# Network: Test second random value
0 ipset -T test 192.168.68.95
# Network: Test value not added to the set
1 ipset -T test 2.0.1.0
# Network: Add third random network
0 ipset -A test 200.100.0.12
# Network: Delete the same network
0 ipset -D test 200.100.0.12
# Network: Delete element not added to the set
1 ipset -D test 200.100.0.12
# Network: List set
0 ipset -L test | grep -v Revision: > .foo0 && ./sort.sh .foo0
# Network: Check listing
0 diff -u -I 'Size in memory.*' .foo iphash.t.list1
# Network: Flush test set
0 ipset -F test
# Network: Delete test set
0 ipset -X test
# eof
