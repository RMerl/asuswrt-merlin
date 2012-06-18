#!/usr/bin/env perl
BEGIN {
	# add current source dir to the include-path
	# we need this for make distcheck
	(my $srcdir = $0) =~ s,/[^/]+$,/,;
	unshift @INC, $srcdir;
}

use strict;
use IO::Socket;
use Test::More tests => 4;
use LightyTest;

my $tf = LightyTest->new();
my $t;

ok($tf->start_proc == 0, "Starting lighttpd") or die();

# mod-cgi
#
$t->{REQUEST}  = ( <<EOF
GET /ssi.shtml HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => "/ssi.shtml\n" } ];
ok($tf->handle_http($t) == 0, 'ssi - echo ');


## bug #280
$t->{REQUEST}  = ( <<EOF
GET /exec-date.shtml HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => "2\n\n" } ];
ok($tf->handle_http($t) == 0, 'ssi - echo ');


ok($tf->stop_proc == 0, "Stopping lighttpd");

