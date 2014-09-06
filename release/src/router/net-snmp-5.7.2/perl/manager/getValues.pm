#
# getValues($dbh, 
#           [-varcol => varname,]
#           [-valcol => varval,]
#           [-key => keyname,]
#           tablename => indexname,
#           ...)

package NetSNMP::manager::getValues;
require Exporter;
@ISA = qw(Exporter);
@EXPORT_OK=(getValues);

my $varcol = "varcol";
my $valcol = "valcol";
my $key = "lookup";

sub getValues {
    my $dbh = shift;
    my (@vars2, $tmp, $cursor, $row, %results, $i, $cursor);
    my ($varcol, $valcol, $key) = ($varcol, $valcol, $key);
    my @vars = @_;
    while($#vars >= 0) {
	$i = shift @vars;
	$tmp = shift @vars;
	if ($i =~ /^-/) {
	    $varcol = $tmp if ($i =~ /-varcol/);
	    $valcol = $tmp if ($i =~ /-valcol/);
	    $key = $tmp if ($i =~ /-key/);
	} else {
	    push(@vars2,$i,$tmp);
	}
    }
    while($#vars2 >= 0) {
	$i = shift @vars2;
	$tmp = shift @vars2;
#	print "select $varcol,$valcol from $i where $key = '$tmp'\n";
	($cursor =
	 $dbh->prepare("select $varcol,$valcol from $i where $key = '$tmp'"))
	    or die "\nnot ok: $DBI::errstr\n";
	($cursor->execute)
	    or die "\nnot ok: $DBI::errstr\n";
	while ( $row = $cursor->fetchrow_hashref ) {
	    $results{$row->{$varcol}} = $row->{$valcol};
	}
    }
    return %results;
}
1;
