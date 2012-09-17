#!/usr/bin/perl


print "Content-Type: text/plain\r\n\r\n";

if ($ENV{"REQUEST_METHOD"} eq "POST") {
	my $l = 0;
	while(<>) {
		$l += length($_);
	}
	print $l;
} else {
	print "0";
}

