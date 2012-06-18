#!/usr/bin/env perl
BEGIN {
	# add current source dir to the include-path
	# we need this for make distcheck
	(my $srcdir = $0) =~ s,/[^/]+$,/,;
	unshift @INC, $srcdir;
}

use strict;
use IO::Socket;
use Test::More tests => 11;
use LightyTest;

my $tf = LightyTest->new();
my $t;

$tf->{CONFIGFILE} = 'mod-compress.conf';

ok($tf->start_proc == 0, "Starting lighttpd") or die();

$t->{REQUEST}  = ( <<EOF
GET /index.html HTTP/1.0
Accept-Encoding: deflate
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, '+Vary' => '' } ];
ok($tf->handle_http($t) == 0, 'Vary is set');

$t->{REQUEST}  = ( <<EOF
GET /index.html HTTP/1.0
Accept-Encoding: deflate
Host: no-cache.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, '+Vary' => '', 'Content-Length' => '1288', '+Content-Encoding' => '' } ];
ok($tf->handle_http($t) == 0, 'deflate - Content-Length and Content-Encoding is set');

$t->{REQUEST}  = ( <<EOF
GET /index.html HTTP/1.0
Accept-Encoding: deflate
Host: cache.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, '+Vary' => '', 'Content-Length' => '1288', '+Content-Encoding' => '' } ];
ok($tf->handle_http($t) == 0, 'deflate - Content-Length and Content-Encoding is set');

$t->{REQUEST}  = ( <<EOF
GET /index.html HTTP/1.0
Accept-Encoding: gzip
Host: no-cache.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, '+Vary' => '', 'Content-Length' => '1306', '+Content-Encoding' => '' } ];
ok($tf->handle_http($t) == 0, 'gzip - Content-Length and Content-Encoding is set');

$t->{REQUEST}  = ( <<EOF
GET /index.html HTTP/1.0
Accept-Encoding: gzip
Host: cache.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, '+Vary' => '', 'Content-Length' => '1306', '+Content-Encoding' => '' } ];
ok($tf->handle_http($t) == 0, 'gzip - Content-Length and Content-Encoding is set');


$t->{REQUEST}  = ( <<EOF
GET /index.txt HTTP/1.0
Accept-Encoding: gzip, deflate
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, '+Vary' => '', '+Content-Encoding' => '' } ];
ok($tf->handle_http($t) == 0, 'gzip, deflate - Content-Length and Content-Encoding is set');

$t->{REQUEST}  = ( <<EOF
GET /index.txt HTTP/1.0
Accept-Encoding: gzip, deflate
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, '+Vary' => '', '+Content-Encoding' => '', 'Content-Type' => "text/plain; charset=utf-8" } ];
ok($tf->handle_http($t) == 0, 'Content-Type is from the original file');

$t->{REQUEST}  = ( <<EOF
GET /index.txt HTTP/1.0
Accept-encoding:
X-Accept-encoding: x-i2p-gzip;q=1.0, identity;q=0.5, deflate;q=0, gzip;q=0, *;q=0
User-Agent: MYOB/6.66 (AN/ON)
Connection: close
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, '+Vary' => '', 'Content-Type' => "text/plain; charset=utf-8" } ];
ok($tf->handle_http($t) == 0, 'Empty Accept-Encoding');

$t->{REQUEST}  = ( <<EOF
GET /index.txt HTTP/1.0
Accept-Encoding: bzip2, gzip, deflate
Host: cache.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, '+Vary' => '', 'Content-Encoding' => 'gzip', 'Content-Type' => "text/plain; charset=utf-8" } ];
ok($tf->handle_http($t) == 0, 'bzip2 requested but disabled');


ok($tf->stop_proc == 0, "Stopping lighttpd");
