#!/usr/bin/env perl
BEGIN {
	# add current source dir to the include-path
	# we need this for make distcheck
	(my $srcdir = $0) =~ s,/[^/]+$,/,;
	unshift @INC, $srcdir;
}

use strict;
use Test::More tests => 56;
use LightyTest;

my $tf = LightyTest->new();

my $t;
my $php_child = -1;

my $phpbin = (defined $ENV{'PHP'} ? $ENV{'PHP'} : '/usr/bin/php-cgi');
$ENV{'PHP'} = $phpbin;

SKIP: {
	skip "PHP already running on port 1026", 1 if $tf->listening_on(1026);
	skip "no php binary found", 1 unless -x $phpbin;
	ok(-1 != ($php_child = $tf->spawnfcgi($phpbin, 1026)), "Spawning php");
}

SKIP: {
	skip "no PHP running on port 1026", 33 unless $tf->listening_on(1026);

	ok($tf->start_proc == 0, "Starting lighttpd") or goto cleanup;

	$t->{REQUEST} = ( <<EOF
GET /phpinfo.php HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
	ok($tf->handle_http($t) == 0, 'valid request');

	$t->{REQUEST}  = ( <<EOF
GET /phpinfofoobar.php HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 404 } ];
	ok($tf->handle_http($t) == 0, 'file not found');

	$t->{REQUEST}  = ( <<EOF
GET /go/ HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
	ok($tf->handle_http($t) == 0, 'index-file handling');

	$t->{REQUEST}  = ( <<EOF
GET /redirect.php HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 302, 'Location' => 'http://www.example.org:2048/' } ];
	ok($tf->handle_http($t) == 0, 'Status + Location via FastCGI');

	$t->{REQUEST}  = ( <<EOF
GET /redirect.php/ HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 302, 'Location' => 'http://www.example.org:2048/' } ];
	ok($tf->handle_http($t) == 0, 'Trailing slash as path-info (#1989: workaround broken operating systems)');

	$t->{REQUEST}  = ( <<EOF
GET /get-server-env.php?env=PHP_SELF HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
	ok($tf->handle_http($t) == 0, '$_SERVER["PHP_SELF"]');

	$t->{REQUEST}  = ( <<EOF
GET /get-server-env.php/foo?env=SCRIPT_NAME HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => '/get-server-env.php' } ];
	ok($tf->handle_http($t) == 0, '$_SERVER["SCRIPT_NAME"]');

	$t->{REQUEST}  = ( <<EOF
GET /get-server-env.php/foo?env=PATH_INFO HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE}  = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => '/foo' } ];
	ok($tf->handle_http($t) == 0, '$_SERVER["PATH_INFO"]');

	$t->{REQUEST}  = ( <<EOF
GET /get-server-env.php?env=SERVER_NAME HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'www.example.org' } ];
	ok($tf->handle_http($t) == 0, 'SERVER_NAME');

	$t->{REQUEST}  = ( <<EOF
GET /get-server-env.php?env=SERVER_NAME HTTP/1.0
Host: foo.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'www.example.org' } ];
	ok($tf->handle_http($t) == 0, 'SERVER_NAME');

	$t->{REQUEST}  = ( <<EOF
GET /get-server-env.php?env=SERVER_NAME HTTP/1.0
Host: vvv.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'www.example.org' } ];
	ok($tf->handle_http($t) == 0, 'SERVER_NAME');

	$t->{REQUEST}  = ( <<EOF
GET /get-server-env.php?env=SERVER_NAME HTTP/1.0
Host: zzz.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'www.example.org' } ];
	ok($tf->handle_http($t) == 0, 'SERVER_NAME');

	$t->{REQUEST}  = ( <<EOF
GET /cgi.php/abc HTTP/1.0
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
	ok($tf->handle_http($t) == 0, 'PATHINFO');

	$t->{REQUEST}  = ( <<EOF
GET /cgi.php%20%20%20 HTTP/1.0
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 404 } ];
	ok($tf->handle_http($t) == 0, 'No source retrieval');

	$t->{REQUEST}  = ( <<EOF
GET /www/abc/def HTTP/1.0
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 404 } ];
	ok($tf->handle_http($t) == 0, 'PATHINFO on a directory');

	$t->{REQUEST}  = ( <<EOF
GET /indexfile/ HTTP/1.0
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => '/indexfile/index.php' } ];
	ok($tf->handle_http($t) == 0, 'PHP_SELF + Indexfile, Bug #3');

	$t->{REQUEST}  = ( <<EOF
GET /prefix.fcgi?var=SCRIPT_NAME HTTP/1.0
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => '/prefix.fcgi' } ];
	ok($tf->handle_http($t) == 0, 'PATH_INFO, check-local off');

	$t->{REQUEST}  = ( <<EOF
GET /prefix.fcgi/foo/bar?var=SCRIPT_NAME HTTP/1.0
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => '/prefix.fcgi' } ];
	ok($tf->handle_http($t) == 0, 'PATH_INFO, check-local off');

	$t->{REQUEST}  = ( <<EOF
GET /prefix.fcgi/foo/bar?var=PATH_INFO HTTP/1.0
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => '/foo/bar' } ];
	ok($tf->handle_http($t) == 0, 'PATH_INFO, check-local off');

	$t->{REQUEST}  = ( <<EOF
GET /sendfile.php?range=0- HTTP/1.0
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'Content-Length' => 4348 } ];
	ok($tf->handle_http($t) == 0, 'X-Sendfile2');

	$t->{REQUEST}  = ( <<EOF
GET /sendfile.php?range=0-4&range2=5- HTTP/1.0
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'Content-Length' => 4348 } ];
	ok($tf->handle_http($t) == 0, 'X-Sendfile2');


	ok($tf->stop_proc == 0, "Stopping lighttpd");


	$tf->{CONFIGFILE} = 'fastcgi-10.conf';
	ok($tf->start_proc == 0, "Starting lighttpd with $tf->{CONFIGFILE}") or goto cleanup;
	$t->{REQUEST}  = ( <<EOF
GET /get-server-env.php?env=SERVER_NAME HTTP/1.0
Host: zzz.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'zzz.example.org' } ];
	ok($tf->handle_http($t) == 0, 'FastCGI + Host');

	$t->{REQUEST}  = ( <<EOF
GET http://zzz.example.org/get-server-env.php?env=SERVER_NAME HTTP/1.0
Host: aaa.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'zzz.example.org' } ];
	ok($tf->handle_http($t) == 0, 'SERVER_NAME (absolute url in request line)');

	ok($tf->stop_proc == 0, "Stopping lighttpd");
	
	$tf->{CONFIGFILE} = 'bug-06.conf';
	ok($tf->start_proc == 0, "Starting lighttpd with $tf->{CONFIGFILE}") or goto cleanup;
	$t->{REQUEST}  = ( <<EOF
GET /indexfile/ HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => '/indexfile/index.php' } ];
	ok($tf->handle_http($t) == 0, 'Bug #6');

	ok($tf->stop_proc == 0, "Stopping lighttpd");

	$tf->{CONFIGFILE} = 'bug-12.conf';
	ok($tf->start_proc == 0, "Starting lighttpd with bug-12.conf") or goto cleanup;
	$t->{REQUEST}  = ( <<EOF
POST /indexfile/abc HTTP/1.0
Host: www.example.org
Content-Length: 0
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 404, 'HTTP-Content' => '/indexfile/return-404.php' } ];
	ok($tf->handle_http($t) == 0, 'Bug #12');

	ok($tf->stop_proc == 0, "Stopping lighttpd");
}

SKIP: {
	skip "PHP not started, cannot stop it", 1 unless $php_child != -1;
	ok(0 == $tf->endspawnfcgi($php_child), "Stopping php");
	$php_child = -1;
}

SKIP: {
	skip "no fcgi-auth found", 5 unless -x $tf->{BASEDIR}."/tests/fcgi-auth" || -x $tf->{BASEDIR}."/tests/fcgi-auth.exe"; 

	$tf->{CONFIGFILE} = 'fastcgi-auth.conf';
	ok($tf->start_proc == 0, "Starting lighttpd with $tf->{CONFIGFILE}") or die();
	$t->{REQUEST}  = ( <<EOF
GET /index.html?ok HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
	ok($tf->handle_http($t) == 0, 'FastCGI - Auth');

	$t->{REQUEST}  = ( <<EOF
GET /index.html?fail HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 403 } ];
	ok($tf->handle_http($t) == 0, 'FastCGI - Auth');

	$t->{REQUEST}  = ( <<EOF
GET /expire/access.txt?ok HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
	ok($tf->handle_http($t) == 0, 'FastCGI - Auth in subdirectory');

	ok($tf->stop_proc == 0, "Stopping lighttpd");
}

SKIP: {
	skip "no php found", 5 unless -x $phpbin;
	$tf->{CONFIGFILE} = 'fastcgi-13.conf';
	ok($tf->start_proc == 0, "Starting lighttpd with $tf->{CONFIGFILE}") or die();
	$t->{REQUEST}  = ( <<EOF
GET /indexfile/index.php HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 } ];
	ok($tf->handle_http($t) == 0, 'FastCGI + local spawning');

	$t->{REQUEST} = ( <<EOF
HEAD /indexfile/index.php HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, '-Content-Length' => '0' } ];
	# Of course a valid content-length != 0 would be ok, but we assume for now that such one is not generated.
	ok($tf->handle_http($t) == 0, 'Check for buggy content length with HEAD');

	$t->{REQUEST}  = ( <<EOF
GET /get-env.php?env=MAIL HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200 , 'HTTP-Content' => '' } ];
	ok($tf->handle_http($t) == 0, 'FastCGI + bin-copy-environment');



	ok($tf->stop_proc == 0, "Stopping lighttpd");
}


SKIP: {
	skip "no fcgi-responder found", 11 unless -x $tf->{BASEDIR}."/tests/fcgi-responder" || -x $tf->{BASEDIR}."/tests/fcgi-responder.exe";
	
	$tf->{CONFIGFILE} = 'fastcgi-responder.conf';
	ok($tf->start_proc == 0, "Starting lighttpd with $tf->{CONFIGFILE}") or die();
	$t->{REQUEST}  = ( <<EOF
GET /index.fcgi?lf HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'test123' } ];
	ok($tf->handle_http($t) == 0, 'line-ending \n\n');

	$t->{REQUEST}  = ( <<EOF
GET /index.fcgi?crlf HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'test123' } ];
	ok($tf->handle_http($t) == 0, 'line-ending \r\n\r\n');

	$t->{REQUEST}  = ( <<EOF
GET /index.fcgi?slow-lf HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'test123' } ];
	ok($tf->handle_http($t) == 0, 'line-ending \n + \n');

	$t->{REQUEST}  = ( <<EOF
GET /index.fcgi?slow-crlf HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'test123' } ];
	ok($tf->handle_http($t) == 0, 'line-ending \r\n + \r\n');

	$t->{REQUEST}  = ( <<EOF
GET /abc/def/ghi?path_info HTTP/1.0
Host: wsgi.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => '/abc/def/ghi' } ];
	ok($tf->handle_http($t) == 0, 'PATH_INFO (wsgi)');

	$t->{REQUEST}  = ( <<EOF
GET /abc/def/ghi?script_name HTTP/1.0
Host: wsgi.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => '' } ];
	ok($tf->handle_http($t) == 0, 'SCRIPT_NAME (wsgi)');


	$t->{REQUEST}  = ( <<EOF
GET /index.fcgi?die-at-end HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'test123' } ];
	ok($tf->handle_http($t) == 0, 'killing fastcgi and wait for restart');

	select(undef, undef, undef, .2);
	$t->{REQUEST}  = ( <<EOF
GET /index.fcgi?die-at-end HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'test123' } ];
	ok($tf->handle_http($t) == 0, 'killing fastcgi and wait for restart');


	select(undef, undef, undef, .2);
	$t->{REQUEST}  = ( <<EOF
GET /index.fcgi?crlf HTTP/1.0
Host: www.example.org
EOF
 );
	$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 200, 'HTTP-Content' => 'test123' } ];
	ok($tf->handle_http($t) == 0, 'regular response of after restart');


	ok($tf->stop_proc == 0, "Stopping lighttpd");
}

exit 0;

cleanup: ;

$tf->endspawnfcgi($php_child) if $php_child != -1;

die();
