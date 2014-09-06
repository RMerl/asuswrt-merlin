
# functions used by RFC-1213 MIB test modules

myport=$SNMP_TRANSPORT_SPEC:$SNMP_TEST_DEST$SNMP_SNMPD_PORT

noauth=""  # no - use Auth+Priv . yes - no auth, no priv

if [ "x$noauth" = xyes ] ; then
   TEST_AUTHPRIV_PARMS="-l noAuthnoPriv"
else
   TEST_AUTHPRIV_PARMS="-l authNoPriv -a MD5 -A testpass"
fi

config()
{
	rm -f $SNMP_CONFIG_FILE
	CONFIGAGENT rwcommunity test
	STARTAGENT
}

configv3()
{
	rm -f $SNMP_CONFIG_FILE
	CONFIGAGENT rwcommunity test
	CONFIGAGENT rwuser testrwuser noauth
	CONFIGAGENT createUser testrwuser MD5 testpass
	STARTAGENT
}

get_snmp_variable()
{
	test_start "Access $2.0 by SNMPv$1..."
	CAPTURE "snmpget -v $1 -c test $myport $2.0"
	if [ $? != 0 ];then
		test_finish FAIL
	else
		test_finish PASS
	fi

}

get_snmpv3_variable()
{
	test_start "Access $2.0 by SNMPv3..."
	CAPTURE "snmpget -v 3 -u testrwuser $TEST_AUTHPRIV_PARMS $myport $2.0"
	if [ $? != 0 ];then
		test_finish FAIL
	else
		test_finish PASS
	fi

}

get_snmp_table()
{
	test_start "Access table $2 by SNMPv$1..."
	CAPTURE "snmpgetnext -Of -v $1 -c test $myport $2"
        CHECKFILE '' "\.$2\."
        if [ "$snmp_last_test_result" = 0 ] ; then
		test_finish FAIL
	else
		test_finish PASS
	fi

}

get_snmpv3_table()
{
  	test_start "Access table $2 by SNMPv3..."
	CAPTURE "snmpgetnext -Of -v 3 -u testrwuser $TEST_AUTHPRIV_PARMS $myport $2"
        CHECKFILE '' "\.$2\."
        if [ "$snmp_last_test_result" = 0 ] ; then
		test_finish FAIL
	else
		test_finish PASS
	fi

}
