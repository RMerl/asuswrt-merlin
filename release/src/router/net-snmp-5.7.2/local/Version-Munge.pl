#!/usr/bin/perl

use Getopt::Std;

sub usage {
    print "
$0 [-v VERSION] -R -C -M -D -h

  -M           Modify the files with a new version (-v required)
  -v VERSION   Use VERSION as the version string
  -T TAG       Use TAG as SVN tag (must being with Ext-)
  -C           Commit changes to the files
  -R           Revert changes to the files
  -D           Compare files (svn diff)
  -f FILE      Just do a particular file
  -t TYPE      Just do a particular type of file
  -P           Print resulting modified lines
  -V           verbose
";
    exit 1;
}

getopts("Pv:T:RCMDhnf:t:V",\%opts) || usage();
if ($opts{'h'}) { usage(); }

if (!$opts{'v'} && $opts{'M'} && !$opts{'T'}) {
  warn "no version (-v or -T) specified";
  usage;
}
if (!$opts{'R'} && !$opts{'M'} && !$opts{'C'} && !$opts{'D'}) {
  warn "nothing to do (need -R -C -D or -M)\n";
  usage;
}

my @exprs = (
	     # documentation files
	     { type => 'docs',
	       expr => 'Version: [\.0-9a-zA-Z]+',
	       repl => 'Version: $VERSION', 
	       files => [qw(README FAQ dist/net-snmp.spec)],
	       not_required => {'dist/net-snmp.spec' => 1}
	     },

	     # Makefiles
	     { type => 'Makefile',
	       expr => 'VERSION = [\.0-9a-zA-Z]+',
	       repl => 'VERSION = $VERSION',
	       files => [qw(dist/Makefile)],
	       not_required => {'dist/Makefile' => 1}
	     },

	     # perl files
	     { type => 'perl',
	       expr => 'VERSION = \'(.*)\'',
	       repl => 'VERSION = \'$VERSION_FLOAT\'',
	       files => [qw(perl/SNMP/SNMP.pm
			    perl/agent/agent.pm
			    perl/agent/Support/Support.pm
			    perl/agent/default_store/default_store.pm
			    perl/default_store/default_store.pm
			    perl/OID/OID.pm
			    perl/ASN/ASN.pm
			    perl/AnyData_SNMP/Storage.pm
			    perl/AnyData_SNMP/Format.pm
			    perl/TrapReceiver/TrapReceiver.pm
			   )],
	       not_required => {'perl/agent/Support/Support.pm' => 1}
	     },

	     # configure script files
	     { type => 'configure',
	       expr => 'AC_INIT\\(\\[Net-SNMP\\], \\[([^\\]]+)\\]',
	       repl => 'AC_INIT([Net-SNMP], [$VERSION]',
	       files => [qw(configure.ac)],
	       exec => 'autoconf',
	       exfiles => [qw(configure)],
	     },

	    );

#
# set up versioning information
#
if ($opts{'T'} && !$opts{'v'}) {
    $opts{'v'} = $opts{'T'};
    die "usage error: version tag must begin with Ext-" if ($opts{'T'} !~ /^Ext-/);
    $opts{'v'} =~ s/^Ext-//;
    $opts{'v'} =~ s/-/./g;
}
$VERSION = $opts{'v'};
$VERSION_FLOAT = floatize_version($VERSION);


#
# loop through all the expression types
#
my @files;
for ($i = 0; $i <= $#exprs; $i++) {

    # drop other file types if only one was requested.
    next if ($opts{'t'} && $exprs[$i]{'type'} ne $opts{'t'});

    # loop through each file and process
    foreach my $f (@{$exprs[$i]->{'files'}}) {

	# skip files that weren't specifically in the todo list if need be
	next if ($opts{'f'} && $f ne $opts{'f'});

	# remove the changes and revert to SVN
	if ($opts{'R'}) {
	    print "removing changes and updating $f\n" if ($opts{'V'});
	    system("svn revert $f");
	}

	# make sure it exists
	if (! -f $f) {
	    if (!exists($exprs[$i]->{'not_required'}{$f})) {
		print STDERR "FAILED to find file $f\n";
		exit(1);
	    } else {
		print STDERR "SKIPPING file $f\n";
		next;
	    }
	}

	# modify the files with the version
	if ($opts{'M'}) {
	    rename ($f,"$f.bak");
	    open(I,"$f.bak");
	    open(O,">$f");
	    while (<I>) {
		my $res = eval "s/$exprs[$i]->{'expr'}/$exprs[$i]->{'repl'}/";
		if ($res && $opts{'P'}) {
		    my $shortened = $_;
		    $shortened =~ s/^\s*//;
		    printf("%s:\n          %s", $f, $shortened);
		}
		print O;
	    }
	    close(I);
	    close(O);
	    unlink("$f.bak");
	    push @files, $f;
	    print "modified $f using s/$exprs[$i]->{'expr'}/$exprs[$i]->{'repl'}/\n" if ($opts{'V'});
	}

	# run diff if requested.
	if ($opts{'D'}) {
	    print "diffing $f\n" if ($opts{'V'});
	    system("svn diff $f");
	}
    }
    system($exprs[$i]->{'exec'}) if ($exprs[$i]->{'exec'});
    push @files, @{$exprs[$i]->{'exfiles'}} if ($exprs[$i]->{'exfiles'});
}

#
# commit the modified files
#
if ($opts{'C'}) {
    my $files = join(" ",@files);
    print "committing $files\n" if ($opts{'V'});
    $ret = system("svn commit -m \"- version tag ( $VERSION )\" $files");
    exit($ret);
}

sub floatize_version {
    my ($major, $minor, $patch, $opps) = ($_[0] =~ /^(\d+)\.(\d+)\.?(\d*)\.?(\d*)/);
    return $major + $minor/100 + $patch/10000 + $opps/100000;
}
