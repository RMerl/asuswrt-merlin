# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl 1.t'

#########################

# change 'tests => 2' to 'tests => last_test_to_print';

use Test;
BEGIN { plan tests => 2 };
# use NetSNMP::TrapReceiver;  # we can't include this directly in a module.
ok(1); # If we made it this far, we're ok.  Bogus test!


my $fail;
foreach my $constname (qw(
	NETSNMPTRAPD_AUTH_HANDLER NETSNMPTRAPD_HANDLER_BREAK
	NETSNMPTRAPD_HANDLER_FAIL NETSNMPTRAPD_HANDLER_FINISH
	NETSNMPTRAPD_HANDLER_OK NETSNMPTRAPD_POST_HANDLER
	NETSNMPTRAPD_PRE_HANDLER)) {
  next if (eval "my \$a = $constname; 1");
  if ($@ =~ /^Your vendor has not defined NetSNMP::TrapReceiver macro $constname/) {
    print "# pass: $@";
  } else {
    print "# fail: $@";
    $fail = 1;    
  }
}
if ($fail) {
  print "not ok 2\n";
} else {
  print "ok 2\n";
}

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

