#!/bin/sh
#
# Blackbox tests for the "net registry" and "net rpc registry" commands.
#
# Copyright (C) 2010-2011 Michael Adam <obnox@samba.org>
# Copyright (C) 2010 Gregor Beck <gbeck@sernet.de>
#
# rpc tests are chose by specifying "rpc" as commandline parameter.

if [ $# -lt 3 ]; then
cat <<EOF
Usage: test_net_registry.sh SCRIPTDIR SERVERCONFFILE CONFIGURATION RPC
EOF
exit 1;
fi

SCRIPTDIR="$1"
SERVERCONFFILE="$2"
CONFIGURATION="$3"
RPC="$4"

NET="$VALGRIND ${NET:-$BINDIR/net} $CONFIGURATION"

if test "x${RPC}" = "xrpc" ; then
	NETREG="${NET} -U${USERNAME}%${PASSWORD} -I ${SERVER_IP} rpc registry"
else
	NETREG="${NET} registry"
fi

test x"$TEST_FUNCTIONS_SH" != x"INCLUDED" && {
incdir=`dirname $0`/../../../testprogs/blackbox
. $incdir/subunit.sh
}

failed=0

test_enumerate()
{
	KEY="$1"

	${NETREG} enumerate ${KEY}
}

test_getsd()
{
	KEY="$1"

	${NETREG} getsd ${KEY}
}

test_enumerate_nonexisting()
{
	KEY="$1"
	${NETREG} enumerate ${KEY}

	if test "x$?" = "x0" ; then
		echo "ERROR: enumerate succeeded with key '${KEY}'"
		false
	else
		true
	fi
}

test_enumerate_no_key()
{
	${NETREG} enumerate
	if test "x$?" = "x0" ; then
		echo "ERROR: enumerate succeeded without any key spcified"
		false
	else
		true
	fi
}

test_create_existing()
{
	KEY="HKLM"
	EXPECTED="createkey opened existing ${KEY}"

	OUTPUT=`${NETREG} createkey ${KEY}`
	if test "x$?" = "x0" ; then
		if test "$OUTPUT" = "$EXPECTED" ; then
			true
		else
			echo "got '$OUTPUT', expected '$EXPECTED'"
			false
		fi
	else
		printf "%s\n" "$OUTPUT"
		false
	fi
}

test_createkey()
{
	KEY="$1"
	BASEKEY=`dirname $KEY`
	SUBKEY=`basename $KEY`

	OUTPUT=`${NETREG} createkey ${KEY}`
	if test "x$?" != "x0" ; then
		echo "ERROR: createkey ${KEY} failed"
		echo "output:"
		printf "%s\n" "$OUTPUT"
		false
		return
	fi

	# check enumerate of basekey lists new key:
	OUTPUT=`${NETREG} enumerate ${BASEKEY}`
	if test "x$?" != "x0" ; then
		echo "ERROR: failed to enumerate key '${BASEKEY}'"
		echo "output:"
		printf "%s\n" "$OUTPUT"
		false
		return
	fi

	EXPECTED="Keyname = ${SUBKEY}"
	printf "%s\n" "$OUTPUT" | grep '^Keyname' | grep ${SUBKEY}
	if test "x$?" != "x0" ; then
		echo "ERROR: did not find expexted '$EXPECTED' in output"
		echo "output:"
		printf "%s\n" "$OUTPUT"
		false
	fi

	# check enumerate of new key works:
	${NETREG} enumerate ${KEY}
}

test_deletekey()
{
	KEY="$1"
	BASEKEY=`dirname ${KEY}`
	SUBKEY=`basename ${KEY}`

	OUTPUT=`test_createkey "${KEY}"`
	if test "x$?" != "x0" ; then
		printf "%s\n" "${OUTPUT}"
		false
		return
	fi

	OUTPUT=`${NETREG} deletekey ${KEY}`
	if test "x$?" != "x0" ; then
		printf "%s\n" "${OUTPUT}"
		false
		return
	fi

	# check enumerate of basekey does not show key anymore:
	OUTPUT=`${NETREG} enumerate ${BASEKEY}`
	if test "x$?" != "x0" ; then
		printf "%s\n" "$OUTPUT"
		false
		return
	fi

	UNEXPECTED="Keyname = ${SUBKEY}"
	printf "%s\n" "$OUTPUT" | grep '^Keyname' | grep ${SUBKEY}
	if test "x$?" = "x0" ; then
		echo "ERROR: found '$UNEXPECTED' after delete in output"
		echo "output:"
		printf "%s\n" "$OUTPUT"
		false
	fi

	# check enumerate of key itself does not work anymore:
	${NETREG} enumerate ${KEY}
	if test "x$?" = "x0" ; then
		echo "ERROR: 'enumerate ${KEY}' works after 'deletekey ${KEY}'"
		false
	else
		true
	fi
}

test_deletekey_nonexisting()
{
	KEY="$1"

	OUTPUT=`test_deletekey "${KEY}"`
	if test "x$?" != "x0" ; then
		printf "%s\n" "${OUTPUT}"
		false
		return
	fi

	${NETREG} deletekey "${KEY}"
	if test "x$?" = "x0" ; then
		echo "ERROR: delete after delete succeeded for key '${KEY}'"
		false
	fi
}

test_createkey_with_subkey()
{
	KEY="$1"
	KEY2=`dirname ${KEY}`
	SUBKEYNAME2=`basename ${KEY}`
	BASENAME=`dirname ${KEY2}`
	SUBKEYNAME1=`basename ${KEY2}`

	OUTPUT=`${NETREG} createkey ${KEY}`
	if test "x$?" != "x0" ; then
		echo "ERROR: createkey ${KEY} failed"
		printf "%s\n" "${OUTPUT}"
		false
		return
	fi

	# check we can enumerate to level key
	OUTPUT=`${NETREG} enumerate ${KEY}`
	if test "x$?" != "x0" ; then
		echo "ERROR: failed to enumerate '${KEY}' after creation"
		printf "%s\n" "${OUTPUT}"
		false
		return
	fi

	# clear:
	${NETREG} deletekey ${KEY} && ${NETREG} deletekey ${KEY2}
}

test_deletekey_with_subkey()
{
	KEY="$1"
	KEY2=`dirname ${KEY}`

	OUTPUT=`${NETREG} createkey ${KEY}`
	if test "x$?" != "x0" ; then
		printf "%s\n" "${OUTPUT}"
		false
		return
	fi

	OUTPUT=`${NETREG} deletekey ${KEY2}`

	if test "x$?" = "x0" ; then
		echo "ERROR: delete of key with subkey succeeded"
		echo "output:"
		printf "%s\n" "$OUTPUT"
		false
		return
	fi

	${NETREG} deletekey ${KEY} && ${NETREG} deletekey ${KEY2}
}

test_setvalue()
{
	KEY="$1"
	VALNAME="${2#_}"
	VALTYPE="$3"
	VALVALUE="$4"

	OUTPUT=`test_createkey ${KEY}`
	if test "x$?" != "x0" ; then
		printf "%s\n" "${OUTPUT}"
		false
		return
	fi

	OUTPUT=`${NETREG} setvalue ${KEY} "${VALNAME}" ${VALTYPE} ${VALVALUE}`
	if test "x$?" != "x0" ; then
		echo "ERROR: failed to set value testval in key ${KEY}"
		printf "%s\n" "${OUTPUT}"
		false
		return
	fi

	OUTPUT=`${NETREG} getvalueraw ${KEY} "${VALNAME}"`
	if test "x$?" != "x0" ; then
		echo "ERROR: failure calling getvalueraw for key ${KEY}"
		echo output:
		printf "%s\n" "${OUTPUT}"
		false
		return
	fi

	if test "x${OUTPUT}" != "x${VALVALUE}" ; then
		echo "ERROR: failure retrieving value ${VALNAME} for key ${KEY}"
		printf "expected: %s\ngot: %s\n" "${VALVALUE}" "${OUTPUT}"
		false
		return
	fi

}

test_deletevalue()
{
	KEY="$1"
	VALNAME="${2#_}"

	${NETREG} deletevalue ${KEY} "${VALNAME}"
}

test_deletevalue_nonexisting()
{
	KEY="$1"
	VALNAME="${2#_}"

	${NETREG} deletevalue ${KEY} "${VALNAME}"
	if test "x$?" = "x0" ; then
		echo "ERROR: succeeded deleting value ${VALNAME}"
		false
	else
		true
	fi
}

test_setvalue_twice()
{
	KEY="$1"
	VALNAME="${2#_}"
	VALTYPE1="$3"
	VALVALUE1="$4"
	VALTYPE2="$5"
	VALVALUE2="$6"

	OUTPUT=`test_setvalue ${KEY} _"${VALNAME}" ${VALTYPE1} ${VALVALUE1}`
	if test "x$?" != "x0" ; then
		echo "ERROR: first setvalue call failed"
		printf "%s\n" "$OUTPUT"
		false
		return
	fi

	${NETREG} setvalue ${KEY} "${VALNAME}" ${VALTYPE2} ${VALVALUE2}
}


testit "enumerate HKLM" \
	test_enumerate HKLM || \
	failed=`expr $failed + 1`

testit "enumerate nonexisting hive" \
	test_enumerate_nonexisting XYZ || \
	failed=`expr $failed + 1`

testit "enumerate without key" \
	test_enumerate_no_key || \
	failed=`expr $failed + 1`

# skip getsd test for registry currently: it fails
if test "x${RPC}" != "xrpc" ; then
testit "getsd HKLM" \
	test_getsd HKLM || \
	failed=`expr $failed + 1`
fi

testit "create existing HKLM" \
	test_create_existing || \
	failed=`expr $failed + 1`

testit "create key" \
	test_createkey HKLM/testkey || \
	failed=`expr $failed + 1`

testit "delete key" \
	test_deletekey HKLM/testkey || \
	failed=`expr $failed + 1`

testit "delete^2 key" \
	test_deletekey_nonexisting HKLM/testkey || \
	failed=`expr $failed + 1`

testit "enumerate nonexisting key" \
	test_enumerate_nonexisting HKLM/testkey || \
	failed=`expr $failed + 1`

testit "create key with subkey" \
	test_createkey_with_subkey HKLM/testkey/subkey || \
	failed=`expr $failed + 1`

testit "delete key with subkey" \
	test_deletekey_with_subkey HKLM/testkey/subkey || \
	failed=`expr $failed + 1`

testit "set value" \
	test_setvalue HKLM/testkey _testval sz moin || \
	failed=`expr $failed + 1`

testit "delete value" \
	test_deletevalue HKLM/testkey _testval || \
	failed=`expr $failed + 1`

testit "delete nonexisting value" \
	test_deletevalue_nonexisting HKLM/testkey _testval || \
	failed=`expr $failed + 1`

testit "set value to different type" \
	test_setvalue_twice HKLM/testkey testval sz moin dword 42 || \
	failed=`expr $failed + 1`

testit "set default value" \
	test_setvalue HKLM/testkey _"" sz 42 || \
	failed=`expr $failed + 1`

testit "delete default value" \
	test_deletevalue HKLM/testkey _"" || \
	failed=`expr $failed + 1`

testit "delete nonexisting default value" \
	test_deletevalue_nonexisting HKLM/testkey _"" || \
	failed=`expr $failed + 1`

testit "delete key with value" \
	test_deletekey HKLM/testkey || \
	failed=`expr $failed + 1`


testok $0 $failed

