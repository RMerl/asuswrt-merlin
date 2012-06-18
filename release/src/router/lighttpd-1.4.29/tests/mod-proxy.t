#!/usr/bin/env perl
BEGIN {
	# add current source dir to the include-path
	# we need this for make distcheck
	(my $srcdir = $0) =~ s,/[^/]+$,/,;
	unshift @INC, $srcdir;
}

use strict;
use IO::Socket;
use Test::More tests => 9;
use LightyTest;

my $tf_real = LightyTest->new();
my $tf_proxy = LightyTest->new();

my $t;
my $php_child = -1;

my $phpbin = (defined $ENV{'PHP'} ? $ENV{'PHP'} : '/usr/bin/php-cgi');
$ENV{'PHP'} = $phpbin;

SKIP: {
	skip "PHP already running on port 1026", 1 if $tf_real->listening_on(1026);
	skip "no php binary found", 1 unless -x $phpbin;
	ok(-1 != ($php_child = $tf_real->spawnfcgi($phpbin, 1026)), "Spawning php");
}

## we need two procs
## 1. the real webserver
## 2. the proxy server

$tf_real->{PORT} = 2048;
$tf_real->{CONFIGFILE} = 'lighttpd.conf';

$tf_proxy->{PORT} = 2050;
$tf_proxy->{CONFIGFILE} = 'proxy.conf';

ok($tf_real->start_proc == 0, "Starting lighttpd") or goto cleanup;

ok($tf_proxy->start_proc == 0, "Starting lighttpd as proxy") or goto cleanup;

$t->{REQUEST}  = ( <<EOF
GET /index.html HTTP/1.0
Host: www.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
ok($tf_proxy->handle_http($t) == 0, 'valid request');

$t->{REQUEST}  = ( <<EOF
GET /index.html HTTP/1.0
Host: www.example.org
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'Server' => 'Apache 1.3.29' } ];
ok($tf_proxy->handle_http($t) == 0, 'drop Server from real server');

SKIP: {
	skip "no PHP running on port 1026", 1 unless $tf_real->listening_on(1026);
	$t->{REQUEST}  = ( <<EOF
GET /rewrite/all/some+test%3axxx%20with%20space HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => '/some+test%3axxx%20with%20space' } ];
	ok($tf_proxy->handle_http($t) == 0, 'rewrited urls work with encoded path');
}

ok($tf_proxy->stop_proc == 0, "Stopping lighttpd proxy");

ok($tf_real->stop_proc == 0, "Stopping lighttpd");

SKIP: {
	skip "PHP not started, cannot stop it", 1 unless $php_child != -1;
	ok(0 == $tf_real->endspawnfcgi($php_child), "Stopping php");
	$php_child = -1;
}

exit 0;

cleanup:

$tf_real->endspawnfcgi($php_child) if $php_child != -1;

die();
