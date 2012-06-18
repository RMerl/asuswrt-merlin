#!/usr/bin/perl

# edit this to the printer hostname
$them = 'ken';
$port = 9101;

open(STDIN, "$ARGV[0]") if $#ARGV >= 0;

use Socket;
#use Sys::Hostname;

#$hostname = hostname;

($name, $aliases, $proto) = getprotobyname('tcp');
($name, $aliases, $port) = getservbyname($port, 'tcp')
	unless $port =~ /^\d+$/;

#$thisaddr = inet_aton($hostname);
#defined($thisaddr) or &errexit("inet_aton: cannot resolve $hostname\n");

$thataddr = inet_aton($them);
defined($thataddr) or &errexit("inet_aton: cannot resolve $them\n");

socket(S, PF_INET, SOCK_STREAM, $proto) or &errexit("socket: $!\n");

#$this = sockaddr_in(0, $thisaddr);
#bind(S, $this) || &errexit("bind: $!\n");

$that = sockaddr_in($port, $thataddr);
connect(S, $that) || &errexit("connect: $!\n");

select(S); $| = 1; select(STDOUT);

$buffer = '';
while (1)
{
	$rin = '';
	vec($rin, fileno(S), 1) = 1;
	$nfound = select($rout=$rin, $wout=$rin, undef, undef);
	if (vec($rout, fileno(S), 1)) {
		print STDERR "$buffer\n" if
			defined($nread = sysread(S, $buffer, 8192));
	}
	if (vec($wout, fileno(S), 1)) {
		$nread = read(STDIN, $buffer, 8192);
		last if $nread == 0;
		&errexit("write: $!\n") unless
			defined($written = syswrite(S,$buffer,$nread));
	}
}
close(S);
exit 0;

sub errexit
{
	print STDERR @_;
	exit 2;
}
