#!/bin/sh

echo "Running tdb feature tests"

mv $LDB_URL $LDB_URL.2

checkcount() {
    count=$1
    expression="$2"
    n=`$VALGRIND ldbsearch$EXEEXT "$expression" | grep '^dn' | wc -l`
    if [ $n != $count ]; then
	echo "Got $n but expected $count for $expression"
	$VALGRIND ldbsearch$EXEEXT "$expression"
	exit 1
    fi
    echo "OK: $count $expression"
}

echo "Testing case sensitive search"
cat <<EOF | $VALGRIND ldbadd$EXEEXT || exit 1
dn: cn=t1,cn=TEST
objectClass: testclass
test: foo
EOF
checkcount 1 '(test=foo)'
checkcount 0 '(test=FOO)'
checkcount 0 '(test=FO*)'

echo "Making case insensitive"
cat <<EOF | $VALGRIND ldbmodify$EXEEXT || exit 1
dn: @ATTRIBUTES
changetype: add
add: test
test: CASE_INSENSITIVE
EOF

echo $ldif | $VALGRIND ldbmodify$EXEEXT || exit 1
checkcount 1 '(test=foo)'
checkcount 1 '(test=FOO)'
checkcount 1 '(test=fo*)'

echo "adding i"
cat <<EOF | $VALGRIND ldbmodify$EXEEXT || exit 1
dn: cn=t1,cn=TEST
changetype: modify
add: i
i: 0x100
EOF
checkcount 1 '(i=0x100)'
checkcount 0 '(i=256)'

echo "marking i as INTEGER"
cat <<EOF | $VALGRIND ldbmodify$EXEEXT || exit 1
dn: @ATTRIBUTES
changetype: modify
add: i
i: INTEGER
EOF
checkcount 1 '(i=0x100)'
checkcount 1 '(i=256)'

echo "adding j"
cat <<EOF | $VALGRIND ldbmodify$EXEEXT || exit 1
dn: cn=t1,cn=TEST
changetype: modify
add: j
j: 0x100
EOF
checkcount 1 '(j=0x100)'
checkcount 0 '(j=256)'

echo "Adding wildcard attribute"
cat <<EOF | $VALGRIND ldbmodify$EXEEXT || exit 1
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

echo "Adding index"
cat <<EOF | $VALGRIND ldbadd$EXEEXT || exit 1
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
cat <<EOF | $VALGRIND ldbmodify$EXEEXT || exit 1
dn: @ATTRIBUTES
changetype: modify
replace: test
test: NONE
EOF
checkcount 1 '(test=foo)'
checkcount 0 '(test=FOO)'
checkcount 1 '(test=f*o*)'

checkone() {
    count=$1
    base="$2"
    expression="$3"
    n=`$VALGRIND ldbsearch$EXEEXT -s one -b "$base" "$expression" | grep '^dn' | wc -l`
    if [ $n != $count ]; then
	echo "Got $n but expected $count for $expression"
	$VALGRIND ldbsearch$EXEEXT -s one -b "$base" "$expression"
	exit 1
    fi
    echo "OK: $count $expression"
}

echo "Removing wildcard attribute"
cat <<EOF | $VALGRIND ldbmodify$EXEEXT || exit 1
dn: @ATTRIBUTES
changetype: modify
delete: *
*: INTEGER
EOF

echo "Adding one level indexes"
cat <<EOF | $VALGRIND ldbmodify$EXEEXT || exit 1
dn: @INDEXLIST
changetype: modify
add: @IDXONE
@IDXONE: 1
EOF

echo "Testing one level indexed search"
cat <<EOF | $VALGRIND ldbadd$EXEEXT || exit 1
dn: cn=one,cn=t1,cn=TEST
objectClass: oneclass
cn: one
test: one
EOF
checkone 1 "cn=t1,cn=TEST" '(test=one)'
cat <<EOF | $VALGRIND ldbadd$EXEEXT || exit 1
dn: cn=two,cn=t1,cn=TEST
objectClass: oneclass
cn: two
test: one

dn: cn=three,cn=t1,cn=TEST
objectClass: oneclass
cn: three
test: one
EOF
checkone 3 "cn=t1,cn=TEST" '(test=one)'
checkone 1 "cn=t1,cn=TEST" '(cn=two)'

