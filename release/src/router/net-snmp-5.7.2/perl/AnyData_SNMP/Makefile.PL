use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'		=> 'AnyData::SNMP',
    'VERSION_FROM'	=> 'Storage.pm', # finds $VERSION
    'PREREQ_PM'		=> {# SNMP => 5.0,
			    AnyData => 0}, # e.g., Module::Name => 1.1
    'PM'                => { 
	'Format.pm' => '$(INST_LIBDIR)/Format/SNMP.pm',
	'Storage.pm' => '$(INST_LIBDIR)/Storage/SNMP.pm',
    },
    'EXE_FILES'         => [qw(netsh)]
);
