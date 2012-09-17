#!/usr/bin/perl

my $s = $ENV{$ENV{"QUERY_STRING"}};

printf("Content-Length: %d\r\n", length($s));
print "Content-Type: text/plain\r\n\r\n";

print $s;
