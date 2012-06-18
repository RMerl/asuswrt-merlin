#!/usr/bin/env perl
BEGIN {
	# add current source dir to the include-path
	# we need this for make distcheck
	(my $srcdir = $0) =~ s,/[^/]+$,/,;
	unshift @INC, $srcdir;
}

use strict;
use IO::Socket;
use Test::More tests => 10;
use LightyTest;

my $tf = LightyTest->new();
my $t;

$tf->{CONFIGFILE} = 'lowercase.conf';

ok($tf->start_proc == 0, "Starting lighttpd") or die();

## check if lower-casing works

$t->{REQUEST}  = ( <<EOF
GET /image.JPG HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'uppercase access');

$t->{REQUEST}  = ( <<EOF
GET /image.jpg HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'lowercase access');

## check that mod-auth works

$t->{REQUEST}  = ( <<EOF
GET /image.JPG HTTP/1.0
Host: lowercase-auth
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 401 } ];
ok($tf->handle_http($t) == 0, 'uppercase access');

$t->{REQUEST}  = ( <<EOF
GET /image.jpg HTTP/1.0
Host: lowercase-auth
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 401 } ];
ok($tf->handle_http($t) == 0, 'lowercase access');


## check that mod-staticfile exclude works
$t->{REQUEST}  = ( <<EOF
GET /image.JPG HTTP/1.0
Host: lowercase-exclude
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 403 } ];
ok($tf->handle_http($t) == 0, 'upper case access to staticfile.exclude-extension');

$t->{REQUEST}  = ( <<EOF
GET /image.jpg HTTP/1.0
Host: lowercase-exclude
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 403 } ];
ok($tf->handle_http($t) == 0, 'lowercase access');


## check that mod-access exclude works
$t->{REQUEST}  = ( <<EOF
GET /image.JPG HTTP/1.0
Host: lowercase-deny
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 403 } ];
ok($tf->handle_http($t) == 0, 'uppercase access to url.access-deny protected location');

$t->{REQUEST}  = ( <<EOF
GET /image.jpg HTTP/1.0
Host: lowercase-deny
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 403 } ];
ok($tf->handle_http($t) == 0, 'lowercase access');



ok($tf->stop_proc == 0, "Stopping lighttpd");

