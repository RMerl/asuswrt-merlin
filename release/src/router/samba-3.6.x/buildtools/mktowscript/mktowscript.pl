#!/usr/bin/perl -w

use strict;
use Data::Dumper;
use File::Basename;
use List::MoreUtils qw(uniq);

my $globals;
my $dname;

sub read_file($)
{
	my $filename = shift;
	open(CONFIG_MK, "$filename");
	my @lines = <CONFIG_MK>;
	close(CONFIG_MK);
	return @lines;
}

sub trim($)
{
	my $string = shift;
	$string =~ s/^\s+//;
	$string =~ s/\s+$//;
	return $string;
}

sub strlist($)
{
	my $s = shift;
	$s =~ s/\$\(SHLIBEXT\)/so/g;
	$s =~ s/\$\(heimdalsrcdir\)/..\/heimdal/g;
	$s =~ s/\$\(heimdalbuildsrcdir\)/..\/heimdal_build/g;
	$s =~ s/\$\(nsswitchsrcdir\)/..\/nsswitch/g;
	$s =~ s/\$\(param_OBJ_FILES\)/..\/pyparam.c/g;
	$s =~ s/\$\(libclisrcdir\)\///g;
	$s =~ s/\$\(socketwrappersrcdir\)\///g;
	$s =~ s/\$\(libcompressionsrcdir\)\///g;
	$s =~ s/\$\(\w*srcdir\)\///g;
	$s =~ s/\$\(libgpodir\)\///g;
	$s =~ s/\:\.o=\.ho//g;
	$s =~ s/\:\.o=\.d//g;

	# this one doesn't exist?
	$s =~ s/\bLDAP_ENCODE\b//g;

	# these need to use the library names
	$s =~ s/\bLIBLDB\b/ldb/g;
	$s =~ s/\bLDB\b/ldb/g;
	$s =~ s/\bLIBTALLOC\b/talloc/g;
	$s =~ s/\bTALLOC\b/talloc/g;
	$s =~ s/\bLIBTEVENT\b/tevent/g;
	$s =~ s/\bTEVENT\b/tevent/g;
	$s =~ s/\bTSOCKET\b/LIBTSOCKET/g;
	$s =~ s/\bGENSEC\b/gensec/g;
	$s =~ s/\bLIBTDB\b/tdb/g;
	$s =~ s/\bRESOLV\b/resolv/g;

	return trim(join(' ', split(/\s+/, $s)));
}

sub expand_vars($$)
{
	my $vars = shift;
	my $s = shift;
	foreach my $v (keys %{$vars}) {
		if ($s =~ /\$\($v\)/) {
			$s =~ s/\$\($v\)/$vars->{$v}/g;
			delete($vars->{$v});
		}
	}
	foreach my $v (keys %{$globals}) {
		if ($s =~ /\$\($v\)/) {
			$s =~ s/\$\($v\)/$globals->{$v}/g;
		}
	}
	return $s;
}

sub find_file($)
{
	my $f = shift;
	my $orig = $f;

	if ($f =~ /\$/) {
		printf(STDERR "bad variable expansion for file $orig in $dname\n");
		exit(1);
	}

	my $b = basename($f);
	return $b if (-e $b);

	return $f if (-e $f);

	while ($f =~ /\//) {
		$f =~ s/^[^\/]+\///g;
		return $f if (-e $f);
	}
	my $f2;
	$f2 = `find . -name "$f" -type f`;
	return $f2 unless ($f2 eq "");
	$f2 = `find .. -name "$f" -type f`;
	return $f2 unless ($f2 eq "");
	$f2 = `find ../.. -name "$f" -type f`;
	return $f2 unless ($f2 eq "");
	$f2 = `find ../../.. -name "$f" -type f`;
	return $f2 unless ($f2 eq "");
	printf(STDERR "Failed to find $orig in $dname\n");
	exit(1);
	return '';
}

sub find_files($)
{
	my $list = shift;
	my $ret = '';
	foreach my $f (split(/\s+/, $list)) {
		if ($f =~ /\.[0-9]$/) {
			# a man page
			my $m = find_file($f . ".xml");
			die("Unable to find man page $f\n") if ($m eq "");
			$m =~ s/\.xml$//;
			return $m;
		}
		$f = find_file($f);
		$f =~ s/^[.]\///;
		$ret .= ' ' . $f;
	}
	$ret = strlist($ret);
	my @list = split(/\s+/, $ret);
	@list = uniq(@list);
	$ret = trim(join(' ', @list));
	return $ret;
}

sub read_config_mk($)
{
	my $filename = shift;
	my @lines = read_file($filename);
	my $prev = "";
	my $linenum = 1;
	my $section = "GLOBAL";
	my $infragment;
	my $result;
	my $line = "";
	my $secnumber = 1;

	$result->{"GLOBAL"}->{SECNUMBER} = $secnumber++;

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

		if ($line =~ /^mkinclude.*asn1_deps.pl\s+([^\s]+)\s+([^\s]+)\s+\\\$\\\(\w+\\\)\/([^\s|]+)\s*([^|]*)\|$/) {
			my $src = $1;
			$section = $2;
			my $dir = $3;
			my $options = $4;
			$section = "HEIMDAL_" . uc($section);
			$result->{$section}->{TYPE} = 'ASN1';
			$result->{$section}->{SECNUMBER} = $secnumber++;
			if ($options ne '') {
				$result->{$section}->{OPTIONS} = $options;
			}
			$result->{$section}->{DIRECTORY} = $dir;
			$result->{$section}->{$section . '_OBJ_FILES'} = $src;
			next;
		}

		if ($line =~ /^mkinclude.*et_deps.pl\s+([^\s]+)\s+\\\$\\\(\w+\\\)\/([^\s|]+)\|$/) {
			my $src = $1;
			my $dir = $2;
			$section = basename($src);
			$section =~ s/\./_/g;
			$section = "HEIMDAL_" . uc($section);
			$result->{$section}->{TYPE} = 'ERRTABLE';
			$result->{$section}->{SECNUMBER} = $secnumber++;
			$result->{$section}->{DIRECTORY} = "$dir";
			$result->{$section}->{$section . '_OBJ_FILES'} = $src;
			next;
		}

		if ($line =~ /^\[(\w+)::([\w-]+)\]/)
		{
			my $type = $1;
			$section = $2;
			$infragment = 0;

			$result->{$section}->{TYPE} = $type;
			$result->{$section}->{SECNUMBER} = $secnumber++;
			next;
		}

		# include
		if ($line =~ /^mkinclude (.*)$/) {
			my $subfile = $1;
			$result->{$subfile}->{TYPE} = 'SUBCONFIG';
			$result->{$subfile}->{SECNUMBER} = $secnumber++;
			next;
		}

		# empty line
		if ($line =~ /^[ \t]*$/) {
			next;
		}

		# global stuff is considered part of the makefile
		if ($section eq "GLOBAL") {
			$infragment = 1;
			next;
		}

		# Assignment
		if ($line =~ /^([a-zA-Z0-9_-]+)[\t ]*=(.*)$/) {
			$result->{$section}->{$1} = expand_vars($result->{$section}, strlist($2));
			$globals->{$1} = $result->{$section}->{$1};
			next;
		}

		# +=
		if ($line =~ /^([a-zA-Z0-9_-]+)[\t ]*\+=(.*)$/) {
			if (!$result->{$section}->{$1}) {
				$result->{$section}->{$1}="";
			}
			$result->{$section}->{$1} .= " " . expand_vars($result->{$section}, strlist($2));
			$globals->{$1} = $result->{$section}->{$1};
			next;
		}

		if ($line =~ /\$\(eval.\$\(call.proto_header_template.*,(.*),.*/) {
			$result->{$section}->{AUTOPROTO} = $1;
		}
		if ($line =~ /^\$\(eval/) {
			# skip eval lines for now
			next;
		}

		printf(STDERR "$linenum: Bad line: $line");
	}

	return $result;
}


sub process_results($)
{
	my $result = shift;

	foreach my $s (sort {$result->{$a}->{SECNUMBER} <=> $result->{$b}->{SECNUMBER}} keys %{$result}) {
		next if ($s eq "GLOBAL");
		my $sec = $result->{$s};
		if ($sec->{TYPE} eq "SUBCONFIG") {
			my $d = dirname($s);
			next if ($d eq ".");
			printf "bld.BUILD_SUBDIR('%s')\n", dirname($s);
		} else {
			printf "\nbld.SAMBA_%s('%s'", $sec->{TYPE}, $s;
			my $trailer="";
			my $got_src = 0;
			my $got_private_deps = 0;

			foreach my $k (keys %{$sec}) {
				#print "key=$k\n";

				next if ($k eq "SECNUMBER");
				next if ($k eq "TYPE");
				if ($k eq "INIT_FUNCTION") {
					$trailer .= sprintf(",\n\tinit_function='%s'", trim($sec->{$k}));
					next;
				}
				if ($k eq "INIT_FUNCTION_SENTINEL") {
					$trailer .= sprintf(",\n\tinit_function_sentinal='%s'", trim($sec->{$k}));
					next;
				}
				if ($k eq "_PY_FILES" ||
				    $k eq "EPYDOC_OPTIONS" ||
				    $k eq "COV_TARGET" ||
				    $k eq "GCOV" ||
				    $k eq "PC_FILES" ||
				    $k eq "CONFIG4FILE" ||
				    $k eq "LMHOSTSFILE4") {
					$trailer .= sprintf(",\n\t# %s='%s'", $k, trim($sec->{$k}));
					next;
				}
				if ($k eq "SUBSYSTEM") {
					$trailer .= sprintf(",\n\tsubsystem='%s'", trim($sec->{$k}));
					next;
				}
				if ($k eq "PRIVATE_DEPENDENCIES") {
					$trailer .= sprintf(",\n\tdeps='%s'", strlist($sec->{$k}));
					$got_private_deps = 1;
					next;
				}
				if ($k eq "PUBLIC_DEPENDENCIES") {
					$trailer .= sprintf(",\n\tpublic_deps='%s'", strlist($sec->{$k}));
					next;
				}
				if ($k eq "ALIASES") {
					$trailer .= sprintf(",\n\taliases='%s'", strlist($sec->{$k}));
					next;
				}
				if ($k eq "CFLAGS") {
					$trailer .= sprintf(",\n\tcflags='%s'", strlist($sec->{$k}));
					next;
				}
				if ($k eq "OPTIONS") {
					$trailer .= sprintf(",\n\toptions='%s'", strlist($sec->{$k}));
					next;
				}
				if ($k eq "DIRECTORY") {
					$trailer .= sprintf(",\n\tdirectory='%s'", strlist($sec->{$k}));
					next;
				}
				if ($k eq "LDFLAGS") {
					$trailer .= sprintf(",\n\tldflags='%s'", strlist($sec->{$k}));
					next;
				}
				if ($k eq "INSTALLDIR") {
					$trailer .= sprintf(",\n\tinstalldir='%s'", strlist($sec->{$k}));
					next;
				}
				if ($k eq "ASN1C") {
					$trailer .= sprintf(",\n\tcompiler='%s'", strlist($sec->{$k}));
					next;
				}
				if ($k eq "ET_COMPILER") {
					$trailer .= sprintf(",\n\tcompiler='%s'", strlist($sec->{$k}));
					next;
				}
				if ($k eq "ENABLE") {
					my $v = strlist($sec->{$k});
					if ($v eq "NO") {
						$trailer .= sprintf(",\n\tenabled=False");
						next;
					}
					next if ($v eq "YES");
					die("Unknown ENABLE value $v in $s\n");
				}
				if ($k eq "USE_HOSTCC") {
					my $v = strlist($sec->{$k});
					if ($v eq "YES") {
						$trailer .= sprintf(",\n\tuse_hostcc=True");
						next;
					}
					next if ($v eq "NO");
					die("Unknown HOST_CC value $v in $s\n");
				}
				if ($k eq "$s" . "_VERSION") {
					$trailer .= sprintf(",\n\tvnum='%s'", strlist($sec->{$k}));
					next;
				}
				if ($k eq "$s" . "_SOVERSION") {
					next;
				}
				if ($k eq "LIBRARY_REALNAME") {
					$trailer .= sprintf(",\n\trealname='%s'", strlist($sec->{$k}));
					next;
				}
				if ($k eq "OUTPUT_TYPE") {
					$trailer .= sprintf(",\n\toutput_type='%s'", strlist($sec->{$k}));
					next;
				}
				if ($k eq "AUTOPROTO") {
					my $list = trim(find_files(strlist($sec->{$k})));
					$trailer .= sprintf(",\n\tautoproto='%s'", $list);
					next;
				}
				if ($k eq "PUBLIC_HEADERS") {
					my $list = trim(strlist($sec->{$k}));
					if ($list =~ /\$\(addprefix .*,(.*)\)(.*)$/) {
						$list = trim("$1 $2");
						$list = find_files($list);
					} else {
						$list = trim(find_files(strlist($sec->{$k})));
					}
					$trailer .= sprintf(",\n\tpublic_headers='%s'", $list);
					next;
				}
				if ($k eq "MANPAGES") {
					my $list = trim(find_files(strlist($sec->{$k})));
					$trailer .= sprintf(",\n\tmanpages='%s'", $list);
					next;
				}
				if ($k eq "$s" . "_OBJ_FILES") {
					my $list = trim(strlist($sec->{$k}));
					$list =~ s/\.o/.c/g;
					$list =~ s/\.ho/.c/g;
					if ($list =~ /\$\(addprefix .*,(.*)\)(.*)$/) {
						$list = trim("$1 $2");
						$list = find_files($list);
						$list = "'$list'";
					} elsif ($list =~ /\$\(addprefix \$\((\w+)\)(.*),(.*)\)(.*)$/) {
						my $src = trim($3);
						my $dir = "$1$2";
						$dir =~ s/\/$//;
						my $res = "bld.SUBDIR('$dir', '$src')";
						if ($4) {
							$res = "$res + '$4'";
						}
						$list = $res;
					} else {
						$list = find_files($list);
						$list="'$list'";
					}
					$list =~ s/\$\(\w+srcdir\)\///g;
					printf(",\n\tsource=%s", $list);
					$got_src = 1;
					next;
				}
				if ($k eq "HEIMDAL_GSSAPI_KRB5_OBJ_FILES" ||
				    $k eq "HEIMDAL_GSSAPI_SPNEGO_OBJ_FILES" ||
				    $k eq "HEIMDAL_HEIM_ASN1_DER_OBJ_FILES" ||
				    $k eq "HEIMDAL_HX509_OBJH_FILES" ||
				    $k eq "HEIMDAL_HX509_OBJG_FILES" ||
				    $k eq "HEIMDAL_ROKEN_OBJ_FILES"
				    ) {
					next;
				}
				die("Unknown keyword $k in $s\n");
			}
			die("No source list in $s\n") unless $got_src or $got_private_deps;
			if (! $got_src) {
				printf(",source=''\n\t");
			}
			printf("%s\n\t)\n\n", $trailer);
		}
	}
}

for (my $i=0; $i <= $#ARGV; $i++) {
	my $filename=$ARGV[$i];
	$dname=dirname($filename);
	my $result = read_config_mk($filename);
	if ($i != 0) {
		print "\n\n\n";
	}
	print "# AUTOGENERATED by mktowscript.pl from $filename\n# Please remove this notice if hand editing\n\n";
	die("Unable to chdir to $dname\n") unless chdir($dname);
	process_results($result);
}

