#!/usr/bin/env perl
BEGIN {
	# add current source dir to the include-path
	# we need this for make distcheck
	(my $srcdir = $0) =~ s,/[^/]+$,/,;
	unshift @INC, $srcdir;
}

use strict;
use IO::Socket;
use Test::More tests => 12;
use LightyTest;

my $tf = LightyTest->new();
my $t;

ok($tf->start_proc == 0, "Starting lighttpd") or die();

## Low-Level Response-Header Parsing - HTTP/1.1

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.1
Host: www.example.org
Connection: close
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.1', 'HTTP-Status' => 200, '+Date' => '' } ];
ok($tf->handle_http($t) == 0, 'Date header');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.1
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.1', 'HTTP-Status' => 400, 'Connection' => 'close' } ];
ok($tf->handle_http($t) == 0, 'Host missing');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, '+ETag' => '' } ];
ok($tf->handle_http($t) == 0, 'ETag is set');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'ETag' => '/^".+"$/' } ];
ok($tf->handle_http($t) == 0, 'ETag has quotes');



## Low-Level Response-Header Parsing - Content-Length


$t->{REQUEST}  = ( <<EOF
GET /12345.html HTTP/1.0
Host: 123.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'Content-Length' => '6' } ];
ok($tf->handle_http($t) == 0, 'Content-Length for text/html');

$t->{REQUEST}  = ( <<EOF
GET /12345.txt HTTP/1.0
Host: 123.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'Content-Length' => '6' } ];
ok($tf->handle_http($t) == 0, 'Content-Length for text/plain');


## Low-Level Response-Header Parsing - Location

$t->{REQUEST}  = ( <<EOF
GET /dummydir HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 301, 'Location' => 'http://'.$tf->{HOSTNAME}.':'.$tf->{PORT}.'/dummydir/' } ];
ok($tf->handle_http($t) == 0, 'internal redirect in directory');

$t->{REQUEST}  = ( <<EOF
GET /dummydir?foo HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 301, 'Location' => 'http://'.$tf->{HOSTNAME}.':'.$tf->{PORT}.'/dummydir/?foo' } ];
ok($tf->handle_http($t) == 0, 'internal redirect in directory + querystring');

## simple-vhost

$t->{REQUEST}  = ( <<EOF
GET /12345.txt HTTP/1.0
Host: no-simple.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'Content-Length' => '6' } ];
ok($tf->handle_http($t) == 0, 'disabling simple-vhost via conditionals');

$t->{REQUEST}  = ( <<EOF
GET /12345.txt HTTP/1.0
Host: simple.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 404 } ];
ok($tf->handle_http($t) == 0, 'simple-vhost via conditionals');

ok($tf->stop_proc == 0, "Stopping lighttpd");

