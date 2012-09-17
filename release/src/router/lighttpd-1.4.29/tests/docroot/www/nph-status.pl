#!/usr/bin/perl

my $status = 200;

if (defined $ENV{"QUERY_STRING"}) {
	$status = $ENV{"QUERY_STRING"};
}

print "HTTP/1.0 ".$status." FooBar\r\n";
print "\r\n";
