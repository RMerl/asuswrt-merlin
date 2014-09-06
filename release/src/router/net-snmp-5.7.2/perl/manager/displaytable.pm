# displaytable(TABLENAME, CONFIG...):
#
#   stolen from sqltohtml in the ucd-snmp package
#

package NetSNMP::manager::displaytable;
use POSIX (isprint);

BEGIN {
    use Exporter ();
    use vars qw(@ISA @EXPORT_OK $tableparms $headerparms);
    @ISA = qw(Exporter);
    @EXPORT=qw(&displaytable &displaygraph);

    require DBI;
    require CGI;

    use GD::Graph();
    use GD::Graph::lines();
    use GD::Graph::bars();
    use GD::Graph::points();
    use GD::Graph::linespoints();
    use GD::Graph::area();
    use GD::Graph::pie();
};

$tableparms="border=1 bgcolor=\"#c0c0e0\"";
$headerparms="border=1 bgcolor=\"#b0e0b0\"";

sub displaygraph {
    my $dbh = shift;
    my $tablename = shift;
    my %config = @_;
    my $type = $config{'-type'} || "lines";
    my $x = $config{'-x'} || "640";
    my $y = $config{'-y'} || "480";
    my $bgcolor = $config{'-bgcolor'} || "white";
    my $datecol = $config{'-xcol'} || "updated";
    my $xtickevery = $config{'-xtickevery'} || 50;
    my ($thetable);

#    print STDERR join(",",@_),"\n";

    return -1 if (!defined($dbh) || !defined($tablename) || 
		  !defined ($config{'-columns'}) || 
		  ref($config{'-columns'}) ne "ARRAY" ||
		  !defined ($config{'-indexes'}) || 
		  ref($config{'-indexes'}) ne "ARRAY");


    my $cmd = "SELECT " . 
	join(",",@{$config{'-columns'}},
	     @{$config{'-indexes'}}, $datecol) .
		 " FROM $tablename $config{'-clauses'}";
    ( $thetable = $dbh->prepare($cmd))
	or return -1;
    ( $thetable->execute )
	or return -1;

    my %data;
    my $count = 0;

    while( $row = $thetable->fetchrow_hashref() ) {
	# XXX: multiple indexe columns -> unique name
	# save all the row's data based on the index column(s)
	foreach my $j (@{$config{'-columns'}}) {
	    if ($config{'-difference'} || $config{'-rate'}) {
		if (defined($lastval{$row->{$config{'-indexes'}[0]}}{$j}{'value'})) {
		    $data{$row->{$config{'-indexes'}[0]}}{$row->{$datecol}}{$j}=
			$row->{$j} - 
			    $lastval{$row->{$config{'-indexes'}[0]}}{$j}{'value'};
		    #
		    # convert to a rate if desired.
		    #
		    if ($config{'-rate'}) {
			if (($row->{$datecol} - $lastval{$row->{$config{'-indexes'}[0]}}{$j}{'index'})) {
			    $data{$row->{$config{'-indexes'}[0]}}{$row->{$datecol}}{$j} = $data{$row->{$config{'-indexes'}[0]}}{$row->{$datecol}}{$j}*$config{'-rate'}/($row->{$datecol} - $lastval{$row->{$config{'-indexes'}[0]}}{$j}{'index'});
			} else {
			    $data{$row->{$config{'-indexes'}[0]}}{$row->{$datecol}}{$j} = -1;
			}
		    }

		}
		$lastval{$row->{$config{'-indexes'}[0]}}{$j}{'value'} = $row->{$j};
		$lastval{$row->{$config{'-indexes'}[0]}}{$j}{'index'} = $row->{$datecol};
	    } else {
		$data{$row->{$config{'-indexes'}[0]}}{$row->{$datecol}}{$j} = $row->{$j};
	    }

	    #
	    # limit the data to a vertical range.
	    #
	    if (defined($config{'-max'}) && 
		$data{$row->{$config{'-indexes'}[0]}}{$row->{$datecol}}{$j} > 
		$config{'-max'}) {
		# set to max value
		$data{$row->{$config{'-indexes'}[0]}}{$row->{$datecol}}{$j} = 
		    $config{'-max'};
	    }
	    
	    if (defined($config{'-min'}) && 
		$data{$row->{$config{'-indexes'}[0]}}{$row->{$datecol}}{$j} < 
		$config{'-min'}) {
		# set to min value
		$data{$row->{$config{'-indexes'}[0]}}{$row->{$datecol}}{$j} = 
		    $config{'-min'};
	    }
	}
	push @xdata,$row->{$datecol};
    }

    my @pngdata;

    if (defined($config{'-createdata'})) {
	&{$config{'-createdata'}}(\@pngdata, \@xdata, \%data);
    } else {
	push @pngdata, \@xdata;

	my @datakeys = keys(%data);

#    open(O,">/tmp/data");
	foreach my $i (@datakeys) {
	    foreach my $j (@{$config{'-columns'}}) {
		my @newrow;
		foreach my $k (@xdata) {
#		print O "i=$i k=$k j=$j :: $data{$i}{$k}{$j}\n";
		    push @newrow, ($data{$i}{$k}{$j} || 0);
		}
		push @pngdata,\@newrow;
	    }
	}
    }
#    close O;

    if ($#pngdata > 0) {
    # create the graph itself
	my $graph = new GD::Graph::lines($x, $y);
	$graph->set('bgclr' => $bgcolor);
#	print STDERR "columns: ", join(",",@{$config{'-columns'}}), "\n";
 	if (defined($config{'-legend'})) {
# 	    print STDERR "legend: ", join(",",@{$config{'-legend'}}), "\n";
 	    $graph->set_legend(@{$config{'-legend'}});
 	} else {
 	    my @legend;
 	    foreach my $xxx (@{$config{'-columns'}}) {
 		push @legend, "$xxx = $config{'-indexes'}[0]";
 	    }
 	    $graph->set_legend(@legend);
 	}
	foreach my $i (qw(title x_label_skip x_labels_vertical x_tick_number x_number_format y_number_format x_min_value x_max_value y_min_value y_max_value)) {
#	    print STDERR "setting $i from -$i = " . $config{"-$i"} . "\n";
	    $graph->set("$i" => $config{"-$i"}) if ($config{"-$i"});
	}
	if ($config{'-pngparms'}) {
	    $graph->set(@{$config{'-pngparms'}});
	}
	print $graph->plot(\@pngdata);
	return $#{$pngdata[0]};
    }
    return -1;
}

sub displaytable {
    my $dbh = shift;
    my $tablename = shift;
    my %config = @_;
    my $clauses = $config{'-clauses'};
    my $dolink = $config{'-dolink'};
    my $datalink = $config{'-datalink'};
    my $beginhook = $config{'-beginhook'};
    my $modifiedhook = $config{'-modifiedhook'};
    my $endhook = $config{'-endhook'};
    my $selectwhat = $config{'-select'};
#    my $printonly = $config{'-printonly'};
    $selectwhat = "*" if (!defined($selectwhat));
    my $tableparms = $config{'-tableparms'} || $displaytable::tableparms;
    my $headerparms = $config{'-headerparms'} || $displaytable::headerparms;
    my ($thetable, $data, $ref, $prefs, $xlattable);

    if ($config{'-dontdisplaycol'}) {
	($prefs = $dbh->prepare($config{'-dontdisplaycol'}) )
	    or die "\nnot ok: $DBI::errstr\n";
    }

    # get a list of data from the table we want to display
    ( $thetable = $dbh->prepare("SELECT $selectwhat FROM $tablename $clauses"))
	or return -1;
    ( $thetable->execute )
	or return -1;

    # get a list of data from the table we want to display
    if ($config{'-xlat'}) {
	( $xlattable = 
	 $dbh->prepare("SELECT newname FROM $config{'-xlat'} where oldname = ?"))
	    or die "\nnot ok: $DBI::errstr\n";
    }
    
    # editable/markable setup
    my $edited = 0;
    my $editable = 0;
    my $markable = 0;
    my (@indexkeys, @valuekeys, $uph, %indexhash, $q);
    if (defined($config{'-editable'})) {
	$editable = 1;
    }

    if (defined($config{'-mark'}) || defined($config{'-onmarked'})) {
	$markable = 1;
    }

    if (defined($config{'-CGI'}) && ref($config{'-CGI'}) eq "CGI") {
	$q = $config{'-CGI'};
    }

    if (($editable || $markable)) {
	if (ref($config{'-indexes'}) eq ARRAY && defined($q)) {
	    @indexkeys = @{$config{'-indexes'}};
	    foreach my $kk (@indexkeys) {
		$indexhash{$kk} = 1;
	    }
	} else {
	    $editable = $markable = 0;
	    print STDERR "displaytable error: no -indexes option specified or -CGI not specified\n";
	}
    }

    if (($editable || $markable) && 
	$q->param('edited_' . toalpha($tablename))) {
	$edited = 1;
    }
	
    # table header
    my $doheader = 1;
    my @keys;
    my $rowcount = 0;
    $thetable->execute();
    if ($editable || $markable) {
	print "<input type=hidden name=\"edited_" . toalpha($tablename) . "\" value=1>\n";
    }

    while( $data = $thetable->fetchrow_hashref() ) {
	$rowcount++;
	if ($edited && $editable && !defined($uph)) {
	    foreach my $kk (keys(%$data)) {
		push (@valuekeys, maybe_from_hex($kk)) if (!defined($indexhash{$kk}));
	    }
	    my $cmd = "update $tablename set " . 
		join(" = ?, ",@valuekeys) . 
		    " = ? where " . 
			join(" = ? and ",@indexkeys) .
			    " = ?";
	    $uph = $dbh->prepare($cmd);
#	    print STDERR "setting up: $cmd<br>\n";
	}
	if ($doheader) {
	    if ($config{'-selectorder'} && 
		     ref($config{'-selectorder'}) eq "ARRAY") {
		@keys = @{$config{'-selectorder'}};
	    } elsif ($config{'-selectorder'}) {
		$_ = $selectwhat;
		@keys = split(/, */);
	    } else {
		@keys = (sort keys(%$data));
	    }
	    if (defined($config{'-title'})) {
		print "<br><b>$config{'-title'}</b>\n";
	    } elsif (!defined($config{'-notitle'})) {
		print "<br><b>";
		print "<a href=\"$ref\">" if (defined($dolink) && 
					      defined($ref = &$dolink($tablename)));
		if ($config{'-xlat'}) {
		    my $toval = $xlattable->execute($tablename);
		    if ($toval > 0) {
			print $xlattable->fetchrow_array;
		    } else {
			print "$tablename";
		    }
		} else {
		    print "$tablename";
		}
		print "</a>" if (defined($ref));
		print "</b>\n";
	    }
	    print "<br>\n";
	    print "<table $tableparms>\n";
	    if (!$config{'-noheaders'}) {
		print "<tr $headerparms>";
	    }
	    if (defined($beginhook)) {
		&$beginhook($dbh, $tablename);
	    }
	    if (!$config{'-noheaders'}) {
		if ($markable) {
		    my $ukey = to_unique_key($key, $data, @indexkeys);
		    print "<td>Mark</td>\n";
		}
		foreach $l (@keys) {
		    if (!defined($prefs) || 
			$prefs->execute($tablename, $l) eq "0E0") {
			print "<th>";
			print "<a href=\"$ref\">" if (defined($dolink) && 
						      defined($ref = &$dolink($l)));
			if ($config{'-xlat'}) {
			    my $toval = $xlattable->execute($l);
			    if ($toval > 0) {
				print $xlattable->fetchrow_array;
			    } else {
				print "$l";
			    }
			} else {
			    print "$l";
			}
			print "</a>" if (defined($ref));
			print "</th>";
		    }
		}
	    }
	    if (defined($endhook)) {
		&$endhook($dbh, $tablename);
	    }
	    if (!$config{'-noheaders'}) {
		print "</tr>\n";
	    }
	    $doheader = 0;
	}

	print "<tr>";
	if (defined($beginhook)) {
	    &$beginhook($dbh, $tablename, $data);
	}
	if ($edited && $editable) {
	    my @indexvalues = getvalues($data, @indexkeys);
	    if ($modifiedhook) {
		foreach my $valkey (@valuekeys) {
		    my ($value) = getquery($q, $data, \@indexkeys, $valkey);
		    if ($value ne $data->{$valkey}) {
			&$modifiedhook($dbh, $tablename, $valkey, 
				       $data, @indexvalues);
		    }
		}
	    }
		    
	    my $ret = $uph->execute(getquery($q, $data, \@indexkeys, @valuekeys), 
				    @indexvalues);
	    foreach my $x (@indexkeys) {
		next if (defined($indexhash{$x}));
		$data->{$x} = $q->param(to_unique_key($x, $data, @indexkeys));
	    }
#	    print "ret: $ret, $DBI::errstr<br>\n";
	}
	if ($markable) {
	    my $ukey = to_unique_key("mark", $data, @indexkeys);
	    print "<td><input type=checkbox value=Y name=\"$ukey\"" .
		(($q->param($ukey) eq "Y") ? " checked" : "") . "></td>\n";
	    if ($q->param($ukey) eq "Y" && $config{'-onmarked'}) {
		&{$config{'-onmarked'}}($dbh, $tablename, $data);
	    }
	}
	    
	foreach $key (@keys) {
	    if (!defined($prefs) || 
		$prefs->execute($tablename, $key) eq "0E0") {
		print "<td>";
		print "<a href=\"$ref\">" if (defined($datalink) && 
					      defined($ref = &$datalink($key, $data->{$key})));
		if ($editable && !defined($indexhash{$key})) {
		    my $ukey = to_unique_key($key, $data, @indexkeys);
		    my $sz;
		    if ($config{'-sizehash'}) {
			$sz = "size=" . $config{'-sizehash'}{$key};
		    }
		    if (!$sz && $config{'-inputsize'}) {
			$sz = "size=" . $config{'-inputsize'};
		    }
		    print STDERR "size $key: $sz from $config{'-sizehash'}{$key} / $config{'-inputsize'}\n";
		    print "<input type=text name=\"$ukey\" value=\"" . 
			maybe_to_hex($data->{$key}) . "\" $sz>";
		} else {
		    if ($config{'-printer'}) {
			&{$config{'-printer'}}($key, $data->{$key}, $data);
		    } elsif ($data->{$key} ne "") {
			print $data->{$key};
		    } else {
			print "&nbsp";
		    }
		}
		print "</a>" if (defined($ref));
		print "</td>";
	    }
	}

	if (defined($endhook)) {
	    &$endhook($dbh, $tablename, $data);
	}
	print "</tr>\n";
	last if (defined($config{'-maxrows'}) && 
		 $rowcount >= $config{'-maxrows'});
    }
    if ($rowcount > 0) {
	print "</table>\n";
    }
    return $rowcount;
}

sub to_unique_key {
    my $ret = shift;
    $ret .= "_";
    my $data = shift;
    if (!defined($data)) {
	$ret .= join("_",@_);
    } else {
	foreach my $i (@_) {
	    $ret .= "_" . $data->{$i};
	}
    }
    return toalpha($ret);
}

sub toalpha {
    my $ret = join("",@_);
    $ret =~ s/([^A-Za-z0-9_])/ord($1)/eg;
    return $ret;
}

sub getvalues {
    my $hash = shift;
    my @ret;
    foreach my $i (@_) {
	push @ret, maybe_from_hex($hash->{$i});
    }
    return @ret;
}

sub getquery {
    my $q = shift;
    my $data = shift;
    my $keys = shift;
    my @ret;
    foreach my $i (@_) {
	push @ret, maybe_from_hex($q->param(to_unique_key($i, $data, @$keys)));
    }
    return @ret;
}

sub maybe_to_hex {
    my $str = shift;
    if (!isprint($str)) {
	$str = "0x" . (unpack("H*", $str))[0];
    }
    $str =~ s/\"/&quot;/g;
    return $str;
}

sub maybe_from_hex {
    my $str = shift;
    if (substr($str,0,2) eq "0x") {
	($str) = pack("H*", substr($str,2));
    }
    return $str;
}

1;
__END__

=head1 NAME

SNMP - The Perl5 'SNMP' Extension Module v3.1.0 for the UCD SNMPv3 Library

=head1 SYNOPSIS

 use DBI;
 use displaytable;

 $dbh = DBI->connect(...);
 $numshown = displaytable($dbh, 'tablename', [options]);

=head1 DESCRIPTION

The displaytable and displaygraph functions format the output of a DBI
database query into an html or graph output.

=head1 DISPLAYTABLE OPTIONS

=over 4

=item -select => VALUE

Selects a set of columns, or functions to be displayed in the resulting table.

Example: -select => 'column1, column2'

Default: *

=item -title => VALUE

Use VALUE as the title of the table.

=item -notitle => 1

Don't print a title for the table.

=item -noheaders => 1

Don't print a header row at the top of the table.

=item -selectorder => 1

=item -selectorder => [qw(column1 column2)]

Defines the order of the columns.  A value of 1 will use the order of
the -select statement by textually parsing it's comma seperated list.
If an array is passed containing the column names, that order will be
used.

Example: 

  -select => distinct(column1) as foo, -selectorder => [qw(foo)]

=item -maxrows => NUM

Limits the number of display lines to NUM.

=item -tableparms => PARAMS

=item -headerparms => PARAMS

The parameters to be used for formating the table contents and the
header contents.

Defaults:

  -tableparms  => "border=1 bgcolor='#c0c0e0'"

  -headerparms => "border=1 bgcolor='#b0e0b0'"

=item -dolink => \&FUNC

If passed, FUNC(name) will be called on the tablename or header.  The
function should return a web url that the header/table name should be
linked to.

=item -datalink => \&FUNC

Identical to -dolink, but called for the data portion of the table.
Arguments are the column name and the data element for that column.

=item -printer => \&FUNC

Calls FUNC(COLUMNNAME, COLUMNDATA, DATA) to print the data from each
column.  COLUMNDATA is the data itself, and DATA is a reference to the
hash for the entire row (IE, COLUMNDATA = $DATA->{$COLUMNNAME}).

=item -beginhook => \&FUNC

=item -endhook => \&FUNC

displaytable will call these functions at the beginning and end of the
printing of a row.  Useful for inserting new columns at the beginning
or end of the table.  When the headers to the table are being printed,
they will be called like FUNC($dbh, TABLENAME).  When the data is
being printed, they will be called like FUNC($dbh, TABLENAME, DATA),
which DATA is a reference to the hash containing the row data.

Example: 

  -endhook => sub { 
      my ($d, $t, $data) = @_; 
      if (defined($data)) { 
	  print "<td>",(100 * $data->{'column1'} / $data->{'column2'}),"</td>";
      } else { 
	  print "<td>Percentage</td>"; 
      } 
  }

=item -clauses => sql_clauses

Adds clauses to the sql expression.

Example: -clauses => "where column1 = 'value' limit 10 order by column2"

=item -xlat => xlattable

Translates column headers and the table name by looking in a table for
the appropriate translation.  Essentially uses:

  SELECT newname FROM xlattable where oldname = ?

to translate everything.

=item -editable => 1

=item -indexes   => [qw(INDEX_COLUMNS)]

=item -CGI      => CGI_REFERENCE

If both of these are passed as arguments, the table is printed in
editable format.  The INDEX_COLUMNS should be a list of columns that
can be used to uniquely identify a row.  They will be the non-editable
columns shown in the table.  Everything else will be editable.  The
form and the submit button written by the rest of the script must loop
back to the same displaytable clause for the edits to be committed to
the database.  CGI_REFERENCE should be a reference to the CGI object
used to query web parameters from ($CGI_REFERENCE = new CGI);

=item -mark     => 1

=item -indexes  => [qw(INDEX_COLUMNS)]

=item -CGI      => CGI_REFERENCE

=item -onmarked => \&FUNC

When the first three of these are specified, the left hand most column
will be a check box that allows users to mark the row for future work.

FUNC($dbh, TABLENAME, DATA) will be called for each marked entry when
a submission data has been processed.  $DATA is a hash reference to
the rows dataset.  See -editable above for more information.

-onmarked => \&FUNC implies -mark => 1.

=back

=head1 Author

wjhardaker@ucdavis.edu

=cut
