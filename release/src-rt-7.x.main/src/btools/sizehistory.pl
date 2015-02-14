#!/usr/bin/perl
#
#	sizehistory
#	Copyright (C) 2006 Jonathan Zarate
#

if (($#ARGV < 0) || ($#ARGV > 1)) {
	print "Usage: sizehistory <filename> [datafile]\n";
	exit 1;
}

$fname = $ARGV[0];
$dname = ($ARGV[1] || $fname) . ".size";

print "\nSize history for $fname\n\n";
@size = `arm-linux-size $fname`;
foreach (@size) {
	if (($text, $data, $bss, $total) = $_ =~ /^\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+/) {
		$line = "$text\t$data\t$bss\t$total\t" . scalar localtime((stat($fname))[10]);
		print "text\tdata\tbss\ttotal\ttime\n";

		if (open(F, "<$dname")) {
			@hist = <F>;
			close(F);
			if ($#hist >= 0) {
				if ($#hist > 20) {
					splice(@hist, 0, $#hist - 19);
				}
				print @hist;
				if ($hist[$#hist] =~ /^(\d+)\t(\d+)\t(\d+)\t(\d+)\t/) {
					if (($1 == $text) && ($2 == $data) && ($3 == $bss)) {
						print "--- same size as last ---\n$line <= current\n";
						exit 0;
					}
					printf "%d\t%d\t%d\t%d\t--- changes since above ---\n",  ($text - $1), ($data - $2), ($bss - $3), ($total - $4);
				}
			}
		}

		print $line, "\n";

		if ((localtime((stat($dname))[9]))[7] != (localtime())[7]) {
			open(F, ">>$dname") || die "$dname: $!";
			print F $line, "\n";
			close(F);
		}
		exit 0;
	}
}
exit 1;
