# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; 

	%tests = ( 
                  "NETSNMP_DS_AGENT_VERBOSE"               => 0,
                  "NETSNMP_DS_AGENT_ROLE"                  => 1,
                  "NETSNMP_DS_AGENT_NO_ROOT_ACCESS"        => 2,
                  "NETSNMP_DS_AGENT_AGENTX_MASTER"         => 3,
                  "NETSNMP_DS_AGENT_QUIT_IMMEDIATELY"      => 4,
                  "NETSNMP_DS_AGENT_DISABLE_PERL"          => 5,
                  "NETSNMP_DS_AGENT_NO_CONNECTION_WARNINGS" => 6,
                  "NETSNMP_DS_AGENT_LEAVE_PIDFILE"         => 7,
                  "NETSNMP_DS_AGENT_NO_CACHING"            => 8,
                  "NETSNMP_DS_AGENT_STRICT_DISMAN"         => 9,
                  "NETSNMP_DS_AGENT_DONT_RETAIN_NOTIFICATIONS" => 10,
                  "NETSNMP_DS_AGENT_DONT_LOG_TCPWRAPPERS_CONNECTS" => 12,
                  "NETSNMP_DS_AGENT_SKIPNFSINHOSTRESOURCES" => 13,
                  "NETSNMP_DS_AGENT_PROGNAME"              => 0,
                  "NETSNMP_DS_AGENT_X_SOCKET"              => 1,
                  "NETSNMP_DS_AGENT_PORTS"                 => 2,
                  "NETSNMP_DS_AGENT_INTERNAL_SECNAME"      => 3,
                  "NETSNMP_DS_AGENT_PERL_INIT_FILE"        => 4,
                  "NETSNMP_DS_SMUX_SOCKET"                 => 5,
                  "NETSNMP_DS_NOTIF_LOG_CTX"               => 6,
                  "NETSNMP_DS_AGENT_FLAGS"                 => 0,
                  "NETSNMP_DS_AGENT_USERID"                => 1,
                  "NETSNMP_DS_AGENT_GROUPID"               => 2,
                  "NETSNMP_DS_AGENT_AGENTX_PING_INTERVAL"  => 3,
                  "NETSNMP_DS_AGENT_AGENTX_TIMEOUT"        => 4,
                  "NETSNMP_DS_AGENT_AGENTX_RETRIES"        => 5,
                  "NETSNMP_DS_AGENT_X_SOCK_PERM"           => 6,
                  "NETSNMP_DS_AGENT_X_DIR_PERM"            => 7,
                  "NETSNMP_DS_AGENT_X_SOCK_USER"           => 8,
                  "NETSNMP_DS_AGENT_X_SOCK_GROUP"          => 9,
                  "NETSNMP_DS_AGENT_CACHE_TIMEOUT"         => 10,
                  "NETSNMP_DS_AGENT_INTERNAL_VERSION"      => 11,
                  "NETSNMP_DS_AGENT_INTERNAL_SECLEVEL"     => 12,
                  "NETSNMP_DS_AGENT_MAX_GETBULKREPEATS"    => 13,
                  "NETSNMP_DS_AGENT_MAX_GETBULKRESPONSES"  => 14,
		  );

	print "1.." . (scalar(keys(%tests)) + 2) . "\n"; 
    }
END {print "not ok 1\n" unless $loaded;}
use NetSNMP::agent::default_store (':all');
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):

$c = 2;
foreach my $i (keys(%tests)) {
    my $str = "NetSNMP::agent::default_store::$i";
    my $val = eval $str;
#    print "$i -> $val -> $tests{$i}\n";
    $c++;
    print (($val eq $tests{$i})?"ok $c\n" : "not ok $c\n#  error:  name=$i value_expected=$tests{$i}  value_got=$val \n");
}
