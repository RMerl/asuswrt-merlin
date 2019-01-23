#!/usr/bin/perl -w

open( INFILE, "$ARGV[0]" ) || die $@;

$count = 0;
while ( <INFILE> ) {
	next if ($_ =~ /^#define/);
	$count++ if (length($_) > 80);
}

close( INFILE );
print "$ARGV[0]: $count lines > 80 characters\n" if ($count > 0);

exit( 0 );


