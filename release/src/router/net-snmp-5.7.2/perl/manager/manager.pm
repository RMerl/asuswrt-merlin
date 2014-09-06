package NetSNMP::manager;

use strict ();
use warnings;
use Apache::Constants qw(:common);
use CGI qw(:standard delete_all);
use SNMP ();
use DBI ();
use NetSNMP::manager::displaytable qw(displaytable displaygraph);

# globals
$NetSNMP::manager::hostname = 'localhost';          # Host that serves the mSQL Database
$NetSNMP::manager::dbname = 'snmp';                 # mySQL Database name
$NetSNMP::manager::user = 'root';
# $NetSNMP::manager::pass = "password";
$NetSNMP::manager::imagebase = "/home/hardaker/src/snmp/manager";	# <=== CHANGE ME ====
$NetSNMP::manager::redimage = "/graphics/red.gif";
$NetSNMP::manager::greenimage = "/graphics/green.gif";
#$NetSNMP::manager::verbose = 1;
$NetSNMP::manager::tableparms  = "border=1 bgcolor=\"#c0c0e0\"";
$NetSNMP::manager::headerparms = "border=1 bgcolor=\"#b0e0b0\"";

# init the snmp library
$SNMP::save_descriptions=1;
#SNMP::init_mib();

%NetSNMP::manager::myorder = qw(id 0 oidindex 1 host 2 updated 3);

sub handler {
    my $r = shift;
    Apache->request($r);

    # get info from handler
    my $hostname = $r->dir_config('hostname') || $NetSNMP::manager::hostname;
    my $dbname = $r->dir_config('dbname') || $NetSNMP::manager::dbname;
    my $sqluser = $r->dir_config('user') || $NetSNMP::manager::user;
    my $pass = $r->dir_config('pass') || $NetSNMP::manager::pass;
    my $verbose = $r->dir_config('verbose') || $NetSNMP::manager::verbose;

#===========================================================================
#  Global defines
#===========================================================================

my ($dbh, $query, $remuser);

$remuser = $ENV{'REMOTE_USER'};
$remuser = "guest" if (!defined($remuser) || $remuser eq "");

#===========================================================================
# Connect to the mSQL database with the appropriate driver
#===========================================================================
($dbh = DBI->connect("DBI:mysql:database=$dbname;host=$hostname", $sqluser, $pass))
    or die "\tConnect not ok: $DBI::errstr\n";

#===========================================================================
# stats Images, for inclusion on another page. (ie, slashdot user box)
#===========================================================================
if (my $group = param('groupstat')) {
    $r->content_type("image/gif");
    $r->send_http_header();
    my $cur = getcursor($dbh, "select host from usergroups as ug, hostgroups as hg where ug.groupname = '$group' and hg.groupname = '$group' and user = '$remuser'");
    while (my $row = $cur->fetchrow_hashref ) {
	if (checkhost($dbh, $group, $row->{'host'})) {
	    open(I, "$NetSNMP::manager::imagebase$NetSNMP::manager::redimage");
	    while(read(I, $_, 4096)) { print; }
	    close(I);
	}
    }
    open(I, "$NetSNMP::manager::imagebase$NetSNMP::manager::greenimage");
    while(read(I, $_, 4096)) { print; }
    close(I);
    return OK();
}


sub date_format {
    my $time = shift;
    my @out = localtime($time);
    my $ret = $out[4] . "-" . $out[3] . "-" . $out[5] . " " . $out[2] . " " . $out[1];
#    print STDERR "$time: $ret\n";
    return $ret;
}


#
# Graphing of historical data
#
if ((param('displaygraph') || param('dograph')) && param('table')) {
    my $host = param('host');
    my $group = param('group');
    if (!isuser($dbh, $remuser, $group)) {
	$r->content_type("image/png");
	$r->send_http_header();
	print "Unauthorized access to that group ($group)\n";
	return Exit($dbh, $group);
    }    
    my $table = param('table');
    my @columns;

    if (!param('dograph')) {
	$r->content_type("text/html");
	$r->send_http_header();
	print "<body bgcolor=\"#ffffff\">\n";
	print "<form>\n";
	print "<table border=1><tr><td>\n";

	print "<table>\n";
	print "<tr align=top><th></th><th>Select indexes<br>to graph</th></tr>\n";

	my $handle = getcursor($dbh, "SELECT sql_small_result distinct(oidindex) FROM $table where host = '$host'");
	my @cols;
	while (  $row = $handle->fetchrow_hashref ) {
	    print "<tr><td>$row->{oidindex}</td><td><input type=checkbox value=1 name=" . 'graph_' . displaytable::to_unique_key($row->{'oidindex'}) . "></td></tr>\n";
	}
	print "</table>\n";

	print "</td><td>\n";

	print "<table>\n";
	print "<tr align=top><th></th><th>Select Columns<br>to graph</th></tr>\n";
	my $handle = getcursor($dbh, "SELECT * FROM $table limit 1");
	my $row = $handle->fetchrow_hashref;
	map { print "<tr><td>$_</td><td><input type=checkbox value=1 name=column_" . displaytable::to_unique_key($_) . "></td></tr>\n"; } keys(%$row);
	print "</table>\n";

	print "</td></tr></table>\n";

	print "<br>Graph as a Rate: <input type=checkbox value=1 name=graph_as_rate><br>\n";
	print "<br>Maximum Y Value: <input type=text value=inf name=max_y><br>\n";
	print "<br>Minimum Y Value: <input type=text value=-inf name=min_y><br>\n";

	print "<input type=hidden name=table value=\"$table\">\n";
	print "<input type=hidden name=host value=\"$host\">\n";
	print "<input type=hidden name=dograph value=1>\n";
	print "<input type=hidden name=group value=\"$group\">\n";
	print "<input type=submit name=\"Make Graph\">\n";

	print "</form>\n";

	my $handle = getcursor($dbh, "SELECT distinct(oidindex) FROM $table where host = '$host' order by oidindex");
	return Exit($dbh, $group);
    }
    if (param('graph_all_data')) {
	$clause = "host = '$host'";
    } else {
	my $handle = getcursor($dbh, "SELECT distinct(oidindex) FROM $table where host = '$host'");
	$clause = "where (";
	while (  $row = $handle->fetchrow_hashref ) {
#	    print STDERR "graph test: " . $row->{'oidindex'} . "=" . "graph_" . displaytable::to_unique_key($row->{'oidindex'}) . "=" . param("graph_" . displaytable::to_unique_key($row->{'oidindex'})) . "\n";
	    if (param("graph_" . displaytable::to_unique_key($row->{'oidindex'}))) {
		$clause .= " or oidindex = " . $row->{'oidindex'} . "";
	    }
	}

	my $handle = getcursor($dbh, "SELECT * FROM $table limit 1");
	my $row = $handle->fetchrow_hashref;
	map { push @columns, $_ if (param('column_' . displaytable::to_unique_key($_))) } keys(%$row);

	$clause .= ")";
	$clause =~ s/\( or /\(/;
	if ($clause =~ /\(\)/ || $#columns == -1) {
	    $r->content_type("text/html");
	    $r->send_http_header();
	    print "<body bgcolor=\"#ffffff\">\n";
	    print "<h1>No Data to Graph</h1>\n";
	    print STDERR "No data to graph: $clause, $#columns\n";
	    return Exit($dbh, "$group");
	}
	$clause .= " and host = '$host'";
    }

#    print STDERR "graphing clause: $clause\n";

    # all is ok, display the graph

    $r->content_type("image/png");
    $r->send_http_header();

    print STDERR "graphing clause: $clause, columns: ", join(", ",@columns), "\n";
    my @args;
    push (@args, '-rate', '60') if (param('graph_as_rate'));
    push (@args, '-max', param('max_y')) if (param('max_y') && param('max_y') =~ /^[-.\d]+$/);
    push (@args, '-min', param('min_y')) if (param('min_y') && param('min_y') =~ /^[-.\d]+$/);

    my $ret = 
    displaygraph($dbh, $table,
#		 '-xcol', "date_format(updated,'%m-%d-%y %h:%i')",
		 '-xcol', "unix_timestamp(updated)",
		 '-pngparms', [
		     'x_labels_vertical', '1',
		     'x_tick_number', 6,
		     'x_number_format', \&date_format,
		     'y_label', 'Count/Min',
		     'title', $table,
#		     'y_min_value', 0,
		 ],
		 '-clauses', "$clause order by updated",
		 @args,
		 '-columns', \@columns,
		 '-indexes', ['oidindex']);
    print STDERR "$ret rows graphed\n";
    return OK();
}

#===========================================================================
# Start HTML.
#===========================================================================
$r->content_type("text/html");
$r->send_http_header();
print "<body bgcolor=\"#ffffff\">\n";
print "<h1>UCD-SNMP Management Console</h1>\n";
print "<hr>\n";

#===========================================================================
# Display mib related data information
#===========================================================================
if (param('displayinfo')) {
    makemibtable(param('displayinfo'));
    return Exit($dbh, "");
}

#===========================================================================
# Display a generic sql table of any kind (debugging).
#===========================================================================
# if (my $disptable = param('displaytable')) {
#     if (param('editable') == 1) {
# 	print "<form submit=dont>\n";
# 	displaytable($disptable, -editable, 1);
# 	print "</form>\n";
#     } else {
# 	displaytable($disptable);
#     }
#     return Exit($dbh,  "");
# }

#===========================================================================
# Get host and group from CGI query.
#===========================================================================
my $host = param('host');
my $group = param('group');

#===========================================================================
# Editable user information
#===========================================================================

if (param('setuponcall')) {
    print "<title>oncall schedule for user: $remuser</title>\n";
    print "<h2>oncall schedule for user: $remuser</h2>\n";
    print "<p>Please select your oncall schedule and mailing addresses for your groups below:";
    if (!isexpert($remuser)) {
	print "<ul>\n";
        print "<li>Values for the days/hours fields can be comma seperated lists of hours/days/ranges.  EG: hours: 7-18,0-4.\n";
	print "</ul>\n";
    }
    print "<form method=post><input type=hidden name=setuponcall value=1>\n";
    displaytable($dbh, 'oncall',
    '-clauses',"where user = '$remuser' order by groupname",
    '-select','id, user, groupname, email, pager, days, hours',
    '-selectorder', 1,
    '-notitle', 1,
    '-editable', 1,
    '-indexes', ['id','user','groupname'],
    '-CGI', $CGI::Q
    );
    print "<input type=submit value=\"submit changes\">\n";
    print "</form>\n";
    return Exit($dbh, $group);
}

#===========================================================================
# show the list of groups a user belongs to.
#===========================================================================
if (!defined($group)) {
    my @groups = getgroupsforuser($dbh, $remuser);
    print "<title>Net-SNMP Group List</title>\n";
    print "<h2>Host groupings you may access:</h2>\n";
    if (!isexpert($remuser)) {
	print "<ul>\n";
	print "<li>Click on a group to operate or view the hosts in that group.\n";
	print "<li>Click on a red status light below to list the problems found.\n";
	print "</ul>\n";
    }
	
    if ($#groups > 0) {
	displaytable($dbh, 'usergroups', 
		     '-clauses', "where (user = '$remuser')",
		     '-select', 'distinct groupname',
		     '-notitle', 1,
		     '-printonly', ['groupname'],
		     '-datalink', sub { my $q = self_url();
					my $key = shift;
					my $h = shift;
					return if ($key ne "groupname");
					return addtoken($q,"group=$h");
				    },
		     '-beginhook', 
		     sub { 
			 my $q = self_url();
			 my($dbh, $junk, $data) = @_;
			 if (!defined($data)) {
			     print "<th>Status</th>";
			     return;
			 }
			 my ($cur, $row);
			 $cur = getcursor($dbh, "select host from hostgroups where groupname = '$data->{groupname}'");
			 while (  $row = $cur->fetchrow_hashref ) {
			     if (checkhost($dbh, $data->{'groupname'}, 
					   $row->{'host'})) {
				 print "<td><a href=\"" . addtoken($q,"group=$data->{groupname}&summarizegroup=1") . "\"><img border=0 src=$NetSNMP::manager::redimage></a></td>\n";
				 return;
			     }
			 }
			 print "<td><img src=$NetSNMP::manager::greenimage></td>\n";
		     }
		     );
	$dbh->disconnect();
	return Exit($dbh,  $group);
    } else {
	if ($#groups == -1) {
	    print "You are not configured to use the Net-SNMP-manager, please contact your system administrator.";
	    return Exit($dbh,  $group);
	}
	$group = $groups[0];
    }
}

#===========================================================================
# reject un-authorized people accessing a certain group
#===========================================================================
if (!isuser($dbh, $remuser, $group)) {
    print "Unauthorized access to that group ($group)\n";
    return Exit($dbh, $group);
}    

#===========================================================================
# add a new host to a group
#===========================================================================
if (defined(my $newhost = param('newhost'))) {
    if (isadmin($dbh, $remuser, $group)) {
	if ($dbh->do("select * from hostgroups where host = '$newhost' and groupname = '$group'") eq "0E0") {
	    $dbh->do("insert into hostgroups(host,groupname) values('$newhost','$group')") ;
	} else {
	    print "<b>ERROR: host $newhost already in $group</b>\n";
	}
	CGI::delete('newhost');
    }
}

#===========================================================================
# display setup configuration for a group
#===========================================================================
if (defined(param('setupgroup'))) {
    if (isadmin($dbh, $remuser, $group)) {
	setupgroup($dbh, $group);
    } else {
	print "<h2>You're not able to perform setup operations for group $group\n";
    }
    return Exit($dbh, $group);
}

#===========================================================================
# save configuration information submitted about a group
#===========================================================================
if (defined(param('setupgroupsubmit')) && 
    isadmin($dbh, $remuser, $group)) {
    setupgroupsubmit($dbh, $group);
    delete_all();
    param(-name => 'group', -value => $group);
    print "<a href=\"" . self_url() . "\">Entries submitted</a>";
    return Exit($dbh, $group);
}

#===========================================================================
# user preferences
#===========================================================================
if (defined(param('userprefs'))) {
    setupuserpreferences($dbh, $remuser, $group);
    return Exit($dbh, $group);
}

#===========================================================================
# save submitted user preferences
#===========================================================================
if (defined(param('setupuserprefssubmit')) && 
    isadmin($dbh, $remuser, $group)) {
    setupusersubmit($dbh, $remuser, $group);
    delete_all();
    param(-name => 'group', -value => $group);
    print "<a href=\"" . self_url() . "\">Entries submitted</a>";
    return Exit($dbh, $group);
}

#===========================================================================
# summarize problems in a group
#===========================================================================
if (defined(param('summarizegroup'))) {
    print "<title>group problem summary: $group</title>\n";
    print "<h2>The following is a list of problems in the group \"$group\":</h2>\n";
    summarizeerrors($dbh, "where groupname = '$group'");
    return Exit($dbh, $group);
}

#===========================================================================
# summarize problems on a host
#===========================================================================
if (defined($host) && defined(param('summarizehost'))) {
    print "<title>host summary: $host</title>\n";
    print "<h2>The following is a list of problems for the host \"$host\":</h2>\n";
    summarizeerrors($dbh, "where groupname = '$group' and host = '$host'");
    return Exit($dbh, $group);
}

#===========================================================================
# display a list of hosts in a group
#===========================================================================
if (!defined($host)) {
    print "<title>Net-SNMP Host $host</title>\n";
    print "<h2>Hosts in the group \"$group\":</h2>\n";
    if (!isexpert($remuser)) {
	print "<ul>\n";
	if (isadmin($dbh, $remuser, $group)) {
	    my $q = self_url();
	    $q =~ s/\?.*//;
            print "<li>Make sure you <a href=\"" . addtoken($q,"group=$group&setupgroup=1") . "\">set up the host</a> for the SNMP tables you want to monitor.\n";
        }
	print "<li>Click on a hostname to operate on or view the information tables associated with that group.\n";
	print "<li>Click on a red status light below to list the problems found in with a particular host.\n";
	print "</ul>\n";
    }
    displaytable($dbh, 'hostgroups', 
		 '-notitle',0,
		 '-clauses', "where (groupname = '$group')",
		 '-select', 'distinct host, sysObjectId, sysDescr, sysUpTime, versionTag',
		 '-datalink', sub { my $q = self_url();
				    my $key = shift;
				    my $h = shift;
				    return if ($key ne "host");
				    return addtoken($q,"host=$h");
				},
		 '-beginhook', 
		 sub { 
		     my $q = self_url();
		     my($dbh, $junk, $data) = @_;
		     if (!defined($data)) {
			 print "<th>Status</th>";
			 return;
		     }
		     if (checkhost($dbh, $group, $data->{'host'})) {
			 print "<td><a href=\"" . addtoken($q,"group=$group&summarizehost=1&host=$data->{host}") . "\"><img border=0 src=$NetSNMP::manager::redimage></a></td>\n";
		     } else {
			 print "<td><img src=$NetSNMP::manager::greenimage></td>\n";
		     }
		 }
		 );
    if (isadmin($dbh, $remuser, $group)) {
	addhostentryform($group);
	my $q = self_url();
	$q =~ s/\?.*//;
	print "<a href=\"" . addtoken($q,"group=$group&setupgroup=1") . "\">setup group $group</a>\n";
    }
    return Exit($dbh, $group);
}

#===========================================================================
# setup the host's history records
#===========================================================================
if (param('setuphost')) {
    print "<title>Net-SNMP history setup for host: $host</title>\n";
    print "<h2>Net-SNMP history setup for the host: \"$host\"</h2>\n";
    print "<p>Enter the number of days to keep the data for a given table for the host \"$host\":\n";
    if (!isexpert($remuser)) {
	print "<ul>\n";
        print "<li>Numbers must be greater than or equal to 1 to enable history logging.\n";
	print "</ul>\n";
    }
    print "<form method=post><input type=hidden name=setuphost value=1><input type=hidden name=host value=\"$host\"><input type=hidden name=group value=\"$group\">\n";
    displaytable($dbh, 'hosttables',
    '-clauses',"where host = '$host' and groupname = '$group'",
    '-select','groupname, host, tablename, keephistory',
    '-selectorder', 1,
    '-notitle', 1,
    '-editable', 1,
    '-indexes', ['groupname','host','tablename'],
    '-CGI', $CGI::Q
    );
    print "<input type=submit value=\"submit changes\">\n";
    print "</form>\n";
    return Exit($dbh, $group);
}

#===========================================================================
# display a huge table of history about something
#===========================================================================
if (param('displayhistory')) {
    if (!isuser($dbh, $remuser, $group)) {
        print "Unauthorized access to that group ($group)\n";
        return Exit($dbh, $group);
    }
    displaytable($dbh, param('table'), 
    '-clauses', "where (host = '$host')",
    '-dolink', \&linktodisplayinfo,
    '-dontdisplaycol', "select * from userprefs where user = '$remuser' and groupname = '$group' and tablename = ? and columnname = ? and displayit = 'N'"
    );
    return Exit($dbh, $group);
}

#===========================================================================
# display inforamation about a host
#  optionally add new collection tables
#===========================================================================
showhost($dbh, $host, $group, $remuser);
if (isadmin($dbh, $remuser, $group)) {
    if (param('newtables')) {
    	my $x = param('newtables');
    	$x =~ s/,/ /g;
    	if (/[^\w\s]/) {
    	    print "<br>Illegal table names in addition list: $x<br>\n" 
    	} else {
	    my @x = split(/\s+/,$x);
	    foreach my $i (@x) {
		$dbh->do("insert into hosttables(host, groupname, tablename, keephistory) values('$host','$group','$i','0')");
	    }
    	    print "<br>adding: ",join(", ",@x),"<br>\n";
    	}
    } else {
        print "<br>Add new MIB Tables or Groups that you want to collect for this host: <form><input type=hidden name=host value=\"$host\"><input type=hidden name=group value=\"$group\"><input name=\"newtables\" type=text><br><input type=submit value=\"add tables\"></form>\n";
    }
    my $q = self_url();
    $q =~ s/\?.*//;
    print "<a href=\"" . addtoken($q, "setuphost=1&host=$host&group=$group") . "\">setup host $host</a>\n";
}
return Exit($dbh, $group);

#===========================================================================
# END of handler
#===========================================================================

}

# add a token to a url string.  Use either a ? or an & depending on
# existence of ?.
sub addtoken {
    my $url = shift;
    my $token = shift;
    return "$url&$token" if ($url =~ /\?/);
    return "$url?$token";
}

#
# summarizeerrors(DB-HANDLE, CLAUSE):
#   summarize the list of errors in a given CLAUSE
#
sub summarizeerrors {
    my $dbh = shift;
    my $clause = shift;
    $clause = "where" if ($clause eq "");
    my $clause2 = $clause;
    $clause2 =~ s/ host / hosterrors.host /;

    # Major errors
    displaytable($dbh, 'hosterrors, hostgroups',  # , hostgroups
		 '-select', "hosterrors.host as host, errormsg",
		 '-notitle', 1,
		 '-title', "Fatal Errors",
		 '-clauses', "$clause2 and hosterrors.host = hostgroups.host",
		 '-beginhook', sub {
		     if ($#_ < 2) {
			 #doing header;
			 print "<td></td>";
		     } else {
			 print "<td><img src=\"$NetSNMP::manager::redimage\"></td>\n";
		     }});

    my $tabletop = "<br><table $NetSNMP::manager::tableparms><tr $NetSNMP::manager::headerparms><th><b>Host</b></th><th><b>Table</b></th><th><b>Description</b></th></tr>\n";
    my $donetop = 0;
    my $cursor = 
	getcursor($dbh, "SELECT * FROM hosttables $clause");

    while (my $row = $cursor->fetchrow_hashref ) {

	my $exprs = getcursor($dbh, "SELECT * FROM errorexpressions where (tablename = '$row->{tablename}')");
	
	while (my  $expr = $exprs->fetchrow_hashref ) {
	    my $errors = getcursor($dbh, "select * from $row->{tablename} where $expr->{expression} and host = '$row->{host}'");
	    while (my  $error = $errors->fetchrow_hashref ) {
		print $tabletop if ($donetop++ == 0);
		print "<tr><td>$row->{host}</td><td>$row->{tablename}</td><td>$expr->{returnfield}: $error->{$expr->{returnfield}}</td></tr>";
	    }
	}
    }
    print "</table>";
}

#
# getcursor(CMD):
#    genericlly get a cursor for a given sql command, displaying and
#    printing errors where necessary.
#
sub getcursor {
    my $dbh = shift;
    my $cmd = shift;
    my $cursor;
    ( $cursor = $dbh->prepare( $cmd ))
	or print "\nnot ok: $DBI::errstr\n";
    ( $cursor->execute )
	or print( "\tnot ok: $DBI::errstr\n" );
    return $cursor;
}

#
# mykeysort($a, $b)
#    sorts $a and $b against the order in the mib or against the hard
#    coded special list.
#
sub mykeysort {
    my $a = $displaytable::a;
    my $b = $displaytable::b;
    my $mb = $SNMP::MIB{SNMP::translateObj($b)};
    my $ma = $SNMP::MIB{SNMP::translateObj($a)};

    return $NetSNMP::manager::myorder{$a} <=> $NetSNMP::manager::myorder{$b} if ((defined($NetSNMP::manager::myorder{$a}) || !defined($ma->{'subID'})) && (defined($NetSNMP::manager::myorder{$b}) || !defined($mb->{'subID'})));
    return 1 if (defined($NetSNMP::manager::myorder{$b}) || !defined($mb->{'subID'}));
    return -1 if (defined($NetSNMP::manager::myorder{$a}) || !defined($ma->{'subID'}));

    $ma->{'subID'} <=> $mb->{'subID'};
}

#
# checkhost(GROUP, HOST):
#    if anything in a host is an error, as defined by the
#    errorexpressions table, return 1, else 0
#
sub checkhost {
    my $dbh = shift;
    my $group = shift;
    my $host = shift;
    my ($tblh);

    return 2 if ($dbh->do("select * from hosterrors where host = '$host'") ne "0E0");

    # get a list of tables we want to display
    $tblh = getcursor($dbh, "SELECT * FROM hosttables where (host = '$host' and groupname = '$group')");

    # table data
    my($exprs, $tablelist);
    while ( $tablelist = $tblh->fetchrow_hashref ) {
	$exprs = getcursor($dbh, "SELECT * FROM errorexpressions where (tablename = '$tablelist->{tablename}')");
	while(my $expr = $exprs->fetchrow_hashref) {
	    if ($dbh->do("select * from $tablelist->{tablename} where $expr->{expression} and host = '$host'") ne "0E0") {
		return 1;
	    }
	}
    }
    return 0;
}

#
#  showhost(HOST):
#
#    display all the tables monitored for a given host (in a group).
#
sub showhost {
    my $dbh = shift;
    my $host = shift;
    my $group = shift;
    my $remuser = shift;
    my $q = self_url();
    $q =~ s/\?.*//;
    # host header
    print "<title>Net-SNMP manager report for host: $host</title>\n";
    print "<h2>Monitored information for the host $host</h2>\n";
    if (!isexpert($remuser)) {
	print "<ul>\n";
	print "<li>Click on a column name for information about the data in that column.\n";
	print "<li>Click on a column name or table name for information about the data in the table.\n";
	print "<li>If you are <a href=\"" . addtoken($q, "setuphost=1&host=$host&group=$group") . "\">collecting past history</a> for a data set, links will appear below the table that allow you to view and/or graph the historic data.\n";
	print "</ul>\n";
    }

    # does the host have a serious error?

    my $errlist = getcursor($dbh, "SELECT * FROM hosterrors where (host = '$host')");
    if ( $dbh->do("SELECT * FROM hosterrors where (host = '$host')") ne "0E0") {
	displaytable($dbh, 'hosterrors', 
		     '-clauses', "where (host = '$host')",
		     '-dontdisplaycol', "select * from userprefs where user = '$remuser' and groupname = '$group' and tablename = ? and columnname = ? and displayit = 'N'",
		     '-beginhook', sub {
			 if ($#_ < 2) {
			     #doing header;
			     print "<td></td>";
			 } else {
			     print "<td><img src=\"$NetSNMP::manager::redimage\"></td>\n";
			 }});
    }

    # get a list of tables we want to display
    my $tblh = getcursor($dbh, "SELECT * FROM hosttables where (host = '$host' and groupname = '$group')");

    # table data
    my($tablelist);
    while (  $tablelist = $tblh->fetchrow_hashref ) {

	displaytable($dbh, $tablelist->{'tablename'},
		     '-clauses', "where (host = '$host') order by oidindex",
		     '-dontdisplaycol', "select * from userprefs where user = '$remuser' and groupname = '$group' and tablename = ? and columnname = ? and displayit = 'N'",
		     '-sort', \&mykeysort,
		     '-dolink', \&linktodisplayinfo,
		     '-beginhook', \&printredgreen);
	if ($tablelist->{'keephistory'}) {
	    my $q = self_url();
	    $q =~ s/\?.*//;
	    print "history: ";
	    print "<a href=\"" . addtoken($q, "displayhistory=1&host=$host&group=$group&table=$tablelist->{'tablename'}hist") . "\">[table]</a>\n";
	    print "<a href=\"" . addtoken($q, "displaygraph=1&host=$host&group=$group&table=$tablelist->{'tablename'}hist") . "\">[graph]</a>\n";
	    print "<br>\n";
	}
    }
}

#
#  linktodisplayinfo(STRING):
#
#    returns a url to the appropriate displayinfo link if STRING is a
#    mib node.
#
sub linktodisplayinfo {
    return if (exists($NetSNMP::manager::myorder{shift}));
    return self_url() . "&displayinfo=" . shift;
}

# printredgreen(TABLENAME, DATA):
#
#   display a red or a green dot in a table dependent on the table's
#   values and associated expression
#
#   DATA is NULL when in a header row (displaying header names).
#
sub printredgreen {
    my $dbh = shift;
    my $tablename = shift;
    my $data = shift;
    my ($exprs, $expr, $img);

    if (!defined($data)) {
	#doing header;
	print "<td></td>";
	return;
    }

    my $cmd = "SELECT * FROM errorexpressions where (tablename = '$tablename')";
    print " $cmd\n" if ($NetSNMP::manager::verbose);
    ( $exprs = $dbh->prepare( $cmd ) )
	or die "\nnot ok: $DBI::errstr\n";
    ( $exprs->execute )
	or print( "\tnot ok: $DBI::errstr\n" );

    $img = $NetSNMP::manager::greenimage;
    while($expr = $exprs->fetchrow_hashref) {
	if ($dbh->do("select oidindex from $tablename where host = '$data->{host}' and oidindex = '$data->{oidindex}' and $expr->{expression}") ne "0E0") {
	    $img = $NetSNMP::manager::redimage;
	}
    }
    print "<td><img src=$img></td>";
}

#
# display information about a given mib node as a table.
#
sub makemibtable {
    my $dispinfo = shift;
    # display information about a data type in a table
    my $mib = $SNMP::MIB{SNMP::translateObj($dispinfo)};
    print "<table $NetSNMP::manager::tableparms><tr><td>\n";
    foreach my $i (qw(label type access status units hint moduleID description enums)) {
#    foreach my $i (keys(%$mib)) {
	next if (!defined($$mib{$i}) || $$mib{$i} eq "");
	next if (ref($$mib{$i}) eq "HASH" && $#{keys(%{$$mib{$i}})} == -1);
	print "<tr><td>$i</td><td>";
	if (ref($$mib{$i}) eq "HASH") {
	    print "<table $NetSNMP::manager::tableparms><tr><td>\n";
	    foreach my $j (sort { $$mib{$i}{$a} <=> $$mib{$i}{$b} } keys(%{$$mib{$i}})) {
 		print "<tr><td>$$mib{$i}{$j}</td><td>$j</td></tr>";
	    }
	    print "</table>\n";
	} else {
	    print "$$mib{$i}";
	}
	print "</td></tr>\n";
    }
    print "</table>\n";
}

# given a user, get all the groups he belongs to.
sub getgroupsforuser {
    my (@ret, $cursor, $row);
    my ($dbh, $remuser) = @_;
    ( $cursor = $dbh->prepare( "SELECT * FROM usergroups where (user = '$remuser')"))
	or die "\nnot ok: $DBI::errstr\n";
    ( $cursor->execute )
	or print( "\tnot ok: $DBI::errstr\n" );

    while (  $row = $cursor->fetchrow_hashref ) {
	push(@ret, $row->{'groupname'});
    }
    @ret;
}

# given a host, get all the groups it belongs to.
sub gethostsforgroup {
    my (@ret, $cursor, $row);
    my ($dbh, $group) = @_;
    ( $cursor = $dbh->prepare( "SELECT * FROM hostgroups where (groupname = '$group')"))
	or die "\nnot ok: $DBI::errstr\n";
    ( $cursor->execute )
	or print( "\tnot ok: $DBI::errstr\n" );

    while (  $row = $cursor->fetchrow_hashref ) {
	push(@ret, $row->{'host'});
    }
    @ret;
}

# display the host add entry box
sub addhostentryform {
    my $group = shift;
    print "<form method=\"get\" action=\"" . self_url() . "\">\n";
    print "Add a new host to the group \"$group\": <input type=\"text\" name=\"newhost\"><br>";
    print "<input type=\"hidden\" name=\"group\" value=\"$group\">";
    print "<input type=submit value=\"Add Hosts\">\n";
    print "</form>";
}

#is an expert user?
sub isexpert {
    return 0;
}

#is remuser a admin?
sub isadmin {
    my ($dbh, $remuser, $group) = @_;
    return 0 if (!defined($remuser) || !defined($group));
    return 1 if ($dbh->do("select * from usergroups where user = '$remuser' and groupname = '$group' and isadmin = 'Y'") ne "0E0");
    return 0;
}

#is user a member of this group?
sub isuser {
    my ($dbh, $remuser, $group) = @_;
    return 0 if (!defined($remuser) || !defined($group));
    return 1 if ($dbh->do("select * from usergroups where user = '$remuser' and groupname = '$group'") ne "0E0");
    return 0;
}

# displayconfigarray(HOSTS, NAMES, CONFIG):
#
#   displays an array of generic check buttons to turn on/off certain
#   variables.
sub displayconfigarray {
    my $dbh = shift;
    my $hosts = shift;
    my $names = shift;
    my %config = @_;

    my $cmd;
    if ($config{'-check'}) {
	( $cmd = $dbh->prepare( $config{'-check'} ) )
	    or die "\nnot ok: $DBI::errstr\n";
    }

    print "<table $NetSNMP::manager::tableparms>\n";
    print "<tr><td></td>";
    my ($i, $j);
    foreach $j (@$names) {
	my $nj = $j;
	$nj = $j->[0] if ($config{'-arrayrefs'} || $config{'-arrayref2'});
	print "<td>$nj</td>";
    }
    foreach my $i (@$hosts) {
	my $ni = $i;
	$ni = $i->[0] if ($config{'-arrayrefs'} || $config{'-arrayref1'});
	print "<tr><td>$ni</td>";
	foreach $j (@$names) {
	    my $nj = $j;
	    $nj = $j->[0] if ($config{'-arrayrefs'} || $config{'-arrayref2'});
	    my $checked = "checked" if (defined($cmd) && $cmd->execute($ni,$nj) ne "0E0");
	    print "<td><input type=checkbox $checked value=y name=" . $config{prefix} . $ni . $nj . "></td>\n";
	}
	print "</tr>\n";
    }	
    print "</tr>";
    print "</table>";
}

sub adddefaulttables {
    my ($dbh, $names) = @_;
    my $row;
    # add in known expression tables.
    my $handle = getcursor($dbh, "SELECT * FROM errorexpressions");

    expr: 
    while($row = $handle->fetchrow_hashref) {
	foreach $i (@$names) {
	    if ($i->[0] eq $row->{tablename}) {
		next expr;
	    }
	}
	push @$names, [$row->{tablename}];
    }
}

#
# display the setup information page for a given group.
#
sub setupgroup {
    my $dbh = shift;
    my $group = shift;
    
    my ($hosts, $names) = gethostandgroups($dbh, $group);
    adddefaulttables($dbh, $names);

    print "<form method=\"post\" action=\"" . self_url() . "\">\n";
    print "<input type=hidden text=\"setupgroupsubmit\" value=\"y\">";
    displayconfigarray($dbh, $hosts, $names, 
		       -arrayrefs, 1,
		       -check, "select * from hosttables where (host = ? and tablename = ? and groupname = '$group')");
    print "<input type=hidden name=group value=\"$group\">\n";
    print "<input type=submit value=submit name=\"setupgroupsubmit\">\n";
    print "</form>";
}

# a wrapper around fetching arrays of everything in a table.
sub getarrays {
    my $dbh = shift;
    my $table = shift;
    my %config = @_;
    my $selectwhat = $config{'-select'} || "*";
    my $handle;
    
    $handle = getcursor($dbh, "SELECT $selectwhat FROM $table $config{-clauses}");
    return $handle->fetchall_arrayref;
}

#
# get a list of all tablenames and hostnames for a given group.
#
sub gethostandgroups {
    my $dbh = shift;
    my $group = shift;
    my ($tbnms);

    my $names = getarrays($dbh, 'hosttables', 
			  "-select", 'distinct tablename',
			  "-clauses", "where groupname = '$group'");

    my $hosts = getarrays($dbh, 'hostgroups', 
			  "-select", 'distinct host',
			  "-clauses", "where groupname = '$group'");
    
    return ($hosts, $names);
}

sub setupgroupsubmit {
    my $dbh = shift;
    my $group = shift;
    
    my ($hosts, $names) = gethostandgroups($dbh, $group);
    adddefaulttables($dbh, $names);

    foreach my $i (@$hosts) {
	$dbh->do("delete from hosttables where host = '${$i}[0]' and groupname = '$group'");
    }
    my $rep = $dbh->prepare("insert into hosttables(host,tablename,groupname) values(?,?,'$group')");

    foreach my $i (@$hosts) {
	foreach my $j (@$names) {
	    if (param("${$i}[0]" . "${$j}[0]")) {
		print "test: ","${$i}[0] : ${$j}[0]<br>\n";
		$rep->execute("${$i}[0]", "${$j}[0]") || print "$! $DBI::errstr<br>\n";
            }
	}
    }
    
}

#
# save user pref data submitted by the user
#
sub setupusersubmit {
    my ($dbh, $remuser, $group) = @_;
    my $tables = getarrays($dbh, 'hosttables', 
			   "-select", 'distinct tablename',
			   "-clauses", "where groupname = '$group'");
    
    $dbh->do("delete from userprefs where user = '$remuser' and groupname = '$group'");
    my $rep = $dbh->prepare("insert into userprefs(user, groupname, tablename, columnname, displayit) values('$remuser', '$group', ?, ?, 'N')");

    my ($i, $j);
    foreach my $i (@$tables) {
	my $sth = $dbh->prepare("select * from ${$i}[0] where 1 = 0");
	$sth->execute();

	foreach $j (@{$sth->{NAME}}) {
	    if (param("${$i}[0]" . "$j")) {
		$rep->execute("${$i}[0]", "$j");
	    }
	}
    }
}

sub Exit {
    my ($dbh, $group) = @_;
    my $tq = self_url();
    $tq =~ s/\?.*//;
    print "<hr>\n";
    print "<a href=\"$tq\">[TOP]</a>\n";
    print "<a href=\"$tq?userprefs=1&group=$group\">[display options]</a>\n";
    print "<a href=\"$tq?setuponcall=1\">[setup oncall schedule]</a>\n";
    if (defined($group)) {
	print "<a href=\"$tq?group=$group\">[group: $group]</a>\n";
	print "<a href=\"$tq?group=$group&summarizegroup=1\">[summarize errors]</a>\n";
    }
    $dbh->disconnect() if (defined($dbh));
    return OK();
#    exit shift;
}

#
# setup user preferences by displaying a configuration array of
# checkbuttons for each table.
#
sub setupuserpreferences {
    my ($dbh, $remuser, $group) = @_;
    my $tables = getarrays($dbh, 'hosttables', 
			   "-select", 'distinct tablename',
			   "-clauses", "where groupname = '$group'");

    print "<h3>Select the columns from the tables that you want to <b>hide</b> below and click on submit:</h3>\n";
    print "<form method=\"post\" action=\"" . self_url() . "\">\n";

    my ($i, $j);
    foreach my $i (@$tables) {
	my $sth = $dbh->prepare("select * from ${$i}[0] where 1 = 0");
	$sth->execute();
	displayconfigarray($dbh, [${$i}[0]], $sth->{NAME},
			   -check, "select * from userprefs where (tablename = ? and columnname = ? and user = '$remuser' and groupname = '$group' and displayit = 'N')");
    print "<br>\n";
    }
    print "<input type=hidden name=group value=\"$group\">\n";
    print "<input type=submit value=submit name=\"setupuserprefssubmit\">\n";
    print "</form>";
}
