# Samba Build System
# - the input checking functions
#
#  Copyright (C) Stefan (metze) Metzmacher 2004
#  Copyright (C) Jelmer Vernooij 2004
#  Released under the GNU GPL

use strict;
package smb_build::input;
use File::Basename;

sub strtrim($)
{
	$_ = shift;
	s/^[\t\n ]*//g;
	s/[\t\n ]*$//g;
	return $_;
}

sub str2array($)
{
	$_ = shift;
	s/^[\t\n ]*//g;
	s/[\t\n ]*$//g;
	s/([\t\n ]+)/ /g;

	return () if (length($_)==0);
	return split /[ \t\n]/;
}

sub add_libreplace($)
{
	my ($part) = @_;

	return if ($part->{NAME} eq "LIBREPLACE");
	return if ($part->{NAME} eq "LIBREPLACE_HOSTCC");
	return if ($part->{NAME} eq "REPLACE_READDIR");

	foreach my $n (@{$part->{PRIVATE_DEPENDENCIES}}) {
		return if ($n eq "LIBREPLACE");
		return if ($n eq "LIBREPLACE_HOSTCC");
	}
	foreach my $n (@{$part->{PUBLIC_DEPENDENCIES}}) {
		return if ($n eq "LIBREPLACE");
		return if ($n eq "LIBREPLACE_HOSTCC");
	}

	if (defined($part->{USE_HOSTCC}) && $part->{USE_HOSTCC} eq "YES") {
		unshift (@{$part->{PRIVATE_DEPENDENCIES}}, "LIBREPLACE_HOSTCC");
	} else {
		unshift (@{$part->{PRIVATE_DEPENDENCIES}}, "LIBREPLACE");
	}
}

sub check_subsystem($$$)
{
	my ($INPUT, $subsys, $default_ot) = @_;
	return if ($subsys->{ENABLE} ne "YES");
	
	unless (defined($subsys->{OUTPUT_TYPE})) { $subsys->{OUTPUT_TYPE} = $default_ot; }
	unless (defined($subsys->{INIT_FUNCTION_TYPE})) { $subsys->{INIT_FUNCTION_TYPE} = "NTSTATUS (*) (void)"; }
	unless (defined($subsys->{INIT_FUNCTION_SENTINEL})) { $subsys->{INIT_FUNCTION_SENTINEL} = "NULL"; }
}

sub check_module($$$)
{
	my ($INPUT, $mod, $default_ot) = @_;

	die("Module $mod->{NAME} does not have a SUBSYSTEM set") if not defined($mod->{SUBSYSTEM});

	if (not exists($INPUT->{$mod->{SUBSYSTEM}}{INIT_FUNCTIONS})) {
		$INPUT->{$mod->{SUBSYSTEM}}{INIT_FUNCTIONS} = [];
	}

	if (!(defined($INPUT->{$mod->{SUBSYSTEM}}))) {
		die("Unknown subsystem $mod->{SUBSYSTEM} for module $mod->{NAME}");
	}

	if ($INPUT->{$mod->{SUBSYSTEM}} eq "NO") {
		warn("Disabling module $mod->{NAME} because subsystem $mod->{SUBSYSTEM} is disabled");
		$mod->{ENABLE} = "NO";
		return;
	}

	return if ($mod->{ENABLE} ne "YES");

	if (exists($INPUT->{$mod->{SUBSYSTEM}}{INIT_FUNCTION_TYPE})) {
		$mod->{INIT_FUNCTION_TYPE} = $INPUT->{$mod->{SUBSYSTEM}}{INIT_FUNCTION_TYPE};
	} else {
		$mod->{INIT_FUNCTION_TYPE} = "NTSTATUS (*) (void)";
	}

	unless (defined($mod->{INIT_FUNCTION_SENTINEL})) { $mod->{INIT_FUNCTION_SENTINEL} = "NULL"; }

	if (not defined($mod->{OUTPUT_TYPE})) {
		if ((not defined($INPUT->{$mod->{SUBSYSTEM}})) or
			(not defined($INPUT->{$mod->{SUBSYSTEM}}->{TYPE})) or 
			$INPUT->{$mod->{SUBSYSTEM}}->{TYPE} eq "EXT_LIB") {
			$mod->{OUTPUT_TYPE} = ["SHARED_LIBRARY"];
		} else {
			$mod->{OUTPUT_TYPE} = $default_ot;
		}
	}

	if (grep(/SHARED_LIBRARY/, @{$mod->{OUTPUT_TYPE}})) {
		my $sane_subsystem = lc($mod->{SUBSYSTEM});
		$sane_subsystem =~ s/^lib//;
		$mod->{INSTALLDIR} = "MODULESDIR/$sane_subsystem";
		push (@{$mod->{PUBLIC_DEPENDENCIES}}, $mod->{SUBSYSTEM});
		add_libreplace($mod);
	} 
	if (grep(/MERGED_OBJ/, @{$mod->{OUTPUT_TYPE}}) and $mod->{TYPE} ne "PYTHON") {
		push (@{$INPUT->{$mod->{SUBSYSTEM}}{INIT_FUNCTIONS}}, $mod->{INIT_FUNCTION}) if defined($mod->{INIT_FUNCTION});
		push (@{$INPUT->{$mod->{SUBSYSTEM}}{PRIVATE_DEPENDENCIES}}, $mod->{NAME});
	}
}

sub check_library($$$)
{
	my ($INPUT, $lib, $default_ot) = @_;

	return if ($lib->{ENABLE} ne "YES");

	unless (defined($lib->{OUTPUT_TYPE})) { $lib->{OUTPUT_TYPE} = $default_ot; }

	unless (defined($lib->{INIT_FUNCTION_TYPE})) { $lib->{INIT_FUNCTION_TYPE} = "NTSTATUS (*) (void)"; }
	unless (defined($lib->{INIT_FUNCTION_SENTINEL})) { $lib->{INIT_FUNCTION_SENTINEL} = "NULL"; }
	unless (defined($lib->{INSTALLDIR})) { $lib->{INSTALLDIR} = "LIBDIR"; }

	add_libreplace($lib);
}

sub check_python($$$)
{
	my ($INPUT, $python, $default_ot) = @_;

	return if ($INPUT->{LIBPYTHON}{ENABLE} ne "YES");

	$python->{INSTALLDIR} = "PYTHONDIR";
	unless (defined($python->{CFLAGS})) { $python->{CFLAGS} = []; }
	my $basename = $python->{NAME};
	$basename =~ s/^python_//g;
	unless (defined($python->{LIBRARY_REALNAME})) {
		$python->{LIBRARY_REALNAME} = "$basename.\$(SHLIBEXT)";
	}
	$python->{INIT_FUNCTION} = "{ (char *)\"$basename\", init$basename }";
	push (@{$python->{CFLAGS}}, "\$(EXT_LIB_PYTHON_CFLAGS)");

	$python->{SUBSYSTEM} = "LIBPYTHON";

	check_module($INPUT, $python, $default_ot);
}

sub check_binary($$)
{
	my ($INPUT, $bin) = @_;

	return if ($bin->{ENABLE} ne "YES");

	($bin->{BINARY} = (lc $bin->{NAME})) if not defined($bin->{BINARY});
	unless (defined($bin->{INIT_FUNCTION_SENTINEL})) { $bin->{INIT_FUNCTION_SENTINEL} = "NULL"; }
	unless (defined($bin->{INIT_FUNCTION_TYPE})) { $bin->{INIT_FUNCTION_TYPE} = "NTSTATUS (*) (void)"; }

	$bin->{OUTPUT_TYPE} = ["BINARY"];
	add_libreplace($bin);
}

sub add_implicit($$)
{
	my ($INPUT, $n) = @_;

	$INPUT->{$n}->{TYPE} = "MAKE_RULE";
	$INPUT->{$n}->{NAME} = $n;
	$INPUT->{$n}->{OUTPUT_TYPE} = undef;
	$INPUT->{$n}->{LIBS} = ["\$(".uc($n)."_LIBS)"];
	$INPUT->{$n}->{LDFLAGS} = ["\$(".uc($n)."_LDFLAGS)"];
	$INPUT->{$n}->{CFLAGS} = ["\$(".uc($n)."_CFLAGS)"];
	$INPUT->{$n}->{CPPFLAGS} = ["\$(".uc($n)."_CPPFLAGS)"];
	$INPUT->{$n}->{ENABLE} = "YES";
}

sub calc_unique_deps($$$$$$$$)
{
	sub calc_unique_deps($$$$$$$$);
	my ($name, $INPUT, $deps, $udeps, $withlibs, $forward, $pubonly, $busy) = @_;

	foreach my $n (@$deps) {
		add_implicit($INPUT, $n) unless (defined($INPUT->{$n}) and defined($INPUT->{$n}->{TYPE}));
		my $dep = $INPUT->{$n};
		if (grep (/^$n$/, @$busy)) {
			next if (@{$dep->{OUTPUT_TYPE}}[0] eq "MERGED_OBJ");
			die("Recursive dependency: $n, list: " . join(',', @$busy));
		}
		next if (grep /^$n$/, @$udeps);

		push (@{$udeps}, $n) if $forward;

 		if (defined ($dep->{OUTPUT_TYPE}) && 
			($withlibs or 
			(@{$dep->{OUTPUT_TYPE}}[0] eq "MERGED_OBJ") or 
			(@{$dep->{OUTPUT_TYPE}}[0] eq "STATIC_LIBRARY"))) {
				push (@$busy, $n);
			        calc_unique_deps($n, $INPUT, $dep->{PUBLIC_DEPENDENCIES}, $udeps, $withlibs, $forward, $pubonly, $busy);
			        calc_unique_deps($n, $INPUT, $dep->{PRIVATE_DEPENDENCIES}, $udeps, $withlibs, $forward, $pubonly, $busy) unless $pubonly;
				pop (@$busy);
	        }

		unshift (@{$udeps}, $n) unless $forward;
	}
}

sub check($$$$$)
{
	my ($INPUT, $enabled, $subsys_ot, $lib_ot, $module_ot) = @_;

	foreach my $part (values %$INPUT) {
		if (defined($enabled->{$part->{NAME}})) { 
			$part->{ENABLE} = $enabled->{$part->{NAME}};
			next;
		}
		
		unless(defined($part->{ENABLE})) {
			if ($part->{TYPE} eq "EXT_LIB") {
				$part->{ENABLE} = "NO";
			} else {
				$part->{ENABLE} = "YES";
			}
		}
	}

	foreach my $part (values %$INPUT) {
		$part->{LINK_FLAGS} = [];
		$part->{FULL_OBJ_LIST} = ["\$($part->{NAME}_OBJ_FILES)"];

		if ($part->{TYPE} eq "SUBSYSTEM") { 
			check_subsystem($INPUT, $part, $subsys_ot);
		} elsif ($part->{TYPE} eq "MODULE") {
			check_module($INPUT, $part, $module_ot);
		} elsif ($part->{TYPE} eq "LIBRARY") {
			check_library($INPUT, $part, $lib_ot);
		} elsif ($part->{TYPE} eq "BINARY") {
			check_binary($INPUT, $part);
		} elsif ($part->{TYPE} eq "PYTHON") {
			check_python($INPUT, $part, $module_ot);
		} elsif ($part->{TYPE} eq "EXT_LIB") {
		} else {
			die("Unknown type $part->{TYPE}");
		}
	}

	foreach my $part (values %$INPUT) {
		if (defined($part->{INIT_FUNCTIONS})) {
			push (@{$part->{LINK_FLAGS}}, "\$(DYNEXP)");
		}
	}

	foreach my $part (values %$INPUT) {
		$part->{UNIQUE_DEPENDENCIES_LINK} = [];
		calc_unique_deps($part->{NAME}, $INPUT, $part->{PUBLIC_DEPENDENCIES}, $part->{UNIQUE_DEPENDENCIES_LINK}, 0, 0, 0, []);
		calc_unique_deps($part->{NAME}, $INPUT, $part->{PRIVATE_DEPENDENCIES}, $part->{UNIQUE_DEPENDENCIES_LINK}, 0, 0, 0, []);
	}

	foreach my $part (values %$INPUT) {
		$part->{UNIQUE_DEPENDENCIES_COMPILE} = [];
		calc_unique_deps($part->{NAME}, $INPUT, $part->{PUBLIC_DEPENDENCIES}, $part->{UNIQUE_DEPENDENCIES_COMPILE}, 1, 1, 1, []);
		calc_unique_deps($part->{NAME}, $INPUT, $part->{PRIVATE_DEPENDENCIES}, $part->{UNIQUE_DEPENDENCIES_COMPILE}, 1, 1, 1, []);
	}

	foreach my $part (values %$INPUT) {
		$part->{UNIQUE_DEPENDENCIES_ALL} = [];
		calc_unique_deps($part->{NAME}, $INPUT, $part->{PUBLIC_DEPENDENCIES}, $part->{UNIQUE_DEPENDENCIES_ALL}, 1, 0, 0, []);
		calc_unique_deps($part->{NAME}, $INPUT, $part->{PRIVATE_DEPENDENCIES}, $part->{UNIQUE_DEPENDENCIES_ALL}, 1, 0, 0, []);
	}

	return $INPUT;
}

1;
