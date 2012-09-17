#!/usr/bin/perl
#use CGI qw/:standard/;
use CGI::Fast qw(:standard);
my $cgi = new CGI;
while (new CGI::Fast) {
    my $request_uri = $ENV{'REQUEST_URI'};
    print (STDERR "REQUEST_URI: $request_uri\n");
    if ($request_uri =~ m/^\/dynamic\/200\// ) {
      print header ( -status => 200,
                     -type   => 'text/plain' );
      print ("found here\n");
    }
    elsif ($request_uri =~ m|^/dynamic/302/| ) {
      print header( -status=>302,
                    -location => 'http://www.example.org/');
    }
    elsif ($request_uri =~ m/^\/dynamic\/404\// ) {
      print header ( -status => 404
                     -type   => 'text/plain' );
      print ("Not found here\n");
    }
    else {
      print header ( -status => 500,
                     -type   => 'text/plain');
      print ("huh\n");
    };
};
