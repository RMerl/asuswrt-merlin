#!/usr/local/bin/perl
# 
#@(#) smb-wall.pl Description:
#@(#) A perl script which allows you to announce whatever you choose to
#@(#) every PC client currently connected to a Samba Server...
#@(#) ...using "smbclient -M" message to winpopup service.
#@(#) Default usage is to message every connected PC.
#@(#) Alternate usage is to message every pc on the argument list.
#@(#)  Hacked up by Keith Farrar <farrar@parc.xerox.com>
#
# Cleanup and corrections by
# Michal Jaegermann <michal@ellpspace.math.ualberta.ca>
# Message to send can be now also fed (quietly) from stdin; a pipe will do.
#=============================================================================

$smbstatus = "/usr/local/bin/smbstatus";
$smbshout = "/usr/local/bin/smbclient -M";

if (@ARGV) {
    @clients = @ARGV;
    undef @ARGV;
}
else {  # no clients specified explicitly
    open(PCLIST, "$smbstatus |") || die "$smbstatus failed!.\n$!\n";
    while(<PCLIST>) {
	last if /^Locked files:/;
	split(' ', $_, 6);
        # do not accept this line if less then six fields
        next unless $_[5];
        # if you have A LOT of clients you may speed things up by
        # checking pid - no need to look further if this pid was already
        # seen;  left as an exercise :-)
        $client = $_[4];
	next unless $client =~ /^\w+\./;       # expect 'dot' in a client name
	next if grep($_ eq $client, @clients); # we want this name once
	push(@clients, $client);
    }
    close(PCLIST);
}

if (-t) {
    print <<'EOT';

Enter message for Samba clients of this host
(terminated with single '.' or end of file):
EOT

    while (<>) {
	last if /^\.$/;
	push(@message, $_);
    }
}
else { # keep quiet and read message from stdin
    @message = <>;
}

foreach(@clients) {
##    print "To $_:\n";
    if (open(SENDMSG,"|$smbshout $_")) {
	print SENDMSG @message;
	close(SENDMSG);
    }
    else {
	warn "Cannot notify $_ with $smbshout:\n$!\n";
    }
}

exit 0;

