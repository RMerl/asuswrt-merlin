# Samba Build System					
# - the main program					
#							
#  Copyright (C) Stefan (metze) Metzmacher 2004	
#  Copyright (C) Jelmer Vernooij 2005
#  Released under the GNU GPL				

use smb_build::makefile;
use smb_build::input;
use smb_build::config_mk;
use smb_build::output;
use smb_build::summary;
use smb_build::config;
use Getopt::Long;
use strict;

my $output_file = "data.mk";

my $result = GetOptions (
	'output=s' => \$output_file);

if (not $result) {
	exit(1);
}

my $input_file = shift @ARGV;

my $INPUT = {};
my $mkfile = smb_build::config_mk::run_config_mk($INPUT, $config::config{srcdir}, $config::config{builddir}, $input_file);

my $subsys_output_type = ["MERGED_OBJ"];

my $library_output_type;
my $useshared = (defined($ENV{USESHARED})?$ENV{USESHARED}:$config::config{USESHARED});

if ($useshared eq "true") {
	$library_output_type = ["SHARED_LIBRARY", "MERGED_OBJ"];
} else {
	$library_output_type = ["MERGED_OBJ"];
	push (@$library_output_type, "SHARED_LIBRARY") if 
						($config::config{BLDSHARED} eq "true")
}

my $module_output_type;
if ($useshared eq "true") {
	#$module_output_type = ["SHARED_LIBRARY"];
	$module_output_type = ["MERGED_OBJ"];
} else {
	$module_output_type = ["MERGED_OBJ"];
}

my $DEPEND = smb_build::input::check($INPUT, \%config::enabled,
				     $subsys_output_type,
				     $library_output_type,
				     $module_output_type);
my $OUTPUT = output::create_output($DEPEND, \%config::config);
my $mkenv = new smb_build::makefile(\%config::config, $mkfile);

my $shared_libs_used = 0;
foreach my $key (values %$OUTPUT) {
	next if ($key->{ENABLE} ne "YES");
	push(@{$mkenv->{all_objs}}, "\$($key->{NAME}_OBJ_FILES)");
}

foreach my $key (values %$OUTPUT) {
	next unless defined $key->{OUTPUT_TYPE};

	$mkenv->StaticLibraryPrimitives($key) if grep(/STATIC_LIBRARY/, @{$key->{OUTPUT_TYPE}});
	$mkenv->MergedObj($key) if grep(/MERGED_OBJ/, @{$key->{OUTPUT_TYPE}});
	$mkenv->SharedLibraryPrimitives($key) if ($key->{TYPE} eq "LIBRARY") and
					grep(/SHARED_LIBRARY/, @{$key->{OUTPUT_TYPE}});
	if ($key->{TYPE} eq "LIBRARY" and 
	    ${$key->{OUTPUT_TYPE}}[0] eq "SHARED_LIBRARY") {
		$shared_libs_used = 1;
	}
	if ($key->{TYPE} eq "MODULE" and @{$key->{OUTPUT_TYPE}}[0] eq "MERGED_OBJ" and defined($key->{INIT_FUNCTION})) {
		$mkenv->output("$key->{SUBSYSTEM}_INIT_FUNCTIONS +=$key->{INIT_FUNCTION},\n");
	}
	$mkenv->CFlags($key);
}

foreach my $key (values %$OUTPUT) {
	next unless defined $key->{OUTPUT_TYPE};

 	$mkenv->Integrated($key) if grep(/INTEGRATED/, @{$key->{OUTPUT_TYPE}});
}

foreach my $key (values %$OUTPUT) {
	next unless defined $key->{OUTPUT_TYPE};
	$mkenv->StaticLibrary($key) if grep(/STATIC_LIBRARY/, @{$key->{OUTPUT_TYPE}});

	$mkenv->SharedLibrary($key) if ($key->{TYPE} eq "LIBRARY") and
					grep(/SHARED_LIBRARY/, @{$key->{OUTPUT_TYPE}});
	$mkenv->SharedModule($key) if ($key->{TYPE} eq "MODULE" and
					grep(/SHARED_LIBRARY/, @{$key->{OUTPUT_TYPE}}));
	$mkenv->PythonModule($key) if ($key->{TYPE} eq "PYTHON");
	$mkenv->Binary($key) if grep(/BINARY/, @{$key->{OUTPUT_TYPE}});
	$mkenv->InitFunctions($key) if defined($key->{INIT_FUNCTIONS});
}

$mkenv->write($output_file);

summary::show($OUTPUT, \%config::config);

1;
