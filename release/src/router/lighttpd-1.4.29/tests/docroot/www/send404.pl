#!/usr/bin/perl
use CGI qw/:standard/;
print header ( -status => 404
               -type   => 'text/plain' );
print ("send404\n");
