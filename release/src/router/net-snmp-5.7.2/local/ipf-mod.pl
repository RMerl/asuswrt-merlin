#!/usr/bin/perl -s
##
## IP Filter UCD-SNMP pass module
##
## Allows read IP Filter's tables (In, Out, AccIn, AccOut),
## fetching rules, hits and bytes (for accounting tables only).
##
## Author: Yaroslav Terletsky <ts@polynet.lviv.ua>
## Date: $ Tue Dec  1 10:24:08 EET 1998 $
## Version: 1.1a

# Put this file in /usr/local/bin/ipf-mod.pl and then add the following 
# line to your snmpd.conf file (without the # at the front):
#
#   pass .1.3.6.1.4.1.2021.13.2 /usr/local/bin/ipf-mod.pl

# enterprises.ucdavis.ucdExperimental.ipFilter	= .1.3.6.1.4.1.2021.13.2
# ipfInTable.ipfInEntry.ipfInIndex		integer	= 1.1.1
# ipfInTable.ipfInEntry.ipfInRule		string	= 1.1.2
# ipfInTable.ipfInEntry.ipfInHits		counter	= 1.1.3
# ipfOutTable.ipfOutEntry.ipfOutIndex		integer	= 1.2.1
# ipfOutTable.ipfOutEntry.ipfOutRule		string	= 1.2.2
# ipfOutTable.ipfOutEntry.ipfOutHits		counter	= 1.2.3
# ipfAccInTable.ipfAccInEntry.ipfAccInIndex	integer	= 1.3.1
# ipfAccInTable.ipfAccInEntry.ipfAccInRule	string	= 1.3.2
# ipfAccInTable.ipfAccInEntry.ipfAccInHits	counter	= 1.3.3
# ipfAccInTable.ipfAccInEntry.ipfAccInBytes	counter	= 1.3.4
# ipfAccOutTable.ipfAccOutEntry.ipfAccOutIndex	integer	= 1.4.1
# ipfAccOutTable.ipfAccOutEntry.ipfAccOutRule	string	= 1.4.2
# ipfAccOutTable.ipfAccOutEntry.ipfAccOutHits	counter	= 1.4.3
# ipfAccOutTable.ipfAccOutEntry.ipfAccOutBytes	counter	= 1.4.4

# variables types
%type = ('1.1.1', 'integer', '1.1.2', 'string', '1.1.3', 'counter',
	 '2.1.1', 'integer', '2.1.2', 'string', '2.1.3', 'counter',
	 '3.1.1', 'integer', '3.1.2', 'string', '3.1.3', 'counter',
	 '3.1.4', 'counter',
	 '4.1.1', 'integer', '4.1.2', 'string', '4.1.3', 'counter',
	 '4.1.4', 'counter');

# getnext sequence
%next = ('1.1.1', '1.1.2', '1.1.2', '1.1.3', '1.1.3', '2.1.1',
	 '2.1.1', '2.1.2', '2.1.2', '2.1.3', '2.1.3', '3.1.1',
	 '3.1.1', '3.1.2', '3.1.2', '3.1.3', '3.1.3', '3.1.4',
	 '3.1.4', '4.1.1',
	 '4.1.1', '4.1.2', '4.1.2', '4.1.3', '4.1.3', '4.1.4');

# ipfilter's commands to fetch needed information
$ipfstat_comm="/sbin/ipfstat";
$ipf_in="$ipfstat_comm -ih 2>/dev/null";
$ipf_out="$ipfstat_comm -oh 2>/dev/null";
$ipf_acc_in="$ipfstat_comm -aih 2>/dev/null";
$ipf_acc_out="$ipfstat_comm -aoh 2>/dev/null";

$OID=$ARGV[0];
$IPF_OID='.1.3.6.1.4.1.2021.13.2';
$IPF_OID_NO_DOTS='\.1\.3\.6\.1\.4\.1\.2021\.13\.2';

# exit if OID is not one of IPF-MIB's
exit if $OID !~ /^$IPF_OID_NO_DOTS(\D|$)/;

# get table, entry, column and row numbers
$tecr = $OID;
$tecr =~ s/^$IPF_OID_NO_DOTS(\D|$)//;
($table, $entry, $col, $row, $rest) = split(/\./, $tecr);

# parse 'get' request
if($g) {
	# exit if OID is wrong specified
	if(!defined $table or !defined $entry or !defined $col or !defined $row or defined $rest) {
		print "[1] NO-SUCH NAME\n" if $d;
		exit;
	}

	# get the OID's value
	$value = &get_value($table, $entry, $col, $row);
	print "value=$value\n" if $d;

	# exit if OID does not exist
	print "[2] NO-SUCH NAME\n" if $d and !defined $value;
	exit if !defined $value;

	# set ObjectID and reply with response
	$tec = "$table.$entry.$col";
	$ObjectID = "${IPF_OID}.${tec}.${row}";
	&response;
}

# parse 'get-next' request
if($n) {
	# set values if 0 or unspecified
	$table = 1, $a = 1 if !$table or !defined $table;
	$entry = 1, $a = 1 if !$entry or !defined $entry;
	$col = 1, $a = 1 if !$col or !defined $col;
	$row = 1, $a = 1 if !$row or !defined $row;

	if($a) {
		# get the OID's value
		$value = &get_value($table, $entry, $col, $row);
		print "value=$value\n" if $d;

		# set ObjectID and reply with response
		$tec = "$table.$entry.$col";
		$ObjectID = "${IPF_OID}.${tec}.${row}";
		&response;
	}

	# get next OID's value
	$row++;
	$value = &get_value($table, $entry, $col, $row);

	# choose new table/column if rows exceeded
	if(!defined $value) {
		$tec = "$table.$entry.$col";
		$tec = $next{$tec} if !$a;
		$table = $tec;
		$entry = $tec;
		$col = $tec;
		$table =~ s/\.\d\.\d$//;
		$entry =~ s/^\d\.(\d)\.\d$/$1/;
		$col =~ s/^\d\.\d\.//;
		$row = 1;

		# get the OID's value
		$value = &get_value($table, $entry, $col, $row);
		print "value=$value\n" if $d;
	}

	# set ObjectID and reply with response
	$tec = "$table.$entry.$col";
	$ObjectID = "${IPF_OID}.${tec}.${row}";
	&response;
}

##############################################################################

# fetch values from 'ipfInTable' and 'ipfOutTable' tables
sub fetch_hits_n_rules {
	local($row, $col, $ipf_output) = @_;
	local($asdf, $i, @ipf_lines, $length);

	# create an entry if no rule exists
	$ipf_output = "0 empty list for ipfilter" if !$ipf_output;

	@ipf_lines = split("\n", $ipf_output);
	$length = $#ipf_lines + 1;

	for($i = 1; $i < $length + 1; $i++) {
		$hits{$i} = $ipf_lines[$i-1];
		$hits{$i} =~ s/^(\d+).*$/$1/;
		$rule{$i} = $ipf_lines[$i-1];
		$rule{$i} =~ s/^\d+ //;
		if($i == $row) {
			return $i if $col == 1;
			return $rule{$i} if $col == 2;
			return $hits{$i} if $col == 3;
		}
	}
	# return undefined value
	undef $asdf;
	return $asdf;
}

# fetch values from 'ipfAccInTable' and 'ipfAccOutTable' tables
sub fetch_hits_bytes_n_rules {
	local($row, $col, $ipf_output) = @_;
	local($asdf, $i, @ipf_lines, $length);

	# create an entry if no rule exists
	$ipf_output = "0 0 empty list for ipacct" if !$ipf_output;

	@ipf_lines = split("\n", $ipf_output);
	$length = $#ipf_lines + 1;

	for($i = 1; $i < $length + 1; $i++) {
		$hits{$i} = $ipf_lines[$i-1];
		$hits{$i} =~ s/^(\d+) .*$/$1/;
		$bytes{$i} = $ipf_lines[$i-1];
		$bytes{$i} =~ s/^\d+ (\d+) .*/$1/;
		$rule{$i} = $ipf_lines[$i-1];
		$rule{$i} =~ s/^\d+ \d+ //;
		if($i == $row) {
			return $i if $col == 1;
			return $rule{$i} if $col == 2;
			return $hits{$i} if $col == 3;
			return $bytes{$i} if $col == 4;
		}
	}
	# return undefined value
	undef $asdf;
	return $asdf;
}

# get the values from ipfilter's tables
sub get_value {
	local($table, $entry, $col, $row) = @_;

	if($table == 1) {
		# fetch ipfInTable data
		$ipf_output = `$ipf_in`;
		$value = &fetch_hits_n_rules($row, $col, $ipf_output);
	} elsif($table == 2) {
		# fetch ipfOutTable data
		$ipf_output = `$ipf_out`;
		$value = &fetch_hits_n_rules($row, $col, $ipf_output);
	} elsif($table == 3) {
		# fetch ipfAccInTable data
		$ipf_output = `$ipf_acc_in`;
		$value = &fetch_hits_bytes_n_rules($row, $col, $ipf_output);
	} elsif($table == 4) {
		# fetch ipfAccOutTable data
		$ipf_output = `$ipf_acc_out`;
		$value = &fetch_hits_bytes_n_rules($row, $col, $ipf_output);
	}
	return $value;
}

# generate response to 'get' or 'get-next' request
sub response {
	# print ObjectID, its type and the value
	if(defined $ObjectID and defined $type{$tec} and defined $value) {
		print "$ObjectID\n";
		print "$type{$tec}\n";
		print "$value\n";
	}
	exit;
}
