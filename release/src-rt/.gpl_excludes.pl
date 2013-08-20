#!/usr/bin/perl

sub append_gpl_excludes
{
	my $fexclude;
	my $uc_model;
	my $uc_modeldir;

	$uc_model=uc($_[0]);
	$uc_modeldir=$_[1];

	system("touch ./.gpl_excludes.sysdeps");
	open($fexclude, ">./.gpl_excludes.sysdeps");

	# exclude useless wireless binary 
	if (opendir(DIR, "./wl/sysdeps")) {
		while (my $file = readdir(DIR)) {
			if ( $file ne "$uc_model" && $file ne "." && $file ne ".." && $file ne "default" ) {
				if ( -l "./wl/sysdeps/$uc_model" ) {
					$linkto=readlink("./wl/sysdeps/$uc_model");
					if ( "${file}" ne "$linkto" ) {
						print "not link to $file $linkto\n";
						print $fexclude "${uc_modeldir}/wl/sysdeps/$file\n";
					}
				} 
				else 
				{
					print $fexclude "${uc_modeldir}/wl/sysdeps/$file\n";
				}
			}
		}
		close(DIR);
	}	

	print $fexclude "${uc_modeldir}/wl/sysdeps/${uc_model}/sys\n";
	print $fexclude "${uc_modeldir}/wl/sysdeps/${uc_model}/config\n";
	print $fexclude "${uc_modeldir}/wl/sysdeps/${uc_model}/clm\n";

	# cross exclude broadcom related path
	if (${uc_modeldir} eq "release/src-rt") {
		print $fexclude "release/src-rt-6.x/wl/sysdeps\n";
	}
	if (${uc_modeldir} eq "release/src-rt-6.x") {
		print $fexclude "release/src-rt/wl/sysdeps\n";
	}

	close($fexclude);
}
	
if ( @ARGV >= 2 ) {
	append_gpl_excludes($ARGV[0], $ARGV[1]);
}
else {
	print "usage: .gpl_excludes.pl [model] [modeldir]\n";
}

