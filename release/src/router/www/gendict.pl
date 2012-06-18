#!/usr/bin/perl
#
#	gendict.pl
#
#	- replace model name
#	- replace model dependent text
#


if (($#ARGV < 0) || ($#ARGV > 3 )) {
	print "Usage: gendict.pl <model name> <filename> <install dir>";
}

$mname = $ARGV[0];
$fname = $ARGV[1];
$dname = $ARGV[2];

my $fdict, $ftmp;
my $area;

# replace mode name and collect model dependent text

if(open($fdict, "$fname")) {
	$area = 0;
	
	open($ftmp, ">dict.tmp");

	while (<$fdict>) {
		if(/\[MODELDEP:$mname/) {
			$area=1;
		}
		elsif(/\[###MODEL_DEP###\]\[$mname/) {
			$area=3;
		}
		elsif(/\[MODEL_DEP:/) {
			$area=2;
		}
		elsif (/^\[/) {
			$area=2;
		}
		else {
			$area=0;
		}

		if ($area =~ 0) {
			s/RT-N66U/$mname/g;
			s/ZVMODELVZ/$mname/g;
			print $ftmp $_;
		}
		elsif($area =~ 1) 
		{
			s/\[MODELDEP:$mname\]//g;
			s/RT-N66U/$mname/g;
			s/ZVMODELVZ/$mname/g;
			my ($key, $desc) = split(/=/);
			$hash{$key}=$desc;
			printf $key . "=" . "$desc";
		}	
		elsif($area =~ 3) 
		{
			s/\[###MODEL_DEP###\]\[$mname\]//g;
			s/RT-N66U/$mname/g;
			s/ZVMODELVZ/$mname/g;
			my ($key, $desc) = split(/=/);
			$hash{$key}=$desc;
			printf $key . "=" . "$desc";
		}	
	}

	close($ftmp);
	close($fdict)
}	

if(open($ftmp, "dict.tmp")) {
	open($fdict, ">$dname");

	while(<$ftmp>) {
		if(/=/) {
			my ($key, $desc) = split(/=/);

			if ( exists $hash{$key} ) {
				print $fdict "$key" . "=" . "$hash{$key}";
				delete $hash{$key};
			}
			else {
				print $fdict $_;
			}
		}
		else {
			print $fdict $_;
		}
	}

	while ( my ($key, $desc) = each(%hash)) {
		print $fdict "$key" . "=" . "$hash($key}";
	}
		
	close($fdict);
	close($ftmp);
}

