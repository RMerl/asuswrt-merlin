#!/usr/bin/env perl
BEGIN {
	# add current source dir to the include-path
	# we need this for make distcheck
	(my $srcdir = $0) =~ s,/[^/]+$,/,;
	unshift @INC, $srcdir;
}

use strict;
use IO::Socket;
use Test::More tests => 6;
use LightyTest;

my $tf = LightyTest->new();
my $t;

ok($tf->start_proc == 0, "Starting lighttpd") or die();

$t->{REQUEST} = ( <<EOF
GET /get-header.pl?TRAC_ENV HTTP/1.0
Host: www.example.org
EOF
 );
$t->{RESPONSE}  = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'tracenv' } ];
ok($tf->handle_http($t) == 0, 'query first setenv');

$t->{REQUEST}  = ( <<EOF
GET /get-header.pl?SETENV HTTP/1.0
Host: www.example.org
EOF
 );
$t->{RESPONSE}  = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'setenv' } ];
ok($tf->handle_http($t) == 0, 'query second setenv');

$t->{REQUEST}  = ( <<EOF
GET /get-header.pl?HTTP_FOO HTTP/1.0
Host: www.example.org
EOF
 );
$t->{RESPONSE}  = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'foo' } ];
ok($tf->handle_http($t) == 0, 'query add-request-header');

$t->{REQUEST} = ( <<EOF
GET /index.html HTTP/1.0
Host: www.example.org
EOF
 );
$t->{RESPONSE}  = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'BAR' => 'foo' } ];
ok($tf->handle_http($t) == 0, 'query add-response-header');

ok($tf->stop_proc == 0, "Stopping lighttpd");

