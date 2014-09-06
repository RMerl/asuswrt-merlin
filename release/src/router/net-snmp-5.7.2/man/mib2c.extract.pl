#!/usr/bin/perl

# read comments from mib2c and insert them in mib2c.conf.5
my $mib2csrc = shift;

while (<>) {
    if (/COMMANDSHERE/) {
	open(I,$mib2csrc);
	while (<I>) {
	    last if (/^# which include:/);
	}
	while (<I>) {
	    last if (!/^#/);
	    s/^#\s+//;
	    # Avoid ' at the beginning of a line
	    s/^'/\\&'/;
	    # Quotes in a quoted argument must be doubled.
	    s/"/""/g;
	    s/^(\@.*\@)$/.IP "$1"/;
	    print;
	}
	close(I);
    } elsif (/VAREXPANSIONSHERE/) {
	open(I,$mib2csrc);
	while (<I>) {
	    last if (/^# Mib components,/);
	}
	while (<I>) {
	    last if (!/^#/);
	    next if (/^#\s*$/);
	    s/^#\s+//;
	    # Avoid ' at the beginning of a line
	    s/^'/\\&'/;
	    # Quotes in a quoted argument must be doubled.
	    s/"/""/g;
	    s/^(\S+)\s+--\s+(.*)/.IP "$1"\n$2/;
	    print;
	}
	close(I);
    } else {
	print;
    }
}
