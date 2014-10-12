#!/bin/sh

if [ $# -lt 2 ]; then
cat <<EOF
Usage: test_ldb.sh PROTOCOL SERVER [OPTIONS]
EOF
exit 1;
fi


p=$1
SERVER=$2
PREFIX=$3
shift 2
options="$*"

. `dirname $0`/subunit.sh

check() {
	name="$1"
	shift
	cmdline="$*"
	echo "test: $name"
	$cmdline
	status=$?
	if [ x$status = x0 ]; then
		echo "success: $name"
	else
		echo "failure: $name"
		failed=`expr $failed + 1`
	fi
	return $status
}

export PATH="$BUILDDIR/bin:$PATH"

ldbsearch="$VALGRIND ldbsearch$EXEEXT"

check "RootDSE" $ldbsearch $CONFIGURATION $options --basedn='' -H $p://$SERVER -s base DUMMY=x dnsHostName highestCommittedUSN || failed=`expr $failed + 1`

echo "Getting defaultNamingContext"
BASEDN=`$ldbsearch $CONFIGURATION $options --basedn='' -H $p://$SERVER -s base DUMMY=x defaultNamingContext | grep defaultNamingContext | awk '{print $2}'`
echo "BASEDN is $BASEDN"

check "Listing Users" $ldbsearch $options $CONFIGURATION -H $p://$SERVER '(objectclass=user)' sAMAccountName || failed=`expr $failed + 1`

check "Listing Users (sorted)" $ldbsearch -S $options $CONFIGURATION -H $p://$SERVER '(objectclass=user)' sAMAccountName || failed=`expr $failed + 1`

check "Listing Groups" $ldbsearch $options $CONFIGURATION -H $p://$SERVER '(objectclass=group)' sAMAccountName || failed=`expr $failed + 1`

nentries=`$ldbsearch $options -H $p://$SERVER $CONFIGURATION '(|(|(&(!(groupType:1.2.840.113556.1.4.803:=1))(groupType:1.2.840.113556.1.4.803:=2147483648)(groupType:1.2.840.113556.1.4.804:=10))(samAccountType=805306368))(samAccountType=805306369))' sAMAccountName | grep sAMAccountName | wc -l`
echo "Found $nentries entries"
if [ $nentries -lt 10 ]; then
echo "Should have found at least 10 entries"
failed=`expr $failed + 1`
fi

echo "Check rootDSE for Controls"
nentries=`$ldbsearch $options $CONFIGURATION -H $p://$SERVER -s base -b "" '(objectclass=*)' | grep -i supportedControl | wc -l`
if [ $nentries -lt 4 ]; then
echo "Should have found at least 4 entries"
failed=`expr $failed + 1`
fi

echo "Test Paged Results Control"
nentries=`$ldbsearch $options $CONFIGURATION -H $p://$SERVER --controls=paged_results:1:5 '(objectclass=user)' | grep sAMAccountName | wc -l`
if [ $nentries -lt 1 ]; then
echo "Paged Results Control test returned 0 items"
failed=`expr $failed + 1`
fi

echo "Test Server Sort Control"
nentries=`$ldbsearch $options $CONFIGURATION -H $p://$SERVER --controls=server_sort:1:0:sAMAccountName '(objectclass=user)' | grep sAMAccountName | wc -l`
if [ $nentries -lt 1 ]; then
echo "Server Sort Control test returned 0 items"
failed=`expr $failed + 1`
fi

echo "Test Extended DN Control"
nentries=`$ldbsearch $options $CONFIGURATION -H $p://$SERVER --controls=extended_dn:1 '(objectclass=user)' | grep sAMAccountName | wc -l`
if [ $nentries -lt 1 ]; then
echo "Extended DN Control test returned 0 items"
failed=`expr $failed + 1`
fi
nentries=`$ldbsearch $options $CONFIGURATION -H $p://$SERVER --controls=extended_dn:1:0 '(objectclass=user)' | grep sAMAccountName | wc -l`
if [ $nentries -lt 1 ]; then
echo "Extended DN Control test returned 0 items"
failed=`expr $failed + 1`
fi
nentries=`$ldbsearch $options $CONFIGURATION -H $p://$SERVER --controls=extended_dn:1:1 '(objectclass=user)' | grep sAMAccountName | wc -l`
if [ $nentries -lt 1 ]; then
echo "Extended DN Control test returned 0 items"
failed=`expr $failed + 1`
fi

echo "Test Domain scope Control"
nentries=`$ldbsearch $options $CONFIGURATION -H $p://$SERVER --controls=domain_scope:1 '(objectclass=user)' | grep sAMAccountName | wc -l`
if [ $nentries -lt 1 ]; then
echo "Extended Domain scope Control test returned 0 items"
failed=`expr $failed + 1`
fi

echo "Test Attribute Scope Query Control"
nentries=`$ldbsearch $options $CONFIGURATION -H $p://$SERVER --controls=asq:1:member -s base -b "CN=Administrators,CN=Builtin,$BASEDN" | grep sAMAccountName | wc -l`
if [ $nentries -lt 1 ]; then
echo "Attribute Scope Query test returned 0 items"
failed=`expr $failed + 1`
fi

echo "Test Search Options Control"
nentries=`$ldbsearch $options $CONFIGURATION -H $p://$SERVER --controls=search_options:1:2 '(objectclass=crossRef)' | grep crossRef | wc -l`
if [ $nentries -lt 1 ]; then
echo "Search Options Control Query test returned 0 items"
failed=`expr $failed + 1`
fi

echo "Test Search Options Control with Domain Scope Control"
nentries=`$ldbsearch $options $CONFIGURATION -H $p://$SERVER --controls=search_options:1:2,domain_scope:1 '(objectclass=crossRef)' | grep crossRef | wc -l`
if [ $nentries -lt 1 ]; then
echo "Search Options Control Query test returned 0 items"
failed=`expr $failed + 1`
fi

wellknown_object_test() {
	local guid=$1
	local object=$2
	local basedns
	local dn
	local r
	local c
	local n
	local failed=0

	basedns="<WKGUID=${guid},${BASEDN}> <wkGuId=${guid},${BASEDN}>"
	for dn in ${basedns}; do
		echo "Test ${dn} => ${object}"
		r=`$ldbsearch $options $CONFIGURATION -H $p://$SERVER '(objectClass=*)' -b "${dn}" | grep 'dn: '`
		n=`echo "${r}" | grep 'dn: ' | wc -l`
		c=`echo "${r}" | grep "${object}" | wc -l`

		if [ $n -lt 1 ]; then
			echo "Object not found by WKGUID"
			failed=`expr $failed + 1`
			continue
		fi
		if [ $c -lt 1 ]; then
			echo "Wrong object found by WKGUID: [${r}]"
			failed=`expr $failed + 1`
			continue
		fi
	done

	return $failed
}

wellknown_object_test 22B70C67D56E4EFB91E9300FCA3DC1AA ForeignSecurityPrincipals
st=$?
if [ x"$st" != x"0" ]; then
	failed=`expr $failed + $st`
fi
wellknown_object_test 2FBAC1870ADE11D297C400C04FD8D5CD Infrastructure
st=$?
if [ x"$st" != x"0" ]; then
	failed=`expr $failed + $st`
fi
wellknown_object_test AB1D30F3768811D1ADED00C04FD8D5CD System
st=$?
if [ x"$st" != x"0" ]; then
	failed=`expr $failed + $st`
fi
wellknown_object_test A361B2FFFFD211D1AA4B00C04FD7D83A Domain Controllers
st=$?
if [ x"$st" != x"0" ]; then
	failed=`expr $failed + $st`
fi
wellknown_object_test AA312825768811D1ADED00C04FD8D5CD Computers
st=$?
if [ x"$st" != x"0" ]; then
	failed=`expr $failed + $st`
fi
wellknown_object_test A9D1CA15768811D1ADED00C04FD8D5CD Users
st=$?
if [ x"$st" != x"0" ]; then
	failed=`expr $failed + $st`
fi

echo "Getting HEX GUID/SID of $BASEDN"
HEXDN=`$ldbsearch $CONFIGURATION $options -b "$BASEDN" -H $p://$SERVER -s base "(objectClass=*)" --controls=extended_dn:1:0 distinguishedName | grep 'distinguishedName: ' | cut -d ' ' -f2-`
HEXGUID=`echo "$HEXDN" | cut -d ';' -f1`
echo "HEXGUID[$HEXGUID]"

echo "Getting STR GUID/SID of $BASEDN"
STRDN=`$ldbsearch $CONFIGURATION $options -b "$BASEDN" -H $p://$SERVER -s base "(objectClass=*)" --controls=extended_dn:1:1 distinguishedName | grep 'distinguishedName: ' | cut -d ' ' -f2-`
echo "STRDN: $STRDN"
STRGUID=`echo "$STRDN" | cut -d ';' -f1`
echo "STRGUID[$STRGUID]"

echo "Getting STR GUID/SID of $BASEDN"
STRDN=`$ldbsearch $CONFIGURATION $options -b "$BASEDN" -H $p://$SERVER -s base "(objectClass=*)" --controls=extended_dn:1:1 | grep 'dn: ' | cut -d ' ' -f2-`
echo "STRDN: $STRDN"
STRSID=`echo "$STRDN" | cut -d ';' -f2`
echo "STRSID[$STRSID]"

SPECIALDNS="$HEXGUID $STRGUID $STRSID"
for SPDN in $SPECIALDNS; do
	echo "Search for $SPDN"
	nentries=`$ldbsearch $options $CONFIGURATION -H $p://$SERVER -s base -b "$SPDN" '(objectClass=*)' | grep "dn: $BASEDN"  | wc -l`
	if [ $nentries -lt 1 ]; then
		echo "Special search returned 0 items"
		failed=`expr $failed + 1`
	fi
done

echo "Search using OIDs instead of names"
nentries1=`$ldbsearch $options $CONFIGURATION -H $p://$SERVER '(objectClass=user)' name | grep "^name: "  | wc -l`
nentries2=`$ldbsearch $options $CONFIGURATION -H $p://$SERVER '(2.5.4.0=1.2.840.113556.1.5.9)' name | grep "^name: "  | wc -l`
if [ $nentries1 -lt 1 ]; then
	echo "Error: Searching user via (objectClass=user): '$nentries1' < 1"
	failed=`expr $failed + 1`
fi
if [ $nentries2 -lt 1 ]; then
	echo "Error: Searching user via (2.5.4.0=1.2.840.113556.1.5.9) '$nentries2' < 1"
	failed=`expr $failed + 1`
fi
if [ x"$nentries1" != x"$nentries2" ]; then
	echo "Error: Searching user with OIDS[$nentries1] doesn't return the same as STRINGS[$nentries2]"
	failed=`expr $failed + 1`
fi

exit $failed
