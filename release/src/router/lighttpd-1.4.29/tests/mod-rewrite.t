#!/usr/bin/env perl
BEGIN {
	# add current source dir to the include-path
	# we need this for make distcheck
	(my $srcdir = $0) =~ s,/[^/]+$,/,;
	unshift @INC, $srcdir;
}

use strict;
use IO::Socket;
use Test::More tests => 8;
use LightyTest;

my $tf = LightyTest->new();
my $t;
my $php_child = -1;

my $phpbin = (defined $ENV{'PHP'} ? $ENV{'PHP'} : '/usr/bin/php-cgi');

SKIP: {
	skip "PHP already running on port 1026", 1 if $tf->listening_on(1026);
	skip "no php binary found", 1 unless -x $phpbin;
	ok(-1 != ($php_child = $tf->spawnfcgi($phpbin, 1026)), "Spawning php");
}

SKIP: {
	skip "no PHP running on port 1026", 6 unless $tf->listening_on(1026);

	ok($tf->start_proc == 0, "Starting lighttpd") or goto cleanup;

	$t->{REQUEST}  = ( <<EOF
GET /rewrite/foo HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => '' } ];
	ok($tf->handle_http($t) == 0, 'valid request');
    
	$t->{REQUEST}  = ( <<EOF
GET /rewrite/foo?a=b HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'a=b' } ];
	ok($tf->handle_http($t) == 0, 'valid request');

	$t->{REQUEST}  = ( <<EOF
GET /rewrite/bar?a=b HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'bar&a=b' } ];
	ok($tf->handle_http($t) == 0, 'valid request');

	$t->{REQUEST}  = ( <<EOF
GET /rewrite/nofile?a=b HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'file=/rewrite/nofile&a=b' } ];
	ok($tf->handle_http($t) == 0, 'not existing file rewrite');

	ok($tf->stop_proc == 0, "Stopping lighttpd");
}

SKIP: {
	skip "PHP not started, cannot stop it", 1 unless $php_child != -1;
	ok(0 == $tf->endspawnfcgi($php_child), "Stopping php");
}


exit 0;

cleanup: ;

$tf->endspawnfcgi($php_child) if $php_child != -1;

die();
