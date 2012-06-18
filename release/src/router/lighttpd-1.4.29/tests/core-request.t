#!/usr/bin/env perl
BEGIN {
	# add current source dir to the include-path
	# we need this for make distcheck
	(my $srcdir = $0) =~ s,/[^/]+$,/,;
	unshift @INC, $srcdir;
}

use strict;
use IO::Socket;
use Test::More tests => 36;
use LightyTest;

my $tf = LightyTest->new();
my $t;

ok($tf->start_proc == 0, "Starting lighttpd") or die();

## Low-Level Request-Header Parsing - URI

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



## Low-Level Request-Header Parsing - Host

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Host: www.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'hostname');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Host: 127.0.0.1
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'IPv4 address');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Host: [::1]
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'IPv6 address');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Host: www.example.org:80
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'hostname + port');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Host: 127.0.0.1:80
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'IPv4 address + port');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Host: [::1]:80
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'IPv6 address + port');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Host: ../123.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'directory traversal');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Host: .jsdh.sfdg.sdfg.
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'leading and trailing dot');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Host: jsdh.sfdg.sdfg.
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'trailing dot is ok');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Host: .jsdh.sfdg.sdfg
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'leading dot');


$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Host: jsdh..sfdg.sdfg
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'two dots');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Host: jsdh.sfdg.sdfg:asd
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'broken port-number');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Host: jsdh.sfdg.sdfg:-1
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'negative port-number');


$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Host: :80
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'port given but host missing');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Host: .jsdh.sfdg.:sdfg.
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'port and host are broken');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Host: a.b-c.d123
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'allowed characters in host-name');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Host: -a.c
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'leading dash');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Host: .
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'dot only');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Host: a192.168.2.10:1234
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'broken IPv4 address - non-digit');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Host: 192.168.2:1234
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'broken IPv4 address - too short');



## Low-Level Request-Header Parsing - Content-Length


$t->{REQUEST}  = ( <<EOF
GET /index.html HTTP/1.0
Content-Length: -2
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'negative Content-Length');

$t->{REQUEST}  = ( <<EOF
POST /12345.txt HTTP/1.0
Host: 123.example.org
Content-Length: 2147483648
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 413 } ];
ok($tf->handle_http($t) == 0, 'Content-Length > max-request-size');

$t->{REQUEST}  = ( <<EOF
POST /12345.txt HTTP/1.0
Host: 123.example.org
Content-Length:
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 411 } ];
ok($tf->handle_http($t) == 0, 'Content-Length is empty');

print "\nLow-Level Request-Header Parsing - HTTP/1.1\n";
$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.1
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.1', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'Host missing');

print "\nContent-Type\n";
$t->{REQUEST}  = ( <<EOF
GET /image.jpg HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'Content-Type' => 'image/jpeg' } ];
ok($tf->handle_http($t) == 0, 'Content-Type - image/jpeg');

$t->{REQUEST}  = ( <<EOF
GET /image.JPG HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'Content-Type' => 'image/jpeg' } ];
ok($tf->handle_http($t) == 0, 'Content-Type - image/jpeg (upper case)');

$t->{REQUEST}  = ( <<EOF
GET /a HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'Content-Type' => 'application/octet-stream' } ];
ok($tf->handle_http($t) == 0, 'Content-Type - unknown');

$t->{REQUEST}  = ( <<EOF
GET  HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 400 } ];
ok($tf->handle_http($t) == 0, 'empty request-URI');

$t->{REQUEST}  = ( <<EOF
GET /Foo.txt HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'uppercase filenames');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Location: foo
Location: foobar
  baz
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, '#1232 - duplicate headers with line-wrapping');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
Location: 
Location: foobar
  baz
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, '#1232 - duplicate headers with line-wrapping - test 2');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
A: 
Location: foobar
  baz
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, '#1232 - duplicate headers with line-wrapping - test 3');




ok($tf->stop_proc == 0, "Stopping lighttpd");

