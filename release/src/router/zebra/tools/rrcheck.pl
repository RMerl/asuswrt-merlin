#! /bin/perl
##
## Read BGPd logfile and lookup RR's whois database.
##
##   Copyright (c) 1997 Kunihiro Ishiguro
##
use Socket;

## Configuration variables
$whois_host = "whois.jpix.ad.jp";

#$logfile = "/usr/local/sbin/logfile"
$logfile = shift || die "Please specify filename";

## mail routine
{
    local ($prefix, $origin);

    open (LOG, $logfile) || die "can't open $logfile";
    
    $index = '';
    while ($index) {
	$index = <LOG>;
	if ($index =~ /[bgpd]/) {
	    break;
	}
    }

    while (<LOG>) {
	if (/([\d\.\/]+)\s+([\d\.]+)\s+(\d+)\s+(\d+)\s+([\d ]+)\s+[ie\?]/) {
	    $prefix = $1;
	    $nexthop = $2;
	    $med = $3;
	    $dummy = $4;
	    $aspath = $5;
	    ($origin) = ($aspath =~ /([\d]+)$/);

	    print "$nexthop [$origin] $prefix $aspath ";

	    $ret = &whois_check ($prefix, $origin);
	    if ($ret == 0) {
		print "Check OK\n";
	    } elsif ($ret == 1){
		print "AS orgin mismatch\n";
	    } else {
		print "prefix doesn't exist \n";
	    }
	}
    }
}

sub whois_check
{
    local ($prefix, $origin) = @_;
    local ($rr_prefix, $rr_origin) = ();
    local (@result);

    $origin = "AS" . $origin;

    @result = &whois ($prefix);

    $prefix_match = 0;
    foreach (@result) {
        if (/^route:.*\s([\d\.\/]+)$/) {
            $rr_prefix = $1;
        }
        if (/^origin:.*\s(AS[\d]+)$/) {
            $rr_origin = $1;

            if ($prefix eq $rr_prefix and $origin eq $rr_origin) {
                return 0;
            } elsif ($prefix eq $rr_prefix) {
		$prefix_match = 1;
	    }
        }
    }
#    alarm_mail ($prefix, $origin, @result);
    if ($prefix_match) {
	return 1;
    } else {
	return 2;
    }
}

## get port of whois
sub get_whois_port 
{
    local ($name, $aliases, $port, $proto) = getservbyname ("whois", "tcp");
    return ($port, $proto);
}

## whois lookup
sub whois 
{
    local ($query) = @_;
    local ($port, $proto) = &get_whois_port;
    local (@result);

    if ($whois_host=~ /^\s*\d+\.\d+\.\d+\.\d+\s*$/) {
       $address = pack ("C4",split(/\./,$host));
    } else {
       $address = (gethostbyname ($whois_host))[4];
    }

    socket (SOCKET, PF_INET, SOCK_STREAM, $proto);

    if (connect (SOCKET, sockaddr_in ($port, $address))) {
        local ($oldhandle) = select (SOCKET);
        $| = 1;
        select($oldhandle);

        print SOCKET "$query\r\n";

        @result = <SOCKET>;
        return @result;
    }
}

##
sub alarm_mail 
{
    local ($prefix, $origin, @result) = @_;

    open (MAIL, "|$mailer -t $mail_address") || die "can't open $mailer";
    
    print MAIL "From: root\@rr1.jpix.ad.jp\n";
    print MAIL "Subject: RR $origin $prefix\n";
    print MAIL "MIME-Version: 1.0\n";
    print MAIL "Content-Type: text/plain; charset=us-ascii \n\n";
    print MAIL "RR Lookup Error Report\n";
    print MAIL "======================\n";
    print MAIL "Announced route : $prefix from $origin\n\n";
    print MAIL "@result";
    close MAIL;
}
