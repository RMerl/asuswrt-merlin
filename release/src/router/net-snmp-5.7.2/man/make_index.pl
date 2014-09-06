#!/usr/bin/perl
#
# Creates a .xhtml compliant index.html file
#
my $infile = shift @ARGV;

map { s/\.[0-9]$//; $pages{$_} = 1; } @ARGV;

open(I,$infile);
$first = 1;
print '<p class="SectionTitle">
Man pages
</p>
';

while (<I>) {
    if (/^#\s*(.*)/) {
	print "</table>\n" if (!$first);
	print "<h2>$1</h2>\n";
        print "<table width=\"100%\">\n";
	$first = 0;
    } else {
	my $name = $_;
	my $title;
	chomp($name);
	if (!exists($pages{$name})) {
	    print STDERR "$name is in $infile, but not in the rest of the args.\n";
	    print STDERR "Make sure it's not listed twice in $infile!\n";
	}
	open(H,"$name.html");
	while (<H>) {
	    if (/<h1>(.*?)<\/h1>/i) {
		$title = $1;
	    }
            
	    if (/<h2>NAME<\/h2>(.*)/i) {
                $_ = $1;

                # Ignore blank lines
		while (/^\s*$/) {
		    $_ = <H>;
		}

                $title = $_;
		chomp($title);
                $title =~ s/\s*$name\s*-\s*//;

                # Remove any complete <> tags
                $title =~ s/<.*>//i;
                # Remove any half open tags               
                $title =~ s/<.*//i;
              }
	}
	close(H);
	print "  <tr>\n";
        print "    <td width=\"30%\"><a href=\"$name.html\">$name</a></td>\n";
        print "    <td>$title</td>\n";
        print "  </tr>\n";
        print "\n";
	delete $pages{$name};
    }
}
print '</table>
<br/>';

@left = keys(%pages);
if ($#left > -1) {
    print STDERR "missing a filing location for: ",
      join(", ", @left), "\n";
}
