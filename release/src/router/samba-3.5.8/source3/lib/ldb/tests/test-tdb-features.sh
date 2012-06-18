#!/bin/sh

echo "Running tdb feature tests"

mv $LDB_URL $LDB_URL.2

checkcount() {
    count=$1
    expression="$2"
    n=`bin/ldbsearch "$expression" | grep '^dn' | wc -l`
    if [ $n != $count ]; then
	echo "Got $n but expected $count for $expression"
	$VALGRIND bin/ldbsearch "$expression"
	exit 1
    fi
    echo "OK: $count $expression"
}

echo "Testing case sensitive search"
cat <<EOF | $VALGRIND bin/ldbadd || exit 1
dn: cn=t1,cn=TEST
objectClass: testclass
test: foo
EOF
checkcount 1 '(test=foo)'
checkcount 0 '(test=FOO)'
checkcount 0 '(test=FO*)'

echo "Making case insensitive"
cat <<EOF | $VALGRIND bin/ldbmodify || exit 1
dn: @ATTRIBUTES
changetype: add
add: test
test: CASE_INSENSITIVE
EOF

echo $ldif | $VALGRIND bin/ldbmodify || exit 1
checkcount 1 '(test=foo)'
checkcount 1 '(test=FOO)'
checkcount 1 '(test=fo*)'

echo "adding i"
cat <<EOF | $VALGRIND bin/ldbmodify || exit 1
dn: cn=t1,cn=TEST
changetype: modify
add: i
i: 0x100
EOF
checkcount 1 '(i=0x100)'
checkcount 0 '(i=256)'

echo "marking i as INTEGER"
cat <<EOF | $VALGRIND bin/ldbmodify || exit 1
dn: @ATTRIBUTES
changetype: modify
add: i
i: INTEGER
EOF
checkcount 1 '(i=0x100)'
checkcount 1 '(i=256)'

echo "adding j"
cat <<EOF | $VALGRIND bin/ldbmodify || exit 1
dn: cn=t1,cn=TEST
changetype: modify
add: j
j: 0x100
EOF
checkcount 1 '(j=0x100)'
checkcount 0 '(j=256)'

echo "Adding wildcard attribute"
cat <<EOF | $VALGRIND bin/ldbmodify || exit 1
dn: @ATTRIBUTES
changetype: modify
add: *
*: INTEGER
EOF
checkcount 1 '(j=0x100)'
checkcount 1 '(j=256)'

echo "Testing class search"
checkcount 0 '(objectClass=otherclass)'
checkcount 1 '(objectClass=testclass)'

echo "Adding subclass"
cat <<EOF | $VALGRIND bin/ldbmodify || exit 1
dn: @SUBCLASSES
changetype: add
add: otherclass
otherclass: testclass
EOF
checkcount 1 '(objectClass=otherclass)'
checkcount 1 '(objectClass=testclass)'

echo "Adding index"
cat <<EOF | $VALGRIND bin/ldbadd || exit 1
dn: @INDEXLIST
@IDXATTR: i
@IDXATTR: test
EOF
checkcount 1 '(i=0x100)'
checkcount 1 '(i=256)'
checkcount 0 '(i=-256)'
checkcount 1 '(test=foo)'
checkcount 1 '(test=FOO)'
checkcount 1 '(test=*f*o)'

echo "making test case sensitive"
cat <<EOF | $VALGRIND bin/ldbmodify || exit 1
dn: @ATTRIBUTES
changetype: modify
replace: test
test: NONE
EOF
checkcount 1 '(test=foo)'
checkcount 0 '(test=FOO)'
checkcount 1 '(test=f*o*)'

