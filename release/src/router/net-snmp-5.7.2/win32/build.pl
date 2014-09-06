#!/usr/bin/perl
# 
# Build script for Net-SNMP and MSVC
# Written by Alex Burger - alex_b@users.sourceforge.net
# March 12th, 2004
#
use strict;
my $openssl = "disabled";
my $b_ipv6 = "disabled";
my $b_winextdll = "disabled";
my $sdk = "disabled";
my $default_install_base = "c:/usr";
my $install_base = $default_install_base;
my $install = "enabled";
my $install_devel = "disabled";
my $perl = "disabled";
my $perl_install = "disabled";
my $logging = "enabled";
my $debug = "disabled";
my $configOpts = "";
my $cTmp = "";
my $linktype = "static";
my $option;

# Prepend win32\ if running from main directory
my $current_pwd = `%COMSPEC% /c cd`;
chomp $current_pwd;
if (! ($current_pwd =~ /\\win32$/)) {
  chdir ("win32");
  $current_pwd = `%COMSPEC% /c cd`;
  chomp $current_pwd;
}

if ( -d $ENV{MSVCDir} || -d $ENV{VCINSTALLDIR}) {
}
else {
  print "\nPlease run VCVARS32.BAT first to set up the Visual Studio build\n" .
        "environment.\n\n";
  system("pause");
  exit;
}

while (1) {
  print "\n\nNet-SNMP build and install options\n";
  print "==================================\n\n";
  print "1.  OpenSSL support:                " . $openssl. "\n";
  print "2.  Platform SDK support:           " . $sdk . "\n";
  print "\n";
  print "3.  Install path:                   " . $install_base . "\n";
  print "4.  Install after build:            " . $install . "\n";
  print "\n";
  print "5.  Perl modules:                   " . $perl . "\n";
  print "6.  Install perl modules:           " . $perl_install . "\n";
  print "\n";
  print "7.  Quiet build (logged):           " . $logging . "\n";
  print "8.  Debug mode:                     " . $debug . "\n";
  print "\n";
  print "9.  IPv6 transports (requires SDK): " . $b_ipv6 . "\n";
  print "10. winExtDLL agent (requires SDK): " . $b_winextdll . "\n";
  print "\n";
  print "11. Link type:                      " . $linktype . "\n";
  print "\n";
  print "12. Install development files       " . $install_devel . "\n";
  print "\nF.  Finished - start build\n";
  print "Q.  Quit - abort build\n\n";
  print "Select option to set / toggle: ";

  chomp ($option = <>);
  if ($option eq "1") {
    if ($openssl eq "enabled") {
      $openssl = "disabled";
    }
    else {
      $openssl = "enabled";
    }
  }
  elsif ($option eq "2") {
    if ($sdk eq "enabled") {
      $sdk = "disabled";
    }
    else {
      $sdk = "enabled";
    }
  }
  elsif ($option eq "9") {
    if ($b_ipv6 eq "enabled") {
      $b_ipv6 = "disabled";
    }
    else {
      $b_ipv6 = "enabled";
      if ($sdk = "disabled") {
        print "\n\n* SDK required for IPv6 and has been automatically enabled";
        $sdk = "enabled";
      }
    }
  }
  elsif ($option eq "10") {
    if ($b_winextdll eq "enabled") {
      $b_winextdll = "disabled";
    }
    else {
      $b_winextdll = "enabled";
      if ($sdk = "disabled") {
        print "\n\n* SDK required for IPv6 and has been automatically enabled";
        $sdk = "enabled";
      }
    }
  }
  elsif ($option eq "3") {
    print "Please enter the new install path [$default_install_base]: ";
    chomp ($install_base = <>);
    if ($install_base eq "") {
      $install_base = $default_install_base;
    }
    $install_base =~ s/\\/\//g;
  }
  elsif ($option eq "4") {
    if ($install eq "enabled") {
      $install = "disabled";
    }
    else {
      $install = "enabled";
    }
  }
  elsif ($option eq "12") {
    if ($install_devel eq "enabled") {
      $install_devel = "disabled";
    }
    else {
      $install_devel = "enabled";
    }
  }
  elsif ($option eq "5") {
    if ($perl eq "enabled") {
      $perl = "disabled";
    }
    else {
      $perl = "enabled";
    }
  }
  elsif ($option eq "6") {
    if ($perl_install eq "enabled") {
      $perl_install = "disabled";
    }
    else {
      $perl_install = "enabled";
    }
  }
  elsif ($option eq "7") {
    if ($logging eq "enabled") {
      $logging = "disabled";
    }
    else {
      $logging = "enabled";
    }
  }
  elsif ($option eq "8") {
    if ($debug eq "enabled") {
      $debug = "disabled";
    }
    else {
      $debug = "enabled";
    }
  }
  elsif ($option eq "11") {
    if ($linktype eq "static") {
      $linktype = "dynamic";
    }
    else {
      $linktype = "static";
    }
  }
  elsif (lc($option) eq "f") {
    last;
  }
  elsif (lc($option) eq "q") {
    exit;
  }
}

$cTmp = ($openssl eq "enabled" ? "--with-ssl" : "" );
$configOpts = "$cTmp";
$cTmp = ($sdk eq "enabled" ? "--with-sdk" : "" );
$configOpts = "$configOpts $cTmp";
$cTmp = ($b_ipv6 eq "enabled" ? "--with-ipv6" : "" );
$configOpts = "$configOpts $cTmp";
$cTmp = ($b_winextdll eq "enabled" ? "--with-winextdll" : "" );
$configOpts = "$configOpts $cTmp";
$cTmp = ($debug eq "enabled" ? "--config=debug" : "--config=release" );
$configOpts = "$configOpts $cTmp";

# Set environment variables

# Set to not search for non-existent ".dep" files
$ENV{NO_EXTERNAL_DEPS}="1";

# Set PATH environment variable so Perl make tests can locate the DLL
$ENV{PATH} = "$current_pwd\\bin\\" . ($debug eq "enabled" ? "debug" : "release" ) . ";$ENV{PATH}";

# Set MIBDIRS environment variable so Perl make tests can locate the mibs
my $temp_mibdir = "$current_pwd/../mibs";
$temp_mibdir =~ s/\\/\//g;
$ENV{MIBDIRS}=$temp_mibdir;

# Set SNMPCONFPATH environment variable so Perl conf.t test can locate
# the configuration files.
# See the note about environment variables in the Win32 section of 
# perl/SNMP/README for details on why this is needed. 
$ENV{SNMPCONFPATH}="t";$ENV{SNMPCONFPATH};

print "\nBuilding...\n";

if ($logging eq "enabled") {
  print "\nCreating *.out log files.\n\n";
}

if ($logging eq "enabled") {
  print "Deleting old log files...\n";
  system("del *.out > NUL: 2>&1");

  # Delete net-snmp-config.h from main include folder just in case it was created by a Cygwin or MinGW build
  system("del ..\\include\\net-snmp\\net-snmp-config.h > NUL: 2>&1");
  unlink "../snmplib/transports/snmp_transport_inits.h";
  
  print "Running Configure...\n";
  system("perl Configure $configOpts --linktype=$linktype --prefix=\"$install_base\" > configure.out 2>&1") == 0 || die "Build error (see configure.out)";

  print "Cleaning...\n";
  system("nmake /nologo clean > clean.out 2>&1") == 0 || die "Build error (see clean.out)";

  print "Building main package...\n";
  system("nmake /nologo > make.out 2>&1") == 0 || die "Build error (see make.out)";

  if ($perl eq "enabled") {
    if ($linktype eq "static") {
      print "Running Configure for DLL...\n";
      system("perl Configure $configOpts --linktype=dynamic --prefix=\"$install_base\" > perlconfigure.out 2>&1") == 0 || die "Build error (see perlconfigure.out)";
      
      print "Cleaning libraries...\n";
      system("nmake /nologo libs_clean >> clean.out 2>&1") == 0 || die "Build error (see clean.out)";
      
      print "Building DLL libraries...\n";
      system("nmake /nologo libs > dll.out 2>&1") == 0 || die "Build error (see dll.out)";
    }
   
    print "Cleaning Perl....\n";
    system("nmake /nologo perl_clean >> clean.out 2>&1"); # If already cleaned, Makefile is gone so don't worry about errors!

    print "Building Perl modules...\n";
    system("nmake /nologo perl > perlmake.out 2>&1") == 0 || die "Build error (see perlmake.out)";

    print "Testing Perl modules...\n";
    system("nmake /nologo perl_test > perltest.out 2>&1"); # Don't die if all the tests don't pass..
    
    if ($perl_install eq "enabled") {
      print "Installing Perl modules...\n";
      system("nmake /nologo perl_install > perlinstall.out 2>&1") == 0 || die "Build error (see perlinstall.out)";
    }
      
    print "\nSee perltest.out for Perl test results\n";
  }

  print "\n";
  if ($install eq "enabled") {
    print "Installing main package...\n";
    system("nmake /nologo install > install.out 2>&1") == 0 || die "Build error (see install.out)";
  }
  else {
    print "Type nmake install to install the package to $install_base\n";
  }

  if ($install_devel eq "enabled") {
    print "Installing development files...\n";
    system("nmake /nologo install_devel > install_devel.out 2>&1") == 0 || die "Build error (see install_devel.out)";
  }
  else {
    print "Type nmake install_devel to install the development files to $install_base\n";
  }
  
  if ($perl_install eq "disabled" && $perl eq "enabled") {
    print "Type nmake perl_install to install the Perl modules\n";
  }
}
else {
  system("del *.out");

  # Delete net-snmp-config.h from main include folder just in case it was created by a Cygwin or MinGW build
  system("del ..\\include\\net-snmp\\net-snmp-config.h > NUL: 2>&1");

  system("perl Configure $configOpts --linktype=$linktype --prefix=\"$install_base\"") == 0 || die "Build error (see above)";
  system("nmake /nologo clean") == 0 || die "Build error (see above)";
  system("nmake /nologo") == 0 || die "Build error (see above)";
  
  if ($perl eq "enabled") {
    if ($linktype eq "static") {      
      system("perl Configure $configOpts --linktype=dynamic --prefix=\"$install_base\"") == 0 || die "Build error (see above)";
      system("nmake /nologo libs_clean") == 0 || die "Build error (see above)";
      system("nmake /nologo libs") == 0 || die "Build error (see above)";
    }
      
    system("nmake /nologo perl_clean"); # If already cleaned, Makefile is gone so don't worry about errors!
    system("nmake /nologo perl") == 0 || die "Build error (see above)";

    my $path_old = $ENV{PATH};
    $ENV{PATH} = "$current_pwd\\bin\\" . ($debug eq "enabled" ? "debug" : "release" ) . ";$ENV{PATH}";
    system("nmake /nologo perl_test"); # Don't die if all the tests don't pass..
    $ENV{PATH} = $path_old;
    
    if ($perl_install eq "enabled") {      
      print "Installing Perl modules...\n";
      system("nmake /nologo perl_install") == 0 || die "Build error (see above)";
    }
  }

  print "\n";
  if ($install eq "enabled") {
    print "Installing main package...\n";
    system("nmake /nologo install") == 0 || die "Build error (see above)";
  }
  else {
    print "Type nmake install to install the package to $install_base\n";
  }

  if ($install_devel eq "enabled") {
    print "Installing development files...\n";
    system("nmake /nologo install_devel > install_devel.out 2>&1") == 0 || die "Build error (see install_devel.out)";
  }
  else {
    print "Type nmake install_devel to install the development files to $install_base\n";
  }

  if ($perl_install eq "disabled" && $perl eq "enabled") {
    print "Type nmake perl_install to install the Perl modules\n";
  }
}

print "\nDone!\n";

