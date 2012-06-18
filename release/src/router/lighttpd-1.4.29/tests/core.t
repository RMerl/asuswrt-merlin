#!/usr/bin/env perl
BEGIN {
	# add current source dir to the include-path
	# we need this for make distcheck
	(my $srcdir = $0) =~ s,/[^/]+$,/,;
	unshift @INC, $srcdir;
}

use strict;
use IO::Socket;
use Test::More tests => 21;
use LightyTest;

my $tf = LightyTest->new();
my $t;

ok($tf->start_proc == 0, "Starting lighttpd") or die();

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'Valid HTTP/1.0 Request') or die();

$t->{REQUEST}  = ( <<EOF
GET /
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'missing Protocol');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/01.01
Host: foo
Connection: close
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.1', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'zeros in protocol version');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/.01
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'missing major version');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/01.
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'missing minor version');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/a.b
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'strings as version');

$t->{REQUEST}  = ( <<EOF
BC /
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'missing protocol + unknown method');

$t->{REQUEST}  = ( <<EOF
ABC
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'missing protocol + unknown method + missing URI');

$t->{REQUEST}  = ( <<EOF
ABC / HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 501 } ];
ok($tf->handle_http($t) == 0, 'unknown method');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.3
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 505 } ];
ok($tf->handle_http($t) == 0, 'unknown protocol');

$t->{REQUEST}  = ( <<EOF
GET http://www.example.org/ HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'absolute URI');

print "\nLow-Level Request-Header Parsing\n";
$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
ABC : foo
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'whitespace after key');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
ABC a: foo
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'whitespace with-in key');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
ABC:foo
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'no whitespace');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
ABC:foo
  bc
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'line-folding');

print "\nLow-Level Request-Header Parsing - URI\n";
$t->{REQUEST}  = ( <<EOF
GET /index%2ehtml HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'URL-encoding');

$t->{REQUEST}  = ( <<EOF
GET /index.html%00 HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 404 } ];
ok($tf->handle_http($t) == 0, 'URL-encoding, %00');

$t->{REQUEST}  = ( <<EOF
OPTIONS * HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'OPTIONS');

$t->{REQUEST}  = ( <<EOF
OPTIONS / HTTP/1.1
Host: www.example.org
Connection: close
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.1', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'OPTIONS');



ok($tf->stop_proc == 0, "Stopping lighttpd");

