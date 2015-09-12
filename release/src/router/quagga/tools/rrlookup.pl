#!/usr/bin/env perl
##
## Read BGPd logfile and lookup RR's whois database.
##
##   Copyright (c) 1997 Kunihiro Ishiguro
##
use Socket;

## Configuration variables
$whois_host = "whois.jpix.ad.jp";

#$mail_address = "toshio\@iri.co.jp";
$mail_address = "kunihiro\@zebra.org";
$mailer = "/usr/sbin/sendmail -oi";

#$logfile = "/usr/local/sbin/logfile"
$logfile = "logfile";
$lookuplog = "lookuplog";

## mail routine
{
    local ($prefix, $origin);

    open (LOG, $logfile) || die "can't open $logfile";
    open (LOOKUP, ">$lookuplog") || die "can't open $lookuplog";
    
    for (;;) {
    while (<LOG>) {
        if (/Update\S+ ([\d\.\/]+) .* (\d+) [ie\?]/) {
            $prefix = $1;
            $origin = $2;
            $ret = &whois_check ($prefix, $origin);
            if ($ret) {
                print LOOKUP "$prefix AS$origin : Check OK\n";
            } else {
                print LOOKUP "$prefix AS$origin : Error\n";
            }
#        fflush (LOOKUP);
        }
    }
    sleep (3);
    }
}

sub whois_check
{
    local ($prefix, $origin) = @_;
    local ($rr_prefix, $rr_origin) = ();
    local (@result);

    $origin = "AS" . $origin;

#    print "$prefix $origin\n";

    @result = &whois ($prefix);

    foreach (@result) {
        if (/^route:.*\s([\d\.\/]+)$/) {
            $rr_prefix = $1;
        }
        if (/^origin:.*\s(AS[\d]+)$/) {
            $rr_origin = $1;

            if ($prefix eq $rr_prefix and $origin eq $rr_origin) {
                return 1;
            }
        }
    }
    alarm_mail ($prefix, $origin, @result);
    return 0;
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
