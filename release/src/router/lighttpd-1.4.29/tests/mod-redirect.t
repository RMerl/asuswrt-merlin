#!/usr/bin/env perl
BEGIN {
	# add current source dir to the include-path
	# we need this for make distcheck
	(my $srcdir = $0) =~ s,/[^/]+$,/,;
	unshift @INC, $srcdir;
}

use strict;
use IO::Socket;
use Test::More tests => 7;
use LightyTest;

my $tf = LightyTest->new();
my $t;

ok($tf->start_proc == 0, "Starting lighttpd") or die();

$t->{REQUEST}  = ( <<EOF
GET /redirect/ HTTP/1.0
Host: vvv.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 301, 'Location' => 'http://localhost:'.$tf->{PORT}.'/' } ];
ok($tf->handle_http($t) == 0, 'external redirect');

$t->{REQUEST}  = ( <<EOF
GET /redirect/ HTTP/1.0
Host: vvv.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 301, 'Location' => 'http://localhost:'.$tf->{PORT}.'/', 'Content-Length' => '0' } ];
ok($tf->handle_http($t) == 0, 'external redirect should have a Content-Length: 0');

$t->{REQUEST} = ( <<EOF
GET /redirect/ HTTP/1.0
Host: zzz.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 301, 'Location' => 'http://localhost:'.$tf->{PORT}.'/zzz' } ];
ok($tf->handle_http($t) == 0, 'external redirect with cond regsub');

$t->{REQUEST} = ( <<EOF
GET /redirect/ HTTP/1.0
Host: remoteip.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 301, 'Location' => 'http://localhost:'.$tf->{PORT}.'/127.0.0.1' } ];
ok($tf->handle_http($t) == 0, 'external redirect with cond regsub on remoteip');

$t->{REQUEST} = ( <<EOF
GET /redirect/ HTTP/1.0
Host: remoteip2.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 301, 'Location' => 'http://localhost:'.$tf->{PORT}.'/remoteip2' } ];
ok($tf->handle_http($t) == 0, 'external redirect with cond regsub on remoteip2');

ok($tf->stop_proc == 0, "Stopping lighttpd");
