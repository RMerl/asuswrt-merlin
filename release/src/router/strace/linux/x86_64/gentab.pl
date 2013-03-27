#!/usr/bin/perl -w
#generate syscall table from a template file (usually the master i386 syscall
#ent.h) and the x86_64 unistd.h
%conv =  (
	"exit" => "_exit",
);

%known = (
	"mmap" => "sys_mmap",
	"sched_yield" => "printargs",
);

# only used when the template file has no entry
%args = (
	"arch_prctl" => 2,
	"tkill" => 2,
	"gettid" => 0,
	"readahead" => 3,
	# should decode all these:
	"setxattr" => 5,
	"lsetxattr" => 5,
	"fsetxattr" => 5,
	"getxattr" => 4,
	"lgetxattr" => 4,
	"fgetxattr" => 4,
	"listxattr" => 3,
	"llistxattr" => 3,
	"flistxattr" => 3,
	"removexattr" => 2,
	"lremovexattr" => 2,
	"fremovexattr" => 2,
	"mmap" => 6,
	"sched_yield" => 0,
);

open(F,$ARGV[0]) || die "cannot open template file $ARGV[0]\n";

while (<F>) {
	next unless /{/;
	s/\/\*.*\*\///;
	($name) = /"([^"]+)"/;
	chomp;
	$call{$name} = $_;
}

open(SL, ">syscallnum.h") || die "cannot create syscallnum.h\n";



open(S,$ARGV[1]) || die "cannot open syscall file $ARGV[1]\n";
while (<S>) {
	$name = "";
	next unless  (($name, $num) = /define\s+__NR_(\S+)\s+(\d+)/);
	next if $name eq "";

	$name = $conv{$name} if defined($conv{$name});

	if (!defined($call{$name})) {
		unless (defined($args{$name})) {
			print STDERR "unknown call $name $num\n";
			$na = 3;
		} else {
			$na = $args{$name};
		}
		if (defined($known{$name})) {
			$func = $known{$name};
		} else {
			$func = "printargs";
		}
		print "\t{ $na,\t0,\t$func,\t\"$name\" }, /* $num */\n";
	} else {
		print "$call{$name} /* $num */\n";
	}
	print SL "#define SYS_$name $num\n"
}
