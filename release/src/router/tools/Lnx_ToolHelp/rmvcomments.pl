#!/usr/bin/perl

use IO::File;

$inFile = shift;
unless ($outFile = shift) {
   $outFile = $inFile . ".new";
   $inPlace = 1;
}

$inFH = IO::File->new($inFile, "r");
$outFH = IO::File->new($outFile, "w");

@lines = <$inFH>;
$data = join "", @lines;

sub strip_comments($)
{
    my $script = shift;
    return $script;
}

$data =~ s{
    (<SCRIPT .*?</SCRIPT>)|
    (<STYLE .*?</STYLE>)|
    <!--[^[<]              # comments begin with a `<!--'
                        # followed by 0 or more comments;
    [^\#]*?                # and includes all text up to and including
    -->                    # the *next* occurrence of `-->'
    \s*                    # followed by 0 or more characters of whitespace
    }{
        if ($1) {
            strip_comments($1);
        } elsif ($2) {
            $2;
        }
    }gesxi;
	
print $outFH $data;


if ($inPlace) {
   rename $inFile, $inFile . ".bak";    # backup original file 
   rename $outFile, $inFile;            # rename new file as the original 
   unlink $inFile . ".bak";
}

