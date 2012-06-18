#!/usr/bin/env perl
BEGIN {
	# add current source dir to the include-path
	# we need this for make distcheck
	(my $srcdir = $0) =~ s,/[^/]+$,/,;
	unshift @INC, $srcdir;
}

use strict;
use IO::Socket;
use Test::More tests => 17;
use LightyTest;

my $tf = LightyTest->new();
my $t;

$ENV{"env_test"} = "good_env";

$tf->{CONFIGFILE} = 'var-include.conf';

ok($tf->start_proc == 0, "Starting lighttpd") or die();

$t->{REQUEST}  = ( "GET /index.html HTTP/1.0\r\nHost: www.example.org\r\n" );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 301, 'Location' => "/redirect" } ];
ok($tf->handle_http($t) == 0, 'basic test');

my $myvar = "good";
my $server_name = "test.example.org";
my $mystr = "string";
$mystr .= "_append";
my $tests = {
    "include"        => "/good_include",
      "concat"         => "/good_" . "concat",
      "servername1"    => "/good_" . $server_name,
      "servername2"    => $server_name . "/good_",
      "servername3"    => "/good_" . $server_name . "/",
      "var.myvar"      => "/good_var_myvar" . $myvar,
      "myvar"          => "/good_myvar" . $myvar,
      "env"            => "/" . $ENV{"env_test"},

    "number1"        => "/good_number" . "1",
      "number2"        => "1" . "/good_number",
      "array_append"   => "/good_array_append",
      "string_append"  => "/good_" . $mystr,
      "number_append"  => "/good_" . "2",

    "include_shell"  => "/good_include_shell_" . "456"
};

foreach my $test (keys %{ $tests }) {
	my $expect = $tests->{$test};
	$t->{REQUEST}  = ( <<EOF
GET /$test HTTP/1.0
Host: $server_name
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 301, 'Location' => $expect } ];
	ok($tf->handle_http($t) == 0, $test);
}

ok($tf->stop_proc == 0, "Stopping lighttpd");
