#!/usr/bin/perl -w
use strict;

my $dir = shift;
chdir $dir || die "$dir: $!\n";

my $gitdesc = `git describe --always --dirty || echo -- \"0-gUNKNOWN\"`;
chomp $gitdesc;
my $gitsuffix = ($gitdesc =~ /([0-9a-fA-F]{7}(-dirty)?)$/) ? "-g$1" : "-gUNKNOWN";

printf STDERR "git suffix: %s\n", $gitsuffix;
printf "#define GIT_SUFFIX \"%s\"\n", $gitsuffix;

my $gitcommit = `git log -1 --format=\"%H\" || echo DEADBEEF`;
chomp $gitcommit;
open(BRANCHES, "git branch -a -v --abbrev=40|") || die "git branch: $!\n";
my @names = ();
while (<BRANCHES>) {
	chomp $_;
	if (/\s+(.*?)\s+$gitcommit/) {
		my $branch = $1;
		if ($branch =~ /^remotes\/(.*?)(\/.*)$/) {
			my $path = $2;
			my $url = `git config --get "remote.$1.url"`;
			chomp $url;
			$url =~ s/^(git:|https?:|git@)\/\/github\.com/github/i;
			$url =~ s/^(ssh|git):\/\/git\.sv\.gnu\.org\/srv\/git\//savannah:/i;
			$url =~ s/^(ssh|git):\/\/git\.savannah\.nongnu\.org\//savannah:/i;

			push @names, $url.$path;
		} else {
			push @names, 'local:'.$branch;
		}
	}
}

printf STDERR "git branches: %s\n", join(", ", @names);

my $cr = "\\r\\n\\";
printf <<EOF, $gitdesc, join($cr."\n\\t", @names);
#define GIT_INFO "$cr
This is a git build of %s$cr
Associated branch(es):$cr
\\t%s$cr
"
EOF

