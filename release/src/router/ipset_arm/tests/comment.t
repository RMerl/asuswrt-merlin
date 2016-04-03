# Bitmap comment: create set
0 ipset create test bitmap:ip range 2.0.0.1-2.1.0.0 comment
# Bitmap comment: Check invalid length of comment
1 ipset add test 2.0.0.1 comment "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
# Bitmap comment: Add comment with max length
0 ipset add test 2.0.0.1 comment "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
# Bitmap comment: Add element with comment
0 ipset -! add test 2.0.0.1 comment "text message"
# Bitmap comment: Test element with comment
0 ipset test test 2.0.0.1
# Bitmap comment: List set
0 ipset list test | grep -v Revision: > .foo
# Bitmap comment: Check listing
0 diff -u -I 'Size in memory.*' .foo comment.t.list0
# Bitmap comment: Delete element with comment
0 ipset del test 2.0.0.1
# Bitmap comment: Test deleted element
1 ipset test test 2.0.0.1
# Bitmap comment: Check listing
1 ipset list test | grep 'comment "text message"' >/dev/null
# Bitmap comment: Add element with comment
0 ipset add test 2.0.0.1 comment "text message"
# Bitmap comment: check listing
0 ipset list test | grep 'comment "text message"' >/dev/null
# Bitmap comment: Re-add element with another comment
0 ipset -! add test 2.0.0.1 comment "text message 2"
# Bitmap comment: check listing
0 ipset list test | grep 'comment "text message 2"' >/dev/null
# Bitmap comment: Flush test set
0 ipset flush test
# Bitmap comment: Add multiple elements with comment
0 for x in `seq 1 255`; do echo "add test 2.0.0.$x comment \\\"text message $x\\\""; done | ipset restore
# Bitmap comment: List set
0 ipset list test | grep -v Revision: > .foo
# Bitmap comment: Check listing
0 diff -u -I 'Size in memory.*' .foo comment.t.list1
# Bitmap comment: Delete test set
0 ipset destroy test
# Bitmap comment: create set with timeout
0 ipset create test bitmap:ip range 2.0.0.1-2.1.0.0 comment timeout 5
# Bitmap comment: Add multiple elements with default timeout
0 for x in `seq 1 255`; do echo "add test 2.0.0.$x comment \\\"text message $x\\\""; done | ipset restore
# Bitmap comment: Add multiple elements with zero timeout
0 for x in `seq 1 255`; do echo "add test 2.0.1.$x timeout 0 comment \\\"text message $x\\\""; done | ipset restore
# Bitmap comment: List set
0 ipset list test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo
# Bitmap comment: Check listing
0 diff -u -I 'Size in memory.*' .foo comment.t.list11
# Sleep 5s so that entries can time out
0 sleep 5s
# Bitmap comment: List set
0 ipset list test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo
# Bitmap comment: Check listing
0 diff -u -I 'Size in memory.*' .foo comment.t.list12
# Bitmap comment: Flush set
0 ipset flush test
# Bitmap comment: Delete test set
0 ipset destroy test
# Hash comment: Create a set
0 ipset create test hash:net,net hashsize 128 comment
# Hash comment: Check invalid length of comment
1 ipset add test 1.1.1.1/32,1.1.1.2/32 comment "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
# Hash comment: Add comment with max length
0 ipset add test 1.1.1.1/32,1.1.1.2/32 comment "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
# Hash comment: Try to add /32
0 ipset -! add test 1.1.1.1/32,1.1.1.2/32 comment "text 1.1.1.1/32,1.1.1.2/32"
# Hash comment: Add almost zero valued element
0 ipset add test 0.0.0.0/1,0.0.0.0/1 comment "text 0.0.0.0/1,0.0.0.0/1"
# Hash comment: Test almost zero valued element
0 ipset test test 0.0.0.0/1,0.0.0.0/1
# Hash comment: Delete almost zero valued element
0 ipset del test 0.0.0.0/1,0.0.0.0/1
# Hash comment: Test deleted element
1 ipset test test 0.0.0.0/1,0.0.0.0/1
# Hash comment: Delete element not added to the set
1 ipset del test 0.0.0.0/1,0.0.0.0/1
# Hash comment: Add first random network
0 ipset add test 2.0.0.1/24,2.0.1.1/24 comment "text 2.0.0.1/24,2.0.1.1/24"
# Hash comment: Add second random network
0 ipset add test 192.168.68.69/27,192.168.129.69/27 comment "text 192.168.68.69/27,192.168.129.69/27"
# Hash comment: Test first random value
0 ipset test test 2.0.0.255,2.0.1.255
# Hash comment: Test second random value
0 ipset test test 192.168.68.95,192.168.129.75
# Hash comment: Test value not added to the set
1 ipset test test 2.0.1.0,2.0.0.1
# Hash comment: Try to add IP address
0 ipset add test 2.0.0.1,2.0.0.2 
# Hash comment: List set
0 ipset list test | grep -v Revision: > .foo0 && ./sort.sh .foo0
# Hash comment: Check listing
0 diff -u -I 'Size in memory.*' .foo comment.t.list2
# Hash comment: Update element with comment
0 ipset -! add test 2.0.0.1,2.0.0.2 comment "text 2.0.0.1,2.0.0.2"
# Hash comment: Check updated element
0 ipset list test | grep '"text 2.0.0.1,2.0.0.2"' >/dev/null 
# Hash comment: Update another element with comment
0 ipset -! add test 2.0.0.1/24,2.0.1.1/24 comment "text 2.0.0.1/24\,2.0.1.1/24 new"
# Hash comment: Check updated element
0 ipset list test | grep '"text 2.0.0.1/24,2.0.1.1/24 new"' >/dev/null 
# Hash comment: Flush test set
0 ipset flush test
# Hash comment: Delete test set
0 ipset destroy test
# Hash comment: Stress test with comments
0 ./netnetgen.sh comment | ipset restore
# Hash comment: List set and check the number of elements
0 n=`ipset -L test|grep '^10.'|wc -l` && test $n -eq 87040
# Hash comment: Delete test set
0 ipset destroy test
# Hash comment: Check that resizing keeps the comments
0 ./resizec.sh -4 netnet
# Hash comment: Stress test with comments and timeout
0 ./netnetgen.sh comment timeout | ipset restore
# Hash comment: List set and check the number of elements
0 n=`ipset -L test|grep '^10.'|wc -l` && test $n -eq 87040
# Hash comment: Destroy test set
0 ipset destroy test
# Hash comment: create set with timeout
0 ipset create test hash:ip comment timeout 5
# Hash comment: Add multiple elements with default timeout
0 for x in `seq 0 255`; do echo "add test 2.0.0.$x comment \\\"text message $x\\\""; done | ipset restore
# Hash comment: Add multiple elements with zero timeout
0 for x in `seq 0 255`; do echo "add test 2.0.1.$x timeout 0 comment \\\"text message $x\\\""; done | ipset restore
# Hash comment: List set
0 ipset list test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo0 && ./sort.sh .foo0
# Hash comment: Check listing
0 diff -u -I 'Size in memory.*' .foo comment.t.list21
# Sleep 6s so that entries can time out
0 sleep 6s
# Hash comment: List set
0 ipset list test | grep -v Revision: | sed 's/timeout ./timeout x/' > .foo0 && ./sort.sh .foo0
# Hash comment: Check listing
0 diff -u -I 'Size in memory.*' .foo comment.t.list22
# Hash comment: Flush set
0 ipset flush test
# Hash comment: Delete test set
0 ipset destroy test
# List comment: Create a, b, c sets
0 for x in a b c; do ipset n $x hash:ip; done
# List comment: Create test set with comment
0 ipset n test list:set comment
# List comment: Add c set with comment
0 ipset a test c comment "c set comment"
# List comment: Add a set with comment
0 ipset a test a before c comment "a set comment"
# List comment: Add b set with comment
0 ipset a test b after a comment "b set comment"
# List comment: List sets
0 ipset list | grep -v Revision: > .foo
# List comment: Check listing
0 diff -u -I 'Size in memory.*' .foo comment.t.list3
# Flush sets
0 ipset f
# Destroy sets
0 ipset x
# eof
