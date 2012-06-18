# Samba Build System
# - config.mk parsing functions
#
#  Copyright (C) Stefan (metze) Metzmacher 2004
#  Copyright (C) Jelmer Vernooij 2005
#  Released under the GNU GPL
#

package smb_build::config_mk;
use smb_build::input;
use File::Basename;

use strict;

my $section_types = {
	"EXT_LIB" => {
		"LIBS"			=> "list",
		"CFLAGS"		=> "list",
		"CPPFLAGS"		=> "list",
		"LDFLAGS"		=> "list",
		},
	"PYTHON" => {
		"LIBRARY_REALNAME" => "string",
		"PRIVATE_DEPENDENCIES"	=> "list",
		"PUBLIC_DEPENDENCIES"	=> "list",
		"ENABLE"		=> "bool",
		"LDFLAGS"		=> "list",
	},
	"SUBSYSTEM" => {
		"PRIVATE_DEPENDENCIES"	=> "list",
		"PUBLIC_DEPENDENCIES"	=> "list",

		"ENABLE"		=> "bool",

		"CFLAGS"		=> "list",
		"LDFLAGS"		=> "list",
		"STANDARD_VISIBILITY"	=> "string",
		"INIT_FUNCTION_SENTINEL" => "string"
		},
	"MODULE" => {
		"SUBSYSTEM"		=> "string",

		"INIT_FUNCTION"		=> "string",

		"PRIVATE_DEPENDENCIES"	=> "list",

		"ALIASES" => "list",

		"ENABLE"		=> "bool",

		"OUTPUT_TYPE"		=> "list",

		"CFLAGS"		=> "list"
		},
	"BINARY" => {

		"PRIVATE_DEPENDENCIES"	=> "list",

		"ENABLE"		=> "bool",

		"INSTALLDIR"		=> "string",
		"LDFLAGS"		=> "list",
		"STANDARD_VISIBILITY"	=> "string",

		"USE_HOSTCC"		=> "bool"
		},
	"LIBRARY" => {
		"LIBRARY_REALNAME" => "string",

		"INIT_FUNCTION_TYPE"	=> "string",
		"INIT_FUNCTION_SENTINEL" => "string",
		"OUTPUT_TYPE"		=> "list",

		"PRIVATE_DEPENDENCIES"	=> "list",
		"PUBLIC_DEPENDENCIES"	=> "list",

		"ENABLE"		=> "bool",

		"CFLAGS"		=> "list",
		"LDFLAGS"		=> "list",
		"STANDARD_VISIBILITY"	=> "string"
		}
};

use vars qw(@parsed_files);

@parsed_files = ();

sub _read_config_file($$$)
{
	use Cwd;

	my ($srcdir, $builddir, $filename) = @_;
	my @dirlist;

	# We need to change our working directory because config.mk files can
	# give shell commands as the argument to "include". These shell
	# commands can take arguments that are relative paths and we don't have
	# a way of sensibly rewriting these.
	my $cwd = getcwd;
	chomp $cwd;

	if ($srcdir ne $builddir) {
		# Push the builddir path on the front, so we prefer builddir
		# to srcdir when the file exists in both.
		@dirlist = ($builddir, $srcdir);
	} else {
		@dirlist = ($srcdir);
	}

	foreach my $d (@dirlist) {
		my @lines;
		my $basedir;

		chdir $cwd;
		chdir $d;

		# We need to catch the exception from open in the case where
		# the filename is actually a shell pipeline. Why is this
		# different to opening a regular file? Because this is perl!
		eval {
			open(CONFIG_MK, "./$filename");
			@lines = <CONFIG_MK>;
			close(CONFIG_MK);
		};

		chdir $cwd;
		next unless (@lines);

		# I blame abartlett for this crazy hack -- jpeach
		if ($filename =~ /\|$/) {
			$basedir = $builddir;
		} else {
			$basedir = dirname($filename);
			push(@parsed_files, $filename);
		}
		$basedir =~ s!^($builddir|$srcdir)[/]!!;
		return ($filename, $basedir, @lines);
	}

	chdir $cwd;
	return;
}

###########################################################
# The parsing function which parses the file
#
# $result = _parse_config_mk($input, $srcdir, $builddir, $filename)
#
# $filename -	the path of the config.mk file
#		which should be parsed
sub run_config_mk($$$$)
{
	sub run_config_mk($$$$);
	my ($input, $srcdir, $builddir, $filename) = @_;
	my $result;
	my $linenum = -1;
	my $infragment = 0;
	my $section = "GLOBAL";
	my $makefile = "";

	my $basedir;

	my $parsing_file;
	my @lines;

	$ENV{builddir} = $builddir;
	$ENV{srcdir} = $srcdir;

	($parsing_file, $basedir, @lines) =
	    _read_config_file($srcdir, $builddir, $filename);

	die ("$0: can't open '$filename'")
		unless ($parsing_file and $basedir and @lines);

	my $line = "";
	my $prev = "";

	# Emit a line that lets us match up final makefile output with the
	# corresponding input files. The curlies are so you can match the
	# BEGIN/END pairs in a text editor.
	$makefile .= "# BEGIN{ $parsing_file\n";

	foreach (@lines) {
		$linenum++;

		# lines beginning with '#' are ignored
		next if (/^\#.*$/);
		
		if (/^(.*)\\$/) {
			$prev .= $1;
			next;
		} else {
			$line = "$prev$_";
			$prev = "";
		}

		if ($line =~ /^\[([-a-zA-Z0-9_.:]+)\][\t ]*$/) 
		{
			$section = $1;
			$infragment = 0;

			$result->{$section}{EXISTS}{KEY} = "EXISTS";
			$result->{$section}{EXISTS}{VAL} = 1;
			next;
		}

		# include
		if ($line =~ /^mkinclude (.*)$/) {
			my $subfile= $1;
			my $subdir = dirname($filename);
			$subdir =~ s/^\.$//g;
			$subdir =~ s/^\.\///g;
			$subdir .= "/" if ($subdir ne "");
			$makefile .= "basedir := $subdir\n";
			$makefile .= run_config_mk($input, $srcdir, $builddir, $subdir.$subfile);
			next;
		}

		# empty line
		if ($line =~ /^[ \t]*$/) {
			$section = "GLOBAL";
			if ($infragment) { $makefile.="\n"; }
			next;
		}

		# global stuff is considered part of the makefile
		if ($section eq "GLOBAL") {
			if (!$infragment) { $makefile.="\n"; }
			$makefile .= $line;
			$infragment = 1;
			next;
		}
		
		# Assignment
		if ($line =~ /^([a-zA-Z0-9_]+)[\t ]*=(.*)$/) {
			$result->{$section}{$1}{VAL} = $2;
			$result->{$section}{$1}{KEY} = $1;
		
			next;
		}

		die("$parsing_file:$linenum: Bad line");
	}

	$makefile .= "# }END $parsing_file\n";

	foreach my $section (keys %{$result}) {
		my ($type, $name) = split(/::/, $section, 2);

		my $sectype = $section_types->{$type};
		if (not defined($sectype)) {
			die($parsing_file.":[".$section."] unknown section type \"".$type."\"!");
		}

		$input->{$name}{NAME} = $name;
		$input->{$name}{TYPE} = $type;
		$input->{$name}{MK_FILE} = $parsing_file;
		$input->{$name}{BASEDIR} = $basedir;

		foreach my $key (values %{$result->{$section}}) {
			next if ($key->{KEY} eq "EXISTS");
			$key->{VAL} = smb_build::input::strtrim($key->{VAL});
			my $vartype = $sectype->{$key->{KEY}};
			if (not defined($vartype)) {
				die($parsing_file.":[".$section."]: unknown attribute type \"$key->{KEY}\"!");
			}
			if ($vartype eq "string") {
				$input->{$name}{$key->{KEY}} = $key->{VAL};
			} elsif ($vartype eq "list") {
				$input->{$name}{$key->{KEY}} = [smb_build::input::str2array($key->{VAL})];
			} elsif ($vartype eq "bool") {
				if (($key->{VAL} ne "YES") and ($key->{VAL} ne "NO")) {
					die("Invalid value for bool attribute $key->{KEY}: $key->{VAL} in section $section");
				}
				$input->{$name}{$key->{KEY}} = $key->{VAL};
			}
		}
	}

	return $makefile;
}

1;
