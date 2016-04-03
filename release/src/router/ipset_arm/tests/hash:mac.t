# MAC: Create a set with timeout
0 ipset -N test machash --hashsize 128 timeout 5
# Range: Add zero valued element
1 ipset -A test 0:0:0:0:0:0
# Range: Test zero valued element
1 ipset -T test 0:0:0:0:0:0
# MAC: Add first random value
0 ipset -A test 0:0:0:0:2:0 timeout 5
# MAC: Add second random value
0 ipset -A test 0:a:0:0:0:0 timeout 0
# MAC: Test first random value
0 ipset -T test 0:0:0:0:2:0
# MAC: Test second random value
0 ipset -T test 0:a:0:0:0:0
# MAC: Test value not added to the set
1 ipset -T test 0:0:0:0:1:0
# MAC: Add third random value
0 ipset -A test 1:2:3:4:a:b
# MAC: Delete the same value
0 ipset -D test 1:2:3:4:a:b
# MAC: List set
0 ipset -L test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo0 && ./sort.sh .foo0
# MAC: Check listing
0 diff -u -I 'Size in memory.*' .foo hash:mac.t.list2
# Sleep 5s so that element can time out
0 sleep 5
# MAC: List set
0 ipset -L test 2>/dev/null | grep -v Revision: > .foo0 && ./sort.sh .foo0
# MAC: Check listing
0 diff -u -I 'Size in memory.*' .foo hash:mac.t.list0
# MAC: Destroy test set
0 ipset -X test
# MAC: Create a set with skbinfo extension
0 ipset n test hash:mac skbinfo
# MAC: Add element with mark 
0 ipset a test 1:2:3:4:5:6 skbmark 0x123456
# MAC: Add element with mark/mask
0 ipset a test 1:2:3:4:5:7 skbmark 0x1234ab/0x0000ffff
# MAC: Add element with skbprio
0 ipset a test 1:2:3:4:5:8 skbprio 1:20
# MAC: Add element with skbqueue
0 ipset a test 1:2:3:4:5:9 skbqueue 11
# MAC: Add element with mark and skbprio
0 ipset a test 1:2:3:4:5:10 skbmark 0xaabbccdd skbprio 22:1
# MAC: Add element with mark, skbprio and skbqueue
0 ipset a test 1:2:3:4:5:11 skbmark 0x11223344/0xffff0000 skbprio 2:1 skbqueue 8 
# MAC: List set
0 ipset -L test 2>/dev/null | grep -v Revision: > .foo0 && ./sort.sh .foo0
# MAC: Check listing
0 diff -u -I 'Size in memory.*' .foo hash:mac.t.list1
# MAC: Destroy test set
0 ipset -X test
# MAC: Create a set with small maxelem parameter
0 ipset n test hash:mac maxelem 2 skbinfo
# MAC: Add first element
0 ipset a test 1:2:3:4:5:6 skbprio 1:10
# MAC: Add second element
0 ipset a test 1:2:3:4:5:7 skbprio 1:11
# MAC: Add third element
1 ipset a test 1:2:3:4:5:8 skbprio 1:12
# MAC: Add second element again
1 ipset a test 1:2:3:4:5:7 skbprio 1:11
# MAC: Add second element with another extension value
0 ipset -! a test 1:2:3:4:5:7 skbprio 1:12 skbqueue 8
# MAC: List set
0 ipset -L test 2>/dev/null | grep -v Revision: > .foo0 && ./sort.sh .foo0
# MAC: Check listing
0 diff -u -I 'Size in memory.*' .foo hash:mac.t.list3
# MAC: Destroy test set
0 ipset x test
# eof
