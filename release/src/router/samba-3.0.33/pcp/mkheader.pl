#!/usr/bin/perl


open(PROFILE,"profile.h") || die "Unable to open profile.h\n";
@profile = <PROFILE>;
close PROFILE;

open(METRICS,"> metrics.h") || die "Unable to open metrics.h for output\n";

print METRICS "#define COUNT_TIME_INDOM 0\n";
print METRICS "#define BYTE_INDOM       1\n\n";
print METRICS "#define FIELD_OFF(x) (unsigned)\&(((struct profile_stats *)NULL)->x)\n\n";
print METRICS "typedef struct {\n";
print METRICS "\tchar *name;\n";
print METRICS "\tunsigned offset;\n";
print METRICS "} samba_instance;\n\n";

@instnames = grep(/unsigned .*_time;/,@profile);
foreach $instnames (@instnames) {
    chomp $instnames;
    $instnames =~ s/^.*unsigned (.*)_time.*$/$1/;
}

print METRICS "static samba_instance samba_counts[] = {";
$first = 1;
foreach $1 (@instnames) {
    if ($first == 1) {
	$first = 0;
	print METRICS "\n";
    } else {
	print METRICS ",\n";
    }
    print METRICS "\t{\"$1\", FIELD_OFF($1_count)}";
}
print METRICS "\n};\n\n";
print METRICS "static samba_instance samba_times[] = {";
$first = 1;
foreach $1 (@instnames) {
    if ($first == 1) {
	$first = 0;
	print METRICS "\n";
    } else {
	print METRICS ",\n";
    }
    print METRICS "\t{\"$1\", FIELD_OFF($1_time)}";
}
print METRICS "\n};\n\n";
print METRICS "static samba_instance samba_bytes[] = {";
@instnames = grep(/unsigned .*_bytes;/,@profile);
$first = 1;
foreach $_ (@instnames) {
    if ($first == 1) {
	$first = 0;
	print METRICS "\n";
    } else {
	print METRICS ",\n";
    }
    /^.*unsigned (.*)_bytes.*$/;
    print METRICS "\t{\"$1\", FIELD_OFF($1_bytes)}";
}
print METRICS "\n};\n";

close METRICS
