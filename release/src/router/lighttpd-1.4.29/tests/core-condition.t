#!/usr/bin/env perl
BEGIN {
	# add current source dir to the include-path
	# we need this for make distcheck
	(my $srcdir = $0) =~ s,/[^/]+$,/,;
	unshift @INC, $srcdir;
}

use strict;
use IO::Socket;
use Test::More tests => 19;
use LightyTest;

my $tf = LightyTest->new();
my $t;

$tf->{CONFIGFILE} = 'condition.conf';
ok($tf->start_proc == 0, "Starting lighttpd") or die();

$t->{REQUEST}  = ( <<EOF
GET /index.html HTTP/1.0
Host: www.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 301, 'Location' => "/match_1" } ];
ok($tf->handle_http($t) == 0, 'config deny');

$t->{REQUEST}  = ( <<EOF
GET /index.html HTTP/1.0
Host: test1.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 301, 'Location' => "/match_2" } ];
ok($tf->handle_http($t) == 0, '2nd child of chaining');

$t->{REQUEST}  = ( <<EOF
GET /index.html HTTP/1.0
Host: test2.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 301, 'Location' => "/match_3" } ];
ok($tf->handle_http($t) == 0, '3rd child of chaining');

$t->{REQUEST}  = ( <<EOF
GET /index.html HTTP/1.0
Host: test3.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 301, 'Location' => "/match_5" } ];
ok($tf->handle_http($t) == 0, 'nesting');

$t->{REQUEST}  = ( <<EOF
GET /subdir/index.html HTTP/1.0
Host: test4.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 301, 'Location' => "/match_7" } ];
ok($tf->handle_http($t) == 0, 'url subdir');

$t->{REQUEST}  = ( <<EOF
GET /subdir/../css/index.html HTTP/1.0
Host: test4.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 301, 'Location' => "/match_6" } ];
ok($tf->handle_http($t) == 0, 'url subdir with path traversal');

ok($tf->stop_proc == 0, "Stopping lighttpd");

$tf->{CONFIGFILE} = 'lighttpd.conf';
ok($tf->start_proc == 0, "Starting lighttpd") or die();

$t->{REQUEST}  = ( <<EOF
GET /nofile.png HTTP/1.0
Host: referer.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 404 } ];
ok($tf->handle_http($t) == 0, 'condition: Referer - no referer');

$t->{REQUEST}  = ( <<EOF
GET /nofile.png HTTP/1.0
Host: referer.example.org
Referer: http://referer.example.org/
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 404 } ];
ok($tf->handle_http($t) == 0, 'condition: Referer - referer matches regex');

$t->{REQUEST}  = ( <<EOF
GET /image.jpg HTTP/1.0
Host: www.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'condition: Referer - no referer');

$t->{REQUEST}  = ( <<EOF
GET /image.jpg HTTP/1.0
Host: www.example.org
Referer: http://referer.example.org/
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'condition: Referer - referer matches regex');

$t->{REQUEST}  = ( <<EOF
GET /image.jpg HTTP/1.0
Host: www.example.org
Referer: http://evil-referer.example.org/
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 403 } ];
ok($tf->handle_http($t) == 0, 'condition: Referer - referer doesn\'t match');

$t->{REQUEST} = ( <<EOF
GET /nofile HTTP/1.1
Host: bug255.example.org

GET /nofile HTTP/1.1
Host: bug255.example.org
Connection: close
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.1', 'HTTP-Status' => 403 },  { 'HTTP-Protocol' => 'HTTP/1.1', 'HTTP-Status' => 403 } ];
ok($tf->handle_http($t) == 0, 'remote ip cache (#255)');

$t->{REQUEST}  = ( <<EOF
GET /empty-ref.noref HTTP/1.0
Cookie: empty-ref
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 403 } ];
ok($tf->handle_http($t) == 0, 'condition: $HTTP["referer"] == "" and Referer is no set');

$t->{REQUEST}  = ( <<EOF
GET /empty-ref.noref HTTP/1.0
Cookie: empty-ref
Referer:
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 403 } ];
ok($tf->handle_http($t) == 0, 'condition: $HTTP["referer"] == "" and Referer is empty');

$t->{REQUEST}  = ( <<EOF
GET /empty-ref.noref HTTP/1.0
Cookie: empty-ref
Referer: foobar
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 404 } ];
ok($tf->handle_http($t) == 0, 'condition: $HTTP["referer"] == "" and Referer: foobar');

ok($tf->stop_proc == 0, "Stopping lighttpd");

