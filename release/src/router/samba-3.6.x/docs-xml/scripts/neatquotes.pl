#!/usr/bin/perl

my $inprog = 0;

while(<STDIN>) {
	if(/<(programlisting|screen)>/) { $inprog = 1; }
	if(/<\/(programlisting|screen)>/) { $inprog = 0; }
	if(not /="(.*)"/ and not $inprog) {
		s/"(.*?)"/<quote>\1<\/quote>/g;
	}
	print $_;
}
