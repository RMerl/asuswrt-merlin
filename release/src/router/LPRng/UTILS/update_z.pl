#!/usr/bin/perl
eval 'exec /usr/bin/perl -S $0 ${1+"$@"}'
    if $running_under_some_shell;
            # this emulates #! processing on NIH machines.
            # (remove #! line above if indigestible)

use Getopt::Std;
my $debug = 0;    # always... sigh...
my(%opt, @pc, %options);

# get command line options
getopts( 'A:B:C:D:E:F:G:H:I:J:K:L:M:N:O:P:Q:R:T:S:U:V:W:X:Y:Z:'
. 'a:b:cd:e:f:g:h:i:j:k:l:m:n:o:p:q:r:t:s:u:v:w:x:y:z:', \%opt );
while( @ARGV ){ $opt{acct} = pop @ARGV ; };

# split up the PRINTCAP_ENTRY environment variable value
@pc = split /\n\s*:/s, ($ENV{PRINTCAP_ENTRY} || "");
shift @pc;  # throw way first entry field, printer name
# set the options 
foreach (@pc){ # set the options values
	if( /^(.+)=(.*)/ ){ $options{$1} = $2;
	} elsif ( /^(.+)@/ ){ $options{$1} = 0;
	} else { $options{$_} = 1; }
}

if( $debug ){ # for those interested
	$s="";
	foreach my $v (sort keys %ENV){ $s .= "$v='$ENV{$v}',"; }
	print STDERR "ENV: '$s'\n";
	my $s = "";
	foreach my $v (sort keys %options ){ $s .= "$v='$options{$v}',"; }
	print STDERR "Printcap: '$s'\n";
	#$s="";
	#foreach my $v (sort keys %opt){ $s .= "$v='$opt{$v}',"; }
	#print STDERR "Args: '$s'\n";
}

# read stdin
my( $file, $Zopts, $Q );
$file = join "", <STDIN>;
print STDERR "File '$file'\n" if $debug;
$Zopts = "";
# first use command line Queue name
$Q = $opt{Q};
($Q) = $file =~ /^Q(.*)$/m if not $Q;
# if no queue name fall back to printer name
$Q = $opt{P} if not $Q;
($Q) = $file =~ /^P(.*)$/m if not $Q;
$Q = "" if not $Q;

($Zopts) = $file =~ /^Z(.*)$/m;
$Zopts = "" if not $Zopts;

print STDERR "Q '$Q', Zopts '$Zopts'\n" if $debug;

# now we split up the name and use as parameters for Z options
while( $Q =~ /_([^_]+)/g ){
	# you can simply append them:
    $Zopts .= ",$1";
	# or you can test and then append translated format
	# if( $1 eq "11" ){ $Zopts .= ",legal"; }
    #  elsif( $1 eq "15" ){ $Zopts .= ",ledger"; }
    #
	#if( $1 eq "landscape"
	#	or $1 eq "legal"
	#	or $1 eq "ledger" ){
	#	$Zopts .= ",$1"
	#}
}
print "Final '$Zopts'\n" if $debug;
if( $Zopts ){
	# remove leading comma
	$Zopts =~ s/^,//; 
	#replace or prefix Z options
	$file = "Z$Zopts\n" . $file if( not ($file =~ s/^Z.*$/Z$Zopts/m));
}
print $file;
exit 0
