# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; 

	%tests = ( 
                  "NETSNMP_DS_MAX_IDS"                     => 3,
                  "NETSNMP_DS_MAX_SUBIDS"                  => 48,
                  "NETSNMP_DS_LIBRARY_ID"                  => 0,
                  "NETSNMP_DS_APPLICATION_ID"              => 1,
                  "NETSNMP_DS_TOKEN_ID"                    => 2,
                  "NETSNMP_DS_LIB_MIB_ERRORS"              => 0,
                  "NETSNMP_DS_LIB_SAVE_MIB_DESCRS"         => 1,
                  "NETSNMP_DS_LIB_MIB_COMMENT_TERM"        => 2,
                  "NETSNMP_DS_LIB_MIB_PARSE_LABEL"         => 3,
                  "NETSNMP_DS_LIB_DUMP_PACKET"             => 4,
                  "NETSNMP_DS_LIB_LOG_TIMESTAMP"           => 5,
                  "NETSNMP_DS_LIB_DONT_READ_CONFIGS"       => 6,
                  "NETSNMP_DS_LIB_MIB_REPLACE"             => 7,
                  "NETSNMP_DS_LIB_PRINT_NUMERIC_ENUM"      => 8,
                  "NETSNMP_DS_LIB_PRINT_NUMERIC_OIDS"      => 9,
                  "NETSNMP_DS_LIB_DONT_BREAKDOWN_OIDS"     => 10,
                  "NETSNMP_DS_LIB_ALARM_DONT_USE_SIG"      => 11,
                  "NETSNMP_DS_LIB_PRINT_FULL_OID"          => 12,
                  "NETSNMP_DS_LIB_QUICK_PRINT"             => 13,
                  "NETSNMP_DS_LIB_RANDOM_ACCESS"           => 14,
                  "NETSNMP_DS_LIB_REGEX_ACCESS"            => 15,
                  "NETSNMP_DS_LIB_DONT_CHECK_RANGE"        => 16,
                  "NETSNMP_DS_LIB_NO_TOKEN_WARNINGS"       => 17,
                  "NETSNMP_DS_LIB_NUMERIC_TIMETICKS"       => 18,
                  "NETSNMP_DS_LIB_ESCAPE_QUOTES"           => 19,
                  "NETSNMP_DS_LIB_REVERSE_ENCODE"          => 20,
                  "NETSNMP_DS_LIB_PRINT_BARE_VALUE"        => 21,
                  "NETSNMP_DS_LIB_EXTENDED_INDEX"          => 22,
                  "NETSNMP_DS_LIB_PRINT_HEX_TEXT"          => 23,
                  "NETSNMP_DS_LIB_PRINT_UCD_STYLE_OID"     => 24,
                  "NETSNMP_DS_LIB_READ_UCD_STYLE_OID"      => 25,
                  "NETSNMP_DS_LIB_HAVE_READ_PREMIB_CONFIG" => 26,
                  "NETSNMP_DS_LIB_HAVE_READ_CONFIG"        => 27,
                  "NETSNMP_DS_LIB_QUICKE_PRINT"            => 28,
                  "NETSNMP_DS_LIB_DONT_PRINT_UNITS"        => 29,
                  "NETSNMP_DS_LIB_NO_DISPLAY_HINT"         => 30,
                  "NETSNMP_DS_LIB_16BIT_IDS"               => 31,
                  "NETSNMP_DS_LIB_DONT_PERSIST_STATE"      => 32,
                  "NETSNMP_DS_LIB_2DIGIT_HEX_OUTPUT"       => 33,
                  "NETSNMP_DS_LIB_IGNORE_NO_COMMUNITY"     => 34,
                  "NETSNMP_DS_LIB_DISABLE_PERSISTENT_LOAD" => 35,
                  "NETSNMP_DS_LIB_DISABLE_PERSISTENT_SAVE" => 36,
                  "NETSNMP_DS_LIB_APPEND_LOGFILES"         => 37,
                  "NETSNMP_DS_LIB_MIB_WARNINGS"            => 0,
                  "NETSNMP_DS_LIB_SECLEVEL"                => 1,
                  "NETSNMP_DS_LIB_SNMPVERSION"             => 2,
                  "NETSNMP_DS_LIB_DEFAULT_PORT"            => 3,
                  "NETSNMP_DS_LIB_OID_OUTPUT_FORMAT"       => 4,
                  "NETSNMP_DS_LIB_STRING_OUTPUT_FORMAT"    => 5,
                  "NETSNMP_DS_LIB_HEX_OUTPUT_LENGTH"       => 6,
                  "NETSNMP_DS_LIB_SERVERSENDBUF"           => 7,
                  "NETSNMP_DS_LIB_SERVERRECVBUF"           => 8,
                  "NETSNMP_DS_LIB_CLIENTSENDBUF"           => 9,
                  "NETSNMP_DS_LIB_CLIENTRECVBUF"           => 10,
                  "NETSNMP_DS_SNMP_VERSION_1"              => 128,
                  "NETSNMP_DS_SNMP_VERSION_2c"             => 1,
                  "NETSNMP_DS_SNMP_VERSION_3"              => 3,
                  "NETSNMP_DS_LIB_SECNAME"                 => 0,
                  "NETSNMP_DS_LIB_CONTEXT"                 => 1,
                  "NETSNMP_DS_LIB_PASSPHRASE"              => 2,
                  "NETSNMP_DS_LIB_AUTHPASSPHRASE"          => 3,
                  "NETSNMP_DS_LIB_PRIVPASSPHRASE"          => 4,
                  "NETSNMP_DS_LIB_OPTIONALCONFIG"          => 5,
                  "NETSNMP_DS_LIB_APPTYPE"                 => 6,
                  "NETSNMP_DS_LIB_COMMUNITY"               => 7,
                  "NETSNMP_DS_LIB_PERSISTENT_DIR"          => 8,
                  "NETSNMP_DS_LIB_CONFIGURATION_DIR"       => 9,
                  "NETSNMP_DS_LIB_SECMODEL"                => 10,
                  "NETSNMP_DS_LIB_MIBDIRS"                 => 11,
                  "NETSNMP_DS_LIB_OIDSUFFIX"               => 12,
                  "NETSNMP_DS_LIB_OIDPREFIX"               => 13,
                  "NETSNMP_DS_LIB_CLIENT_ADDR"             => 14,
                  "NETSNMP_DS_LIB_TEMP_FILE_PATTERN"       => 15,
                  "NETSNMP_DS_LIB_AUTHMASTERKEY"           => 16,
                  "NETSNMP_DS_LIB_PRIVMASTERKEY"           => 17,
                  "NETSNMP_DS_LIB_AUTHLOCALIZEDKEY"        => 18,
                  "NETSNMP_DS_LIB_PRIVLOCALIZEDKEY"        => 19,
                  "NETSNMP_DS_LIB_APPTYPES"                => 20,
                  "NETSNMP_DS_LIB_KSM_KEYTAB"              => 21,
                  "NETSNMP_DS_LIB_KSM_SERVICE_NAME"        => 22,
		  );

	print "1.." . (scalar(keys(%tests)) + 10) . "\n"; 
    }
END {print "not ok 1\n" unless $loaded;}
use NetSNMP::default_store (':all');
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):

print ((netsnmp_ds_set_string(1, 1, "hi there") == 0) ? "ok 2\n" : "not ok 2\n"); 
print ((netsnmp_ds_get_string(1, 1) eq "hi there") ? "ok 3\n" : "not ok 3\n"); 
print ((netsnmp_ds_set_int(1, 1, 42) == 0) ? "ok 4\n" : "not ok 4\n"); 
print ((netsnmp_ds_get_int(1, 1) == 42) ? "ok 5\n" : "not ok 5\n"); 
print ((netsnmp_ds_get_int(1, 2) == 0) ? "ok 6\n" : "not ok 6\n"); 
print ((NETSNMP_DS_LIB_REGEX_ACCESS == 15) ? "ok 7\n" : "not ok 7\n"); 
print ((netsnmp_ds_get_int(NETSNMP_DS_APPLICATION_ID, 1) == 42) ? "ok 8\n" : "not ok 8\n"); 
print ((netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, DS_LIB_DEFAULT_PORT, 9161) == 0) ? "ok 9\n" : "not ok 9\n"); 
print ((netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID, DS_LIB_DEFAULT_PORT) == 9161) ? "ok 10\n" : "not ok 10\n"); 

$c = 10;
foreach my $i (keys(%tests)) {
    my $str = "NetSNMP::default_store::$i";
    my $val = eval $str;
#    print "$i -> $val -> $tests{$i}\n";
    $c++;
    print (($val eq $tests{$i})?"ok $c\n" : "not ok $c\n#  error:  name=$i value_expected=$tests{$i}  value_got=$val \n");
}
