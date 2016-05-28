#!/usr/bin/perl -w

# Copyright (C) Guenther Deschner <gd@samba.org> 2006

use strict;
use IO::Socket;
use Convert::ASN1 qw(:debug);
use Getopt::Long;

# TODO: timeout handling, user CLDAP query

##################################

my $server = "";
my $domain = "";
my $host   = "";

##################################

my (
	$opt_debug,
	$opt_domain,
	$opt_help,
	$opt_host,
	$opt_server,
);

my %cldap_flags = (
	ADS_PDC 		=> 0x00000001, # DC is PDC
	ADS_GC 			=> 0x00000004, # DC is a GC of forest
	ADS_LDAP		=> 0x00000008, # DC is an LDAP server
	ADS_DS			=> 0x00000010, # DC supports DS
	ADS_KDC			=> 0x00000020, # DC is running KDC
	ADS_TIMESERV		=> 0x00000040, # DC is running time services
	ADS_CLOSEST		=> 0x00000080, # DC is closest to client
	ADS_WRITABLE		=> 0x00000100, # DC has writable DS
	ADS_GOOD_TIMESERV	=> 0x00000200, # DC has hardware clock (and running time) 
	ADS_NDNC		=> 0x00000400, # DomainName is non-domain NC serviced by LDAP server
);

my %cldap_samlogon_types = (
	SAMLOGON_AD_UNK_R	=> 23,
	SAMLOGON_AD_R		=> 25,
);

my $MAX_DNS_LABEL = 255 + 1;

my %cldap_netlogon_reply = (
	type 			=> 0,
	flags			=> 0x0,
	guid			=> 0,
	forest			=> undef,
	domain			=> undef,
	hostname 		=> undef,
	netbios_domain		=> undef,
	netbios_hostname	=> undef,
	unk			=> undef,
	user_name		=> undef,
	server_site_name	=> undef,
	client_site_name	=> undef,
	version			=> 0,
	lmnt_token		=> 0x0,
	lm20_token		=> 0x0,
);

sub usage { 
	print "usage: $0 [--domain|-d domain] [--help] [--host|-h host] [--server|-s server]\n\n";
}

sub connect_cldap ($) {

	my $server = shift || return undef;
	
	return IO::Socket::INET->new(
		PeerAddr	=> $server,
		PeerPort	=> 389,
		Proto		=> 'udp',
		Type		=> SOCK_DGRAM,
		Timeout		=> 10,
	);
}

sub send_cldap_netlogon ($$$$) {

	my ($sock, $domain, $host, $ntver) = @_;

	my $asn_cldap_req = Convert::ASN1->new;

	$asn_cldap_req->prepare(q<

		SEQUENCE {
			msgid INTEGER,
			[APPLICATION 3] SEQUENCE {
				basedn OCTET STRING,
				scope ENUMERATED,
				dereference ENUMERATED,
				sizelimit INTEGER,
				timelimit INTEGER,
				attronly BOOLEAN,
				[CONTEXT 0] SEQUENCE {
					[CONTEXT 3] SEQUENCE {
						dnsdom_attr OCTET STRING,
						dnsdom_val  OCTET STRING
					}
					[CONTEXT 3] SEQUENCE {
						host_attr OCTET STRING,
						host_val  OCTET STRING
					}
					[CONTEXT 3] SEQUENCE {
						ntver_attr OCTET STRING,
						ntver_val  OCTET STRING
					}
				}
				SEQUENCE {
					netlogon OCTET STRING
				}
			}
		}
	>);

	my $pdu_req = $asn_cldap_req->encode( 
				msgid => 0,
				basedn => "", 
				scope => 0,
				dereference => 0,
				sizelimit => 0,
				timelimit => 0,
				attronly => 0,
				dnsdom_attr => $domain ? 'DnsDomain' : "",
				dnsdom_val => $domain ? $domain : "",
				host_attr => 'Host',
				host_val => $host,
				ntver_attr => 'NtVer',
				ntver_val => $ntver,
				netlogon => 'NetLogon',
				) || die "failed to encode pdu: $@";

	if ($opt_debug) {
		print"------------\n";
		asn_dump($pdu_req);
		print"------------\n";
	}

	return $sock->send($pdu_req) || die "no send: $@";
}

# from source/libads/cldap.c :
#
#/*
#  These seem to be strings as described in RFC1035 4.1.4 and can be:
#
#   - a sequence of labels ending in a zero octet
#   - a pointer
#   - a sequence of labels ending with a pointer
#
#  A label is a byte where the first two bits must be zero and the remaining
#  bits represent the length of the label followed by the label itself.
#  Therefore, the length of a label is at max 64 bytes.  Under RFC1035, a
#  sequence of labels cannot exceed 255 bytes.
#
#  A pointer consists of a 14 bit offset from the beginning of the data.
#
#  struct ptr {
#    unsigned ident:2; // must be 11
#    unsigned offset:14; // from the beginning of data
#  };
#
#  This is used as a method to compress the packet by eliminated duplicate
#  domain components.  Since a UDP packet should probably be < 512 bytes and a
#  DNS name can be up to 255 bytes, this actually makes a lot of sense.
#*/

sub pull_netlogon_string (\$$$) {
	
	my ($ret, $ptr, $str) = @_;

	my $pos = $ptr;

	my $followed_ptr = 0;
	my $ret_len = 0;

	my $retp = pack("x$MAX_DNS_LABEL");

	do {
	
		$ptr = unpack("c", substr($str, $pos, 1));
		$pos++;

		if (($ptr & 0xc0) == 0xc0) {

			my $len;

			if (!$followed_ptr) {
				$ret_len += 2;
				$followed_ptr = 1;
			}

			my $tmp0 = $ptr; #unpack("c", substr($str, $pos-1, 1));
			my $tmp1 = unpack("c", substr($str, $pos, 1));

			if ($opt_debug) {
				printf("tmp0: 0x%x\n", $tmp0);
				printf("tmp1: 0x%x\n", $tmp1);
			}

			$len = (($tmp0 & 0x3f) << 8) | $tmp1;
			$ptr = unpack("c", substr($str, $len, 1));
			$pos = $len;

		} elsif ($ptr) {

			my $len = scalar $ptr;

			if ($len + 1 > $MAX_DNS_LABEL) {
				warn("invalid string size: %d", $len + 1);
				return 0;
			}

			$ptr = unpack("a*", substr($str, $pos, $len));

			$retp = sprintf("%s%s\.", $retp, $ptr);

			$pos += $len;
			if (!$followed_ptr) {
				$ret_len += $len + 1;
			}
		}

	} while ($ptr);

	$retp =~ s/\.$//; #ugly hack...

	$$ret = $retp;

	return $followed_ptr ? $ret_len : $ret_len + 1;
}

sub dump_cldap_flags ($) {

	my $flags = shift || return;
	printf("Flags:\n".
		 "\tIs a PDC:                                   %s\n".
		 "\tIs a GC of the forest:                      %s\n".
		 "\tIs an LDAP server:                          %s\n".
		 "\tSupports DS:                                %s\n".
		 "\tIs running a KDC:                           %s\n".
		 "\tIs running time services:                   %s\n".
		 "\tIs the closest DC:                          %s\n".
		 "\tIs writable:                                %s\n".
		 "\tHas a hardware clock:                       %s\n".
		 "\tIs a non-domain NC serviced by LDAP server: %s\n",
		 ($flags & $cldap_flags{ADS_PDC}) ? "yes" : "no",
		 ($flags & $cldap_flags{ADS_GC}) ? "yes" : "no",
		 ($flags & $cldap_flags{ADS_LDAP}) ? "yes" : "no",
		 ($flags & $cldap_flags{ADS_DS}) ? "yes" : "no",
		 ($flags & $cldap_flags{ADS_KDC}) ? "yes" : "no",
		 ($flags & $cldap_flags{ADS_TIMESERV}) ? "yes" : "no",
		 ($flags & $cldap_flags{ADS_CLOSEST}) ? "yes" : "no",
		 ($flags & $cldap_flags{ADS_WRITABLE}) ? "yes" : "no",
		 ($flags & $cldap_flags{ADS_GOOD_TIMESERV}) ? "yes" : "no",
		 ($flags & $cldap_flags{ADS_NDNC}) ? "yes" : "no");
}

sub guid_to_string ($) {

	my $guid = shift || return undef;
	if ((my $len = length $guid) != 16) {
		printf("invalid length: %d\n", $len);
		return undef;
	}
	my $string = sprintf "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X", 
		unpack("I", $guid), 
		unpack("S", substr($guid, 4, 2)), 
		unpack("S", substr($guid, 6, 2)),
		unpack("C", substr($guid, 8, 1)),
		unpack("C", substr($guid, 9, 1)),
		unpack("C", substr($guid, 10, 1)),
		unpack("C", substr($guid, 11, 1)),
		unpack("C", substr($guid, 12, 1)),
		unpack("C", substr($guid, 13, 1)),
		unpack("C", substr($guid, 14, 1)),
		unpack("C", substr($guid, 15, 1)); 
	return lc($string);
}

sub recv_cldap_netlogon ($\$) {

	my ($sock, $return_string) = @_;
	my ($ret, $pdu_out);

	$ret = $sock->recv($pdu_out, 8192) || die "failed to read from socket: $@";
	#$ret = sysread($sock, $pdu_out, 8192);

	if ($opt_debug) {
		print"------------\n";
		asn_dump($pdu_out);
		print"------------\n";
	}

	my $asn_cldap_rep = Convert::ASN1->new;
	my $asn_cldap_rep_fail = Convert::ASN1->new;

	$asn_cldap_rep->prepare(q<
		SEQUENCE {
			msgid INTEGER,
			[APPLICATION 4] SEQUENCE {
				dn OCTET STRING,
				SEQUENCE {
					SEQUENCE {
						attr OCTET STRING,
						SET {
							val OCTET STRING
						}
					}
				}
			}
		}
		SEQUENCE {
			msgid2 INTEGER,
			[APPLICATION 5] SEQUENCE {
				error_code ENUMERATED,
				matched_dn OCTET STRING,
				error_message OCTET STRING
			}
		}
	>);

	$asn_cldap_rep_fail->prepare(q<
		SEQUENCE {
			msgid2 INTEGER,
			[APPLICATION 5] SEQUENCE {
				error_code ENUMERATED,
				matched_dn OCTET STRING,
				error_message OCTET STRING
			}
		}
	>);

	my $asn1_rep =  $asn_cldap_rep->decode($pdu_out) || 
			$asn_cldap_rep_fail->decode($pdu_out) || 
			die "failed to decode pdu: $@";

	if ($asn1_rep->{'error_code'} == 0) {
		$$return_string = $asn1_rep->{'val'};
	} 

	return $ret;
}

sub parse_cldap_reply ($) {

	my $str = shift || return undef;
        my %hash;
	my $p = 0;

	$hash{type} 	= unpack("L", substr($str, $p, 4)); $p += 4;
	$hash{flags} 	= unpack("L", substr($str, $p, 4)); $p += 4;
	$hash{guid} 	= unpack("a16", substr($str, $p, 16)); $p += 16;

	$p += pull_netlogon_string($hash{forest}, $p, $str);
	$p += pull_netlogon_string($hash{domain}, $p, $str);
	$p += pull_netlogon_string($hash{hostname}, $p, $str);
	$p += pull_netlogon_string($hash{netbios_domain}, $p, $str);
	$p += pull_netlogon_string($hash{netbios_hostname}, $p, $str);
	$p += pull_netlogon_string($hash{unk}, $p, $str);

	if ($hash{type} == $cldap_samlogon_types{SAMLOGON_AD_R}) {
		$p += pull_netlogon_string($hash{user_name}, $p, $str);
	} else {
		$hash{user_name} = "";
	}

	$p += pull_netlogon_string($hash{server_site_name}, $p, $str);
	$p += pull_netlogon_string($hash{client_site_name}, $p, $str);

	$hash{version} 		= unpack("L", substr($str, $p, 4)); $p += 4;
	$hash{lmnt_token} 	= unpack("S", substr($str, $p, 2)); $p += 2;
	$hash{lm20_token} 	= unpack("S", substr($str, $p, 2)); $p += 2;

	return %hash;
}

sub display_cldap_reply {

	my $server = shift;
        my (%hash) = @_;

	my ($name,$aliases,$addrtype,$length,@addrs) = gethostbyname($server);

	printf("Information for Domain Controller: %s\n\n", $name);

	printf("Response Type: ");
	if ($hash{type} == $cldap_samlogon_types{SAMLOGON_AD_R}) {
		printf("SAMLOGON_USER\n");
	} elsif ($hash{type} == $cldap_samlogon_types{SAMLOGON_AD_UNK_R}) {
		printf("SAMLOGON\n");
	} else {
		printf("unknown type 0x%x, please report\n", $hash{type});
	}

	# guid
	printf("GUID: %s\n", guid_to_string($hash{guid}));

	# flags
	dump_cldap_flags($hash{flags});

	# strings
	printf("Forest:\t\t\t%s\n", $hash{forest});
	printf("Domain:\t\t\t%s\n", $hash{domain});
	printf("Domain Controller:\t%s\n", $hash{hostname});

	printf("Pre-Win2k Domain:\t%s\n", $hash{netbios_domain});
	printf("Pre-Win2k Hostname:\t%s\n", $hash{netbios_hostname});

	if ($hash{unk}) { 
		printf("Unk:\t\t\t%s\n", $hash{unk}); 
	}
	if ($hash{user_name}) { 
		printf("User name:\t%s\n", $hash{user_name}); 
	}

	printf("Server Site Name:\t%s\n", $hash{server_site_name});
	printf("Client Site Name:\t%s\n", $hash{client_site_name});

	# some more int
	printf("NT Version:\t\t%d\n", $hash{version});
	printf("LMNT Token:\t\t%.2x\n", $hash{lmnt_token});
	printf("LM20 Token:\t\t%.2x\n", $hash{lm20_token});
}

sub main() {

	my ($ret, $sock, $reply);

	GetOptions(
		'debug'		=> \$opt_debug,
		'domain|d=s'	=> \$opt_domain,
		'help'		=> \$opt_help,
		'host|h=s'	=> \$opt_host,
		'server|s=s'	=> \$opt_server,
	);

	$server = $server || $opt_server;
	$domain = $domain || $opt_domain || undef;
	$host = $host || $opt_host;
	if (!$host) {
		$host = `/bin/hostname`;
		chomp($host);
	}

	if (!$server || !$host || $opt_help) {
		usage();
		exit 1;
	}

	my $ntver = sprintf("%c%c%c%c", 6,0,0,0);

	$sock = connect_cldap($server);
	if (!$sock) {
		die("could not connect to $server");
	}

	$ret = send_cldap_netlogon($sock, $domain, $host, $ntver);
	if (!$ret) {
		close($sock);
		die("failed to send CLDAP request to $server");
	}

	$ret = recv_cldap_netlogon($sock, $reply);
	if (!$ret) {
		close($sock);
		die("failed to receive CLDAP reply from $server");
	}
	close($sock);

	if (!$reply) {
		printf("no 'NetLogon' attribute received\n");
		exit 0;
	}

	%cldap_netlogon_reply = parse_cldap_reply($reply);
	if (!%cldap_netlogon_reply) {
		die("failed to parse CLDAP reply from $server");
	}

	display_cldap_reply($server, %cldap_netlogon_reply);

	exit 0;
}

main();
