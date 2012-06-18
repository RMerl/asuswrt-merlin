#!/usr/bin/env perl
BEGIN {
	# add current source dir to the include-path
	# we need this for make distcheck
	(my $srcdir = $0) =~ s,/[^/]+$,/,;
	unshift @INC, $srcdir;
}

use strict;
use IO::Socket;
use Test::More tests => 13;
use LightyTest;

my $tf = LightyTest->new();
my $t;

$tf->{CONFIGFILE} = 'lighttpd.conf';

ok($tf->start_proc == 0, "Starting lighttpd") or die();

## check if If-Modified-Since, If-None-Match works

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
If-Modified-Since: Sun, 01 Jan 1970 00:00:01 GMT
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'Conditional GET - old If-Modified-Since');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
If-Modified-Since: Sun, 01 Jan 1970 00:00:01 GMT; foo
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, '+Last-Modified' => ''} ];
ok($tf->handle_http($t) == 0, 'Conditional GET - old If-Modified-Since, comment');

my $now = $t->{date};

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
If-Modified-Since: $now
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 304 } ];
ok($tf->handle_http($t) == 0, 'Conditional GET - new If-Modified-Since');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
If-Modified-Since: $now; foo
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 304 } ];
ok($tf->handle_http($t) == 0, 'Conditional GET - new If-Modified-Since, comment');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
If-None-Match: foo
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, '+ETag' => ''} ];
ok($tf->handle_http($t) == 0, 'Conditional GET - old If-None-Match');

my $etag = $t->{etag};

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
If-None-Match: $etag
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 304 } ];
ok($tf->handle_http($t) == 0, 'Conditional GET - old If-None-Match');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
If-None-Match: $etag
If-Modified-Since: Sun, 01 Jan 1970 00:00:01 GMT; foo
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'Conditional GET - ETag + old Last-Modified');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
If-None-Match: $etag
If-Modified-Since: $now; foo
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 304 } ];
ok($tf->handle_http($t) == 0, 'Conditional GET - ETag, Last-Modified + comment');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
If-None-Match: Foo
If-Modified-Since: Sun, 01 Jan 1970 00:00:01 GMT; foo
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'Conditional GET - old ETAG + old Last-Modified');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
If-None-Match: $etag
If-Modified-Since: $now foo
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 412 } ];
ok($tf->handle_http($t) == 0, 'Conditional GET - ETag + Last-Modified + overlong timestamp');

$t->{REQUEST}  = ( <<EOF
GET / HTTP/1.0
If-None-Match: $etag
Host: etag.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'Conditional GET - ETag + disabled etags on server side');

ok($tf->stop_proc == 0, "Stopping lighttpd");

