#!/usr/bin/perl

system("rm -rf /tmp/.snmp1");
system("rm -rf /tmp/.snmp2");

system("cp net-snmp-cert ~/bin");

$str = "\ngenca (in -C /tmp/.snmp1) : ca-snmp\n\n";
print("$str");
die("$str\n") if system("net-snmp-cert genca -I -C /tmp/.snmp1 --cn ca-snmp --email ca\@ca.com --host host.a.b.com  --san DNS:ca.a.b.com --san EMAIL:ca\@ca.com");

print "\nusing -C /tmp/.snmp2 for all following tests\n";
$str = "\ngenca: ca-snmp\n\n";
print("$str");
die("$str\n") if system("net-snmp-cert genca -I -C /tmp/.snmp2 --cn ca-snmp --email ca\@ca.com --host host.a.b.com  --san DNS:ca.a.b.com --san EMAIL:ca\@ca.com");

$str = "\ngenca: ca-snmp-2 (signed w/ ca-snmp)\n\n";
print("$str");
die("$str\n") if system("net-snmp-cert genca -I -C /tmp/.snmp2 --with-ca ca-snmp --cn ca-snmp-2 --email ca2\@ca.com --host host2.a.b.com  --san DNS:ca2.a.b.com --san EMAIL:ca2\@ca.com");

$str = "\ngencsr: snmpapp\n\n";
print("$str");
die("$str\n") if system("net-snmp-cert gencsr -I -C /tmp/.snmp2 -t snmpapp --cn 'admin' --email admin@net-snmp.org --host admin-host.net-snmp.org  --san EMAIL:a\@b.com --san IP:1.2.3.4 --san DNS:admin.a.b.org");

$str = "\nsigncsr: snmpapp w/ca-snmp\n\n";
print("$str");
die("died: $str\n") if system("net-snmp-cert signcsr -I -C /tmp/.snmp2 --with-ca ca-snmp --csr snmpapp  --install");

$str = "\nsigncsr: snmpapp w/ca-snmp-2\n\n";
print("$str");
die("$str\n") if system("net-snmp-cert signcsr -I -C /tmp/.snmp2 --with-ca ca-snmp-2 --csr snmpapp --san EMAIL:noinstall\@b.com --san IP:5.6.7.8");

$str = "\ngencert: snmptrapd (self-signed)\n\n";
print("$str");
die("$str\n") if system("net-snmp-cert gencert -I -C /tmp/.snmp2 -t snmptrapd --cn 'NOC' --email 'noc\@net-snmp.org' --host noc-host.net-snmp.org  --san DNS:noc.a.b.org --san 'EMAIL:noc\@net-snmp.org'");

$str = "\ngencert: snmpd (signed w/ ca-snmp-2)\n\n";
print("$str");
die("$str\n") if system("net-snmp-cert gencert -I -C /tmp/.snmp2 -t snmpd --with-ca ca-snmp-2 --email snmpd\@net-snmp.org --host snmpd-host.net-snmp.org  --san DNS:snmpd.a.b.org --san EMAIL:snmpd\@net-snmp.org");

system("cp net-snmp-cert.conf /tmp/.snmp2");

$str = "\ngenca (in -C /tmp/.snmp2 -i CA-identity)\n\n";
print("$str");
die("$str\n") if system("net-snmp-cert genca -I -C /tmp/.snmp2 -i CA-identity");

$str = "\ngencert (in -C /tmp/.snmp2 -i nocadm -t snmp-identity)\n\n";
print("$str");
die("$str\n") if system("net-snmp-cert gencert -I -C /tmp/.snmp2 -t snmp-identity -i nocadm --with-ca CA-identity");


$str = "\nshow CAs\n\n";
print("$str");
die("$str\n") if system("net-snmp-cert -C /tmp/.snmp2 showca --issuer --subject");

$str = "show Certs\n\n";
print("$str");
die("$str\n") if system("net-snmp-cert -C /tmp/.snmp2 showcert --issuer --subject");

$str = "show CAs fingerprint\n\n";
print("$str");
die("$str\n") if system("net-snmp-cert -C /tmp/.snmp2 showca --fingerprint --brief");

$str = "\nshow Certs fingerprint\n\n";
print("$str");
die("$str\n") if system("net-snmp-cert -C /tmp/.snmp2 showcert --fingerprint --brief");
