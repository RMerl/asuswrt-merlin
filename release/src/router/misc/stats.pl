#!/usr/bin/perl
#
# CGI.pm web script to handle stats POSTs
#
# $Copyright Open Broadcom Corporation$
#
# $Id: stats.pl,v 1.5 2003-10-16 21:13:40 mthawani Exp $
#

use CGI;
use CGI qw/escape unescape/;
use CGI qw/:standard :html3/;

$query = new CGI;
$savedir = "/tmp";

print $query->header;
print $query->start_html("Broadcom Internal Board Statistics");
print $query->h1("Broadcom Internal Board Statistics");

# Here's where we take action on the previous request
&save_parameters($query)              if $query->param('action') eq 'save';
$query = &restore_parameters($query)  if $query->param('action') eq 'restore';

# Here's where we create the form
print $query->startform;
print "Board type: ";
print $query->popup_menu(-name=>'boardtype',
			 -values=>['',
				   'bcm94710dev',
				   'bcm94710r4']);

print $query->p;
if (opendir(DIR, $savedir)) {
    $pattern = $query->param('boardtype');
    @boards = '';
    foreach (readdir(DIR)) {
	/^$pattern\_(\d+)/ && push(@boards, $1);
    }
    closedir(DIR);
    if ($pattern && @boards) {
	print "Board number: ";
	print $query->popup_menu(-name=>'boardnum',
				 -values=>\@boards);
    } else {
	print "No boards";
    }
} else {
    print "No boards";
}
   
print $query->p;
if (opendir(DIR, $savedir.&clean_name($query))) {
    @reports = grep(!/^\./, readdir(DIR));
    @labels{@reports} = map($unused = localtime($_), @reports);
    closedir(DIR);
    if (@reports) {
	print "Report: ";
	print $query->popup_menu(-name=>'savefile',
				 -values=>\@reports,
				 -labels=>\%labels);
    } else {
	print "No reports";
    }
} else {
    print "No reports";
}

print $query->p;
print $query->submit('action', 'restore');
print $query->endform;
print $query->hr;

foreach (split('\n', $query->param('/proc/net/dev'))) {
    (/^Inter/ || /^ face/) && next;
    ($name, $rest) = /^\s*(\S+):(.*)$/;
    ($rx_bytes{$name}, $rx_packets{$name}, $rx_errs{$name}, $rx_drop{$name},
     $rx_fifo{$name}, $rx_frame{$name}, $rx_compressed{$name}, $rx_multicast{$name},
     $tx_bytes{$name}, $tx_packets{$name}, $tx_errs{$name}, $tx_drop{$name},
     $tx_fifo{$name}, $tx_colls{$name}, $tx_carrier{$name}, $tx_compressed{$name}) =
	 $rest =~ /\s*(\d+)\s*(\d+)\s*(\d+)\s*(\d+)\s*(\d+)\s*(\d+)\s*(\d+)\s*(\d+)\s*(\d+)\s*(\d+)\s*(\d+)\s*(\d+)\s*(\d+)\s*(\d+)\s*(\d+)\s*(\d+)/;
}

print table({-border=>undef},
	    Tr({-align=>LEFT,-valign=>TOP},
	       [th('Board type').td($query->param('boardtype')),
		th('Board number').td($query->param('boardnum')),
		th('CPU clock').td($query->param('clkfreq')),
		th('OS name').td($query->param('os_name')),
		th('OS version').td($query->param('os_version')),
		th('Uptime').td($query->param('uptime')),

		th('WAN MAC address').td($query->param('wan_hwaddr')),
		th('WAN IP address').td($query->param('wan_ipaddr')),
		th('WAN protocol').td($query->param('wan_proto')),
		th('WAN receive bytes').td($rx_bytes{$query->param('wan_ifname')}),
		th('WAN receive packets').td($rx_packets{$query->param('wan_ifname')}),
		th('WAN receive errors').td($rx_errs{$query->param('wan_ifname')}),
		th('WAN receive drops').td($rx_drop{$query->param('wan_ifname')}),
		th('WAN transmit bytes').td($tx_bytes{$query->param('wan_ifname')}),
		th('WAN transmit packets').td($tx_packets{$query->param('wan_ifname')}),
		th('WAN transmit errors').td($tx_errs{$query->param('wan_ifname')}),
		th('WAN transmit drops').td($tx_drop{$query->param('wan_ifname')}),

		th('LAN MAC address').td($query->param('lan_hwaddr')),
		th('LAN IP address').td($query->param('lan_ipaddr')),
		th('LAN protocol').td($query->param('lan_proto')),
		th('LAN receive bytes').td($rx_bytes{$query->param('lan_ifname')}),
		th('LAN receive packets').td($rx_packets{$query->param('lan_ifname')}),
		th('LAN receive errors').td($rx_errs{$query->param('lan_ifname')}),
		th('LAN receive drops').td($rx_drop{$query->param('lan_ifname')}),
		th('LAN transmit bytes').td($tx_bytes{$query->param('lan_ifname')}),
		th('LAN transmit packets').td($tx_packets{$query->param('lan_ifname')}),
		th('LAN transmit errors').td($tx_errs{$query->param('lan_ifname')}),
		th('LAN transmit drops').td($tx_drop{$query->param('lan_ifname')}),
		]));

# Here we print out a bit at the end
print $query->end_html;
    
sub save_parameters {
    local($query) = @_;
    local($dir) = $savedir.&clean_name($query);
    local($filename) = time;

    opendir(DIRHANDLE, $dir) ? closedir(DIRHANDLE) : mkdir($dir, 0755);
    if (open(FILE, ">$dir/$filename")) {
	$query->param('savefile', $filename);
	$query->save(FILE);
	close FILE;
	# print "<STRONG>State has been saved to file $dir/$filename</STRONG>\n";
    } else {
	# print "<STRONG>Error:</STRONG> couldn't write to file $dir/$filename: $!\n";
    }
}

sub restore_parameters {
    local($query) = @_;
    local($dir) = $savedir.&clean_name($query);
    local($filename) = $query->param('savefile');

    if (!($filename eq "") &&
	open(FILE, "$dir/$filename")) {
	$query = new CGI(FILE);  # Throw out the old query, replace it with a new one
	close FILE;
	# print "<STRONG>State has been restored from file $filename</STRONG>\n";
    } else {
	# print "<STRONG>Error:</STRONG> couldn't restore file $dir/$filename: $!\n";
    }
    return $query;
}
    

# Very important subroutine -- get rid of all the naughty
# metacharacters from the file name. If there are, we
# complain bitterly and die.
sub clean_name {
   local($query) = @_;
   local($name) = $query->param('boardtype')."_".$query->param('boardnum');
   unless ($name=~/^[\w\._-]+$/) {
      # print "<STRONG>$name has naughty characters.  Only ";
      # print "alphanumerics are allowed.  You can't use absolute names.</STRONG>";
      die "Attempt to use naughty characters";
   }
   return $name;
}
