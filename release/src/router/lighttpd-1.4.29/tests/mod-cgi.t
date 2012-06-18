#!/usr/bin/env perl
BEGIN {
	# add current source dir to the include-path
	# we need this for make distcheck
	(my $srcdir = $0) =~ s,/[^/]+$,/,;
	unshift @INC, $srcdir;
}

use strict;
use IO::Socket;
use Test::More tests => 18;
use LightyTest;

my $tf = LightyTest->new();
my $t;

ok($tf->start_proc == 0, "Starting lighttpd") or die();

# mod-cgi
#
$t->{REQUEST}  = ( <<EOF
GET /cgi.pl HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'perl via cgi');

$t->{REQUEST}  = ( <<EOF
GET /cgi.pl/foo HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => '/cgi.pl' } ];
ok($tf->handle_http($t) == 0, 'perl via cgi + pathinfo');

$t->{REQUEST}  = ( <<EOF
GET /cgi-pathinfo.pl/foo HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => '/foo' } ];
ok($tf->handle_http($t) == 0, 'perl via cgi + pathinfo');

$t->{REQUEST}  = ( <<EOF
GET /nph-status.pl?30 HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'NPH + perl, invalid status-code (#14)');

$t->{REQUEST}  = ( <<EOF
GET /nph-status.pl?304 HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 304 } ];
ok($tf->handle_http($t) == 0, 'NPH + perl, setting status-code (#1125)');

$t->{REQUEST}  = ( <<EOF
GET /nph-status.pl?200 HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf->handle_http($t) == 0, 'NPH + perl, setting status-code');

$t->{REQUEST} = ( <<EOF
GET /get-header.pl?GATEWAY_INTERFACE HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'CGI/1.1' } ];
ok($tf->handle_http($t) == 0, 'cgi-env: GATEWAY_INTERFACE');

$t->{REQUEST} = ( <<EOF
GET /get-header.pl?QUERY_STRING HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'QUERY_STRING' } ];
ok($tf->handle_http($t) == 0, 'cgi-env: QUERY_STRING');

$t->{REQUEST} = ( <<EOF
GET /get-header.pl?GATEWAY_INTERFACE HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'CGI/1.1' } ];
ok($tf->handle_http($t) == 0, 'cgi-env: GATEWAY_INTERFACE');

$t->{REQUEST} = ( <<EOF
GET /get-header.pl?HTTP_HOST HTTP/1.0
Host: www.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'www.example.org' } ];
ok($tf->handle_http($t) == 0, 'cgi-env: HTTP_HOST');

$t->{REQUEST}  = ( <<EOF
GET /get-header.pl?HTTP_XX_YY123 HTTP/1.0
xx-yy123: foo
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'foo' } ];
ok($tf->handle_http($t) == 0, 'cgi-env: quoting headers with numbers');

$t->{REQUEST}  = ( <<EOF
GET /get-header.pl?HTTP_HOST HTTP/1.0
Host: www.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'www.example.org' } ];
ok($tf->handle_http($t) == 0, 'cgi-env: HTTP_HOST');

$t->{REQUEST}  = ( <<EOF
GET /get-header.pl?HTTP_HOST HTTP/1.0
Host: www.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'www.example.org' } ];
ok($tf->handle_http($t) == 0, 'cgi-env: HTTP_HOST');

$t->{REQUEST}  = ( <<EOF
GET /get-header.pl?HTTP_HOST HTTP/1.0
Host: www.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'Content-Type' => 'text/plain' } ];
ok($tf->handle_http($t) == 0, 'cgi-env: HTTP_HOST');

$t->{REQUEST}  = ( <<EOF
GET /get-header.pl?HTTP_HOST HTTP/1.1
Host: www.example.org
Connection: close
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.1', 'HTTP-Status' => 200, '+Content-Length' => '' } ];
ok($tf->handle_http($t) == 0, 'cgi-env: HTTP_HOST');

# broken header crash
$t->{REQUEST}  = ( <<EOF
GET /crlfcrash.pl HTTP/1.0
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 302, 'Location' => 'http://www.example.org/' } ];
ok($tf->handle_http($t) == 0, 'broken header via perl cgi');

ok($tf->stop_proc == 0, "Stopping lighttpd");

