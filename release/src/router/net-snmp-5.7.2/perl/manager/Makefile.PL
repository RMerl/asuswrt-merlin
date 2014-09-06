use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'		=> 'NetSNMP::manager',
    'VERSION'	        => '', # finds $VERSION
    'PREREQ_PM'		=> {# SNMP => 4.0,
			    DBI => 0,
			    CGI => 0}, # e.g., Module::Name => 1.1
    'PM'                => { 
	'displaytable.pm' => '$(INST_LIBDIR)/../displaytable.pm',
	'getValues.pm' => '$(INST_LIBDIR)/../getValues.pm',
	'manager.pm' => '$(INST_LIBDIR)/manager.pm'
    },
    'EXE_FILES'         => [qw(snmptosql setupdb setupuser setupauth)]
);
