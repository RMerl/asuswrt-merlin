use ExtUtils::MakeMaker;
require 5;
use Config;
use Getopt::Long;
my $lib_version;
my %MakeParams = ();

# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

%MakeParams = InitMakeParams();

WriteMakefile(%MakeParams);

Check_Version();

sub InitMakeParams {
    my $opts;
    my %Params = (
		  'NAME'		=> 'NetSNMP::agent::Support',
		  'VERSION_FROM'	=> 'Support.pm', # finds $VERSION
		  'XSPROTOARG'          => '-prototypes',
		  'PREREQ_PM'           => {},
		  );

    if ($ENV{'OSTYPE'} eq 'msys') {
      $Params{'DEFINE'} = "-DMINGW_PERL";
    }
                  
    my ($snmp_lib, $snmp_llib, $sep);

    if (($Config{'osname'} eq 'MSWin32' && $ENV{'OSTYPE'} eq '')) {
      $opts = NetSNMPGetOpts();	
      $Params{'DEFINE'} = "-DMSVC_PERL -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS";
      $sep = '\\';
      $snmp_lib_file = 'netsnmp.lib';
      $snmp_link_lib = 'netsnmp';

      if (lc($opts->{'debug'}) eq "true") {
        $lib_dir = 'lib\\debug';
      }
      else {
        $lib_dir = 'lib\\release';
      }
      
      if (lc($opts->{'insource'}) eq "true") {
	$Params{'LIBS'} = "-L$basedir\\win32\\$lib_dir\\ -l$snmp_link_lib";
      }
      else {
	my @LibDirs = split (';',$ENV{LIB});
        my $LibDir;
	if ($opts->{'prefix'}) {
	  push (@LibDirs,"$ENV{'NET-SNMP-PATH'}${sep}lib");
	}
	$noLibDir = 1;
	while ($noLibDir) {
	  $LibDir = find_files(["$snmp_lib_file"],\@LibDirs);
	  if ($LibDir ne '') {
	    $noLibDir = 0;
            # Put quotes around LibDir to allow spaces in paths
            $LibDir = '"' . $LibDir . '"';
	  }
	  else
	  {
	    @LibDirs = ();
	    @LibDirs[0] = prompt("The Net-SNMP library ($snmp_lib_file) could not be found.\nPlease enter the directory where it is located:");
	    @LibDirs[0] =~ s/\\$//;
	  }
	}
	$Params{LIBS} = "-L$LibDir -l$snmp_link_lib";
      }

      $Params{'INC'} = "-I$basedir\\include\\ -I$basedir\\include\\net-snmp\\ -I$basedir\\win32\\ ";
    }
    else
    {
      $opts = NetSNMPGetOpts("../../");
      $Params{'LIBS'}    = `$opts->{'nsconfig'} --libs`;
      chomp($Params{'LIBS'});
      $Params{'CCFLAGS'} = `$opts->{'nsconfig'} --cflags`;
      chomp($Params{'CCFLAGS'});
      $Params{'CCFLAGS'} .= " " . $Config{'ccflags'};
      $lib_version = `$opts->{'nsconfig'} --version`;
      
      if (lc($opts->{'insource'}) eq "true") {
	$Params{'LIBS'} = "-L../../snmplib/.libs -L../../snmplib/ " . $Params{'LIBS'};
	$Params{'CCFLAGS'} = "-I../../include " . $Params{'CCFLAGS'};
      }
      $Params{'CCFLAGS'} =~ s/ -W(all|inline|strict-prototypes|write-strings|cast-qual|no-char-subscripts)//g; # ignore developer warnings
      if ($Params{'LIBS'} eq "" || $Params{'CCFLAGS'} eq "") {
	die "You need to install net-snmp first (I can't find net-snmp-config)";
      }
    }
    return (%Params);
}
# common subroutines -- DO NOT EDIT.
# They are imported from the Makefile.subs.pl file
sub NetSNMPGetOpts {
    my %ret;
    my $rootpath = shift;
    $rootpath = "../" if (!$rootpath);
    $rootpath .= '/' if ($rootpath !~ /\/$/);
    
    if (($Config{'osname'} eq 'MSWin32' && $ENV{'OSTYPE'} eq '')) {

      # Grab command line options first.  Only used if environment variables are not set
      GetOptions("NET-SNMP-IN-SOURCE=s" => \$ret{'insource'},
        "NET-SNMP-PATH=s"      => \$ret{'prefix'},          
        "NET-SNMP-DEBUG=s"     => \$ret{'debug'});

      if ($ENV{'NET-SNMP-IN-SOURCE'})
      {
	$ret{'insource'} = $ENV{'NET-SNMP-IN-SOURCE'};
        undef ($ret{'prefix'});
      }
      elsif ($ENV{'NET-SNMP-PATH'})
      {
	$ret{'prefix'} = $ENV{'NET-SNMP-PATH'};
      }

      if ($ENV{'NET-SNMP-DEBUG'})
      {
	$ret{'debug'} = $ENV{'NET-SNMP-DEBUG'};
      }

      # Update environment variables in case they are needed
      $ENV{'NET-SNMP-IN-SOURCE'}    = $ret{'insource'};
      $ENV{'NET-SNMP-PATH'}         = $ret{'prefix'};
      $ENV{'NET-SNMP-DEBUG'}        = $ret{'debug'};        
     
      $basedir = `%COMSPEC% /c cd`;
      chomp $basedir;
      $basedir =~ /(.*?)\\perl.*/;
      $basedir = $1;
      print "Net-SNMP base directory: $basedir\n";
      if ($basedir =~ / /) {
        die "\nA space has been detected in the base directory.  This is not " .
            "supported\nPlease rename the folder and try again.\n\n";
      }
    }
    else
    {
      if ($ENV{'NET-SNMP-CONFIG'} && 
        $ENV{'NET-SNMP-IN-SOURCE'}) {
	# have env vars, pull from there
	$ret{'nsconfig'} = $ENV{'NET-SNMP-CONFIG'};
	$ret{'insource'} = $ENV{'NET-SNMP-IN-SOURCE'};
      } else {
	# don't have env vars, pull from command line and put there
	GetOptions("NET-SNMP-CONFIG=s" => \$ret{'nsconfig'},
	           "NET-SNMP-IN-SOURCE=s" => \$ret{'insource'});

	if (lc($ret{'insource'}) eq "true" && $ret{'nsconfig'} eq "") {
	    $ret{'nsconfig'}="sh ROOTPATH../net-snmp-config";
	} elsif ($ret{'nsconfig'} eq "") {
	    $ret{'nsconfig'}="net-snmp-config";
	}

	$ENV{'NET-SNMP-CONFIG'}    = $ret{'nsconfig'};
	$ENV{'NET-SNMP-IN-SOURCE'} = $ret{'insource'};
      }
    }	
    
    $ret{'nsconfig'} =~ s/ROOTPATH/$rootpath/;

    $ret{'rootpath'} = $rootpath;

    \%ret;
}

sub find_files {
    my($f,$d) = @_;
    my ($dir,$found,$file);
    for $dir (@$d){
	$found = 0;
	for $file (@$f) {
	    $found++ if -f "$dir/$file";
	}
	if ($found == @$f) {
	    return $dir;
	}
    }
}


sub Check_Version {
  if (($Config{'osname'} ne 'MSWin32' || $ENV{'OSTYPE'} ne '')) {
    my $foundversion = 0;
    return if ($ENV{'NETSNMP_DONT_CHECK_VERSION'});
    open(I,"<Makefile");
    while (<I>) {
	if (/^VERSION = (.*)/) {
	    my $perlver = $1;
	    my $srcver = $lib_version;
	    chomp($srcver);
	    my $srcfloat = floatize_version($srcver);
	    $perlver =~ s/pre/0./;
	    if ($srcfloat ne $perlver) {
		if (!$foundversion) {
		    print STDERR "ERROR:
Net-SNMP installed version: $srcver => $srcfloat
Perl Module Version:        $perlver

These versions must match for perfect support of the module.  It is possible
that different versions may work together, but it is strongly recommended
that you make these two versions identical.  You can get the Net-SNMP
source code and the associated perl modules directly from 

   http://www.net-snmp.org/

If you want to continue anyway please set the NETSNMP_DONT_CHECK_VERSION
environmental variable to 1 and re-run the Makefile.PL script.\n";
		    exit(1);
		}
	    }
	    $foundversion = 1;
	    last;
	}
    }
    close(I);
    die "ERROR: Couldn't find version number of this module\n" 
      if (!$foundversion);
  }
}

sub floatize_version {
    my ($major, $minor, $patch, $opps) = ($_[0] =~ /^(\d+)\.(\d+)\.?(\d*)\.?(\d*)/);
    return $major + $minor/100 + $patch/10000 + $opps/100000;
}
