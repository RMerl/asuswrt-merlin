#!/usr/bin/env perl
BEGIN {
	# add current source dir to the include-path
	# we need this for make distcheck
	(my $srcdir = $0) =~ s,/[^/]+$,/,;
	unshift @INC, $srcdir;
}

use strict;
use IO::Socket;
use Test::More tests => 5;
use LightyTest;

my $tf = LightyTest->new();
my $t;

ok($tf->start_proc == 0, "Starting lighttpd") or die();

# get current user

$t->{REQUEST}  = ( <<EOF
GET /~foobar/ HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 404 } ];
ok($tf->handle_http($t) == 0, 'valid user');

$t->{REQUEST}  = ( <<EOF
GET /~jan HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 301, 'Location' => 'http://'.$tf->{HOSTNAME}.':'.$tf->{PORT}.'/~jan/' } ];
ok($tf->handle_http($t) == 0, 'valid user + redirect');

$t->{REQUEST}  = ( <<EOF
GET /~jan HTTP/1.0
Host: www.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 301, 'Location' => 'http://www.example.org/~jan/' } ];
ok($tf->handle_http($t) == 0, 'valid user + redirect');

ok($tf->stop_proc == 0, "Stopping lighttpd");

