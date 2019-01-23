#!/usr/bin/perl -w

open( INFILE, "$ARGV[0]" ) || die $@;
open( OUTFILE, ">$ARGV[0].new" ) || die $@;

while ( <INFILE> ) { 
	$_ =~ s/[ \t\r]*$//; 
	print OUTFILE "$_"; 
}

close( INFILE );
close( OUTFILE );

rename( "$ARGV[0].new", "$ARGV[0]" ) || die @_;

exit( 0 );


