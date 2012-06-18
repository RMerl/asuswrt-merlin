# SMB Build System
# - the output generating functions
#
#  Copyright (C) Stefan (metze) Metzmacher 2004
#  Copyright (C) Jelmer Vernooij 2004
#  Released under the GNU GPL

package output;
use strict;

sub generate_shared_library($)
{
	my $lib = shift;
	my $link_name;
	my $lib_name;

	$lib->{DEPEND_LIST} = [];

	$link_name = lc($lib->{NAME});
	$lib_name = $link_name;

	if ($lib->{TYPE} eq "LIBRARY") {
		$link_name = $lib->{NAME};
		$link_name =~ s/^LIB//;
		$link_name = lc($link_name);
		$lib_name = "lib$link_name";
	}

	if ($lib->{TYPE} eq "PYTHON") {
		$lib->{SHAREDDIR} = "bin/python";
	} elsif (defined($lib->{LIBRARY_REALNAME})) {
		$lib->{BASEDIR} =~ s/^\.\///g;
		$lib->{SHAREDDIR} = $lib->{BASEDIR};
	} else {
		if ($lib->{TYPE} eq "MODULE") {
			my $sane_subsystem = lc($lib->{SUBSYSTEM});
			$sane_subsystem =~ s/^lib//;
			$lib->{SHAREDDIR} = "bin/modules/$sane_subsystem";
			$lib->{LIBRARY_REALNAME} = $link_name;
			$lib->{LIBRARY_REALNAME} =~ s/^$sane_subsystem\_//g;
			$lib->{LIBRARY_REALNAME}.= ".\$(SHLIBEXT)";
		} else {
			$lib->{SHAREDDIR} = "\$(shliboutputdir)";
			$lib->{LIBRARY_REALNAME} = "$lib_name.\$(SHLIBEXT)";
		}
	}

	$lib->{LIBRARY_DEBUGNAME} = $lib->{LIBRARY_REALNAME};

	$lib->{LIBRARY_SONAME} = "\$(if \$($lib->{NAME}_SOVERSION),$lib->{LIBRARY_REALNAME}.\$($lib->{NAME}_SOVERSION),$lib->{LIBRARY_REALNAME})";
	$lib->{LIBRARY_REALNAME} = "\$(if \$($lib->{NAME}_VERSION),$lib->{LIBRARY_REALNAME}.\$($lib->{NAME}_VERSION),$lib->{LIBRARY_REALNAME})";
	
	$lib->{RESULT_SHARED_LIBRARY} = "$lib->{SHAREDDIR}/$lib->{LIBRARY_REALNAME}";
	$lib->{OUTPUT_SHARED_LIBRARY} = "-l$link_name";
	$lib->{TARGET_SHARED_LIBRARY} = "$lib->{SHAREDDIR}/$lib->{LIBRARY_DEBUGNAME}";
}

sub generate_merged_obj($)
{
	my $lib = shift;

	my $link_name = $lib->{NAME};
	$link_name =~ s/^LIB//;

	$lib->{MERGED_OBJNAME} = lc($link_name).".o";
	$lib->{RESULT_MERGED_OBJ} = $lib->{OUTPUT_MERGED_OBJ} = "bin/mergedobj/$lib->{MERGED_OBJNAME}";
	$lib->{TARGET_MERGED_OBJ} = $lib->{RESULT_MERGED_OBJ};
}

sub generate_static_library($)
{
	my $lib = shift;
	my $link_name;

	$lib->{DEPEND_LIST} = [];

	$link_name = $lib->{NAME};
	$link_name =~ s/^LIB//;

	$lib->{LIBRARY_NAME} = "lib".lc($link_name).".a";

	$lib->{RESULT_STATIC_LIBRARY} = "bin/static/$lib->{LIBRARY_NAME}";
	$lib->{TARGET_STATIC_LIBRARY} = $lib->{RESULT_STATIC_LIBRARY};
	$lib->{STATICDIR} = 'bin/static';
	$lib->{OUTPUT_STATIC_LIBRARY} = "-l".lc($link_name);
}

sub generate_binary($)
{
	my $bin = shift;

	$bin->{DEPEND_LIST} = [];
	push(@{$bin->{LINK_FLAGS}}, "\$($bin->{NAME}\_FULL_OBJ_LIST)");

	$bin->{DEBUGDIR} = "bin";
	$bin->{RESULT_BINARY} = $bin->{OUTPUT_BINARY} = "$bin->{DEBUGDIR}/$bin->{NAME}";
	$bin->{TARGET_BINARY} = $bin->{RESULT_BINARY};
	$bin->{BINARY} = $bin->{NAME};
}

sub merge_array($$)
{
	# $dest is a reference to an array
	# $src is an array
	my ($dest, $src) = @_;

	return unless defined($src);
	return unless ($#{$src} >= 0);

	foreach my $line (@{$src}) {
		next if (grep /^$line$/, @{$$dest});
		push(@{$$dest}, $line);
	}
}

sub create_output($$)
{
	my ($depend, $config) = @_;
	my $part;

	foreach $part (values %{$depend}) {
		next unless(defined($part->{OUTPUT_TYPE}));

		generate_binary($part) if grep(/BINARY/, @{$part->{OUTPUT_TYPE}});
		generate_shared_library($part) if grep(/SHARED_LIBRARY/, @{$part->{OUTPUT_TYPE}});
		generate_static_library($part) if grep(/STATIC_LIBRARY/, @{$part->{OUTPUT_TYPE}});
		generate_merged_obj($part) if grep(/MERGED_OBJ/, @{$part->{OUTPUT_TYPE}});
		$part->{OUTPUT} = $part->{"OUTPUT_" . @{$part->{OUTPUT_TYPE}}[0]};
		$part->{TARGET} = $part->{"TARGET_" . @{$part->{OUTPUT_TYPE}}[0]};
	}

	foreach $part (values %{$depend}) {
		next if not defined($part->{OUTPUT_TYPE});

		merge_array(\$part->{FINAL_CFLAGS}, $part->{CPPFLAGS});
		merge_array(\$part->{FINAL_CFLAGS}, $part->{CFLAGS});

		foreach (@{$part->{UNIQUE_DEPENDENCIES_ALL}}) {
			my $elem = $depend->{$_};
			next if $elem == $part;

			merge_array(\$part->{FINAL_CFLAGS}, $elem->{CPPFLAGS});
			merge_array(\$part->{FINAL_CFLAGS}, $elem->{CFLAGS});
		}

		# Always import the link options of the unique dependencies
		foreach (@{$part->{UNIQUE_DEPENDENCIES_LINK}}) {
			my $elem = $depend->{$_};
			next if $elem == $part;

			push(@{$part->{LINK_FLAGS}}, @{$elem->{LIBS}}) if defined($elem->{LIBS});
			push(@{$part->{LINK_FLAGS}}, @{$elem->{LDFLAGS}}) if defined($elem->{LDFLAGS});
			if (defined($elem->{OUTPUT_TYPE}) and @{$elem->{OUTPUT_TYPE}}[0] eq "MERGED_OBJ") {
				push (@{$part->{FULL_OBJ_LIST}}, $elem->{TARGET});
			} else {
				push(@{$part->{LINK_FLAGS}}, "\$($elem->{NAME}_OUTPUT)") if defined($elem->{OUTPUT});
				push(@{$part->{DEPEND_LIST}}, $elem->{TARGET}) if (defined($elem->{TARGET}));
			}
		}
	}

	foreach $part (values %{$depend}) {
		if (defined($part->{STANDARD_VISIBILITY}) and ($part->{STANDARD_VISIBILITY} ne "default") and 
			($config->{visibility_attribute} eq "yes")) {
		    	push(@{$part->{FINAL_CFLAGS}}, "-fvisibility=$part->{STANDARD_VISIBILITY}");
		}
	}

	return $depend;
}

1;
