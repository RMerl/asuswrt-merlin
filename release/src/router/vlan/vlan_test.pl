#!/usr/bin/perl

# For now, this just tests the addition and removal of 1000 VLAN interfaces on eth0

# Arguments:
#  graph  Generate a graph.
#  clean  Remove interfaces.

use strict;

$| = 1;

if ($ARGV[0] eq "graph") {
  my $dev_cnt = 0;
  my $user = 0;
  my $system = 0;
  my $real = 0;
  my $prog = "";

  open(IPNH, ">/tmp/ip_rpt_no_hash.txt") || die("Can't open /tmp/ip_rpt_no_hash.txt\n");
  open(IFCFGNH, ">/tmp/ifconfig_rpt_no_hash.txt") || die("Can't open /tmp/ifconfig_rpt_no_hash.txt\n");
  open(IP, ">/tmp/ip_rpt.txt") || die("Can't open /tmp/ip_rpt.txt\n");
  open(IFCFG, ">/tmp/ifconfig_rpt.txt") || die("Can't open /tmp/ifconfig_rpt.txt\n");

  my $hash = $ARGV[1];
  my $no_hash = $ARGV[2];

  open(IF, "$no_hash");
  while (<IF>) {
    my $ln = $_;
    chomp($ln);

    #print "LINE: -:$ln:-\n";

    if ($ln =~ /Doing ip addr show for (\S+)/) {
      $dev_cnt = $1;
      $prog = "ip";
    }
    elsif ($ln =~ /Doing ifconfig -a for (\S+)/) {
      $dev_cnt = $1;
      $prog = "ifconfig";
    }
    elsif ($ln =~ /^real (\S+)/) {
      $real = $1;
    }
    elsif ($ln =~ /^user (\S+)/) {
      $user = $1;
    }
    elsif ($ln =~ /^sys (\S+)/) {
      $system = $1;
      #print "prog: $prog  $dev_cnt\t$user\t$system\t$real\n";
      if ($prog eq "ip") {
	print IPNH "$dev_cnt\t$user\t$system\t$real\n";
      }
      else {
	print IFCFGNH "$dev_cnt\t$user\t$system\t$real\n";
      }
    }
    else {
      #print "INFO:  Didn't match anything -:$ln:-\n";
    }
  }

  close(IPNH);
  close(IFCFGNH);
  close(IF);

  open(IF, "$hash");
  while (<IF>) {
    my $ln = $_;
    chomp($ln);

    #print "LINE: -:$ln:-\n";

    if ($ln =~ /Doing ip addr show for (\S+)/) {
      $dev_cnt = $1;
      $prog = "ip";
    }
    elsif ($ln =~ /Doing ifconfig -a for (\S+)/) {
      $dev_cnt = $1;
      $prog = "ifconfig";
    }
    elsif ($ln =~ /^real (\S+)/) {
      $real = $1;
    }
    elsif ($ln =~ /^user (\S+)/) {
      $user = $1;
    }
    elsif ($ln =~ /^sys (\S+)/) {
      $system = $1;
      #print "prog: $prog  $dev_cnt\t$user\t$system\t$real\n";
      if ($prog eq "ip") {
	print IP "$dev_cnt\t$user\t$system\t$real\n";
      }
      else {
	print IFCFG "$dev_cnt\t$user\t$system\t$real\n";
      }
    }
    else {
      #print "INFO:  Didn't match anything -:$ln:-\n";
    }
  }

  close(IP);
  close(IFCFG);

  my $plot_cmd = "set title \"ip addr show V/S ifconfig -a\"
set terminal png color
set output \"ip_addr_show.png\"
set size 1,2
set xlabel \"Interface Count\"
set ylabel \"Seconds\"
set grid
plot \'/tmp/ip_rpt.txt\' using 1:3 title \"ip_system\" with lines, \\
     \'/tmp/ip_rpt.txt\' using 1:2 title \"ip_user\" with lines, \\
     \'/tmp/ifconfig_rpt.txt\' using 1:3 title \"ifconfig_system\" with lines, \\
     \'/tmp/ifconfig_rpt.txt\' using 1:2 title \"ifconfig_user\" with lines, \\
     \'/tmp/ip_rpt_no_hash.txt\' using 1:3 title \"ip_system_no_hash\" with lines, \\
     \'/tmp/ip_rpt_no_hash.txt\' using 1:2 title \"ip_user_no_hash\" with lines, \\
     \'/tmp/ifconfig_rpt_no_hash.txt\' using 1:3 title \"ifconfig_system_no_hash\" with lines, \\
     \'/tmp/ifconfig_rpt_no_hash.txt\' using 1:2 title \"ifconfig_user_no_hash\" with lines";
  print "Plotting with cmd -:$plot_cmd:-\n";

  open(GP, "| gnuplot") or die ("Can't open gnuplot pipe(2).\n");
  print GP "$plot_cmd";
  close(GP);

  exit(0);
}

my $num_if = 4000;

`/usr/local/bin/vconfig set_name_type VLAN_PLUS_VID_NO_PAD`;

my $d = 5;
my $c = 5;

if ($ARGV[0] ne "clean") {

  my $i;
  print "Adding VLAN interfaces 1 through $num_if\n";

  print "Turnning off /sbin/hotplug";
  `echo  > /proc/sys/kernel/hotplug`;

  my $p = time();
  for ($i = 1; $i<=$num_if; $i++) {
    `/usr/local/bin/vconfig add eth0 $i`;
    #`ip address flush dev vlan$i`;
    `ip address add 192.168.$c.$c/24 dev vlan$i`;
    `ip link set dev vlan$i up`;

    if (($i <= 4000) && (($i % 50) == 0)) {
      #print "Doing ifconfig -a for $i devices.\n";
      #`time -p ifconfig -a > /tmp/vlan_test_ifconfig_a_$i.txt`;
      print "Doing ip addr show for $i devices.\n";
      `time -p ip addr show > /tmp/vlan_test_ip_addr_$i.txt`;
    }

    $d++;
    if ($d > 250) {
      $d = 5;
      $c++;
    }
  }
  my $n = time();
  my $diff = $n - $p;

  print "Done adding $num_if VLAN interfaces in $diff seconds.\n";

  sleep 2;
}

print "Removing VLAN interfaces 1 through $num_if\n";
$d = 5;
$c = 5;
my $p = time();
my $i;
for ($i = 1; $i<=$num_if; $i++) {
  `/usr/local/bin/vconfig rem vlan$i`;

  $d++;
  if ($d > 250) {
    $d = 5;
    $c++;
  }
}
my $n = time();
my $diff = $n - $p;
print "Done deleting $num_if VLAN interfaces in $diff seconds.\n";

sleep 2;


if ($ARGV[0] ne "clean") {

  my $tmp = $num_if / 4;
  print "\nGoing to add and remove 2 interfaces $tmp times.\n";
  $p = time();
  
  
  for ($i = 1; $i<=$tmp; $i++) {
    `/usr/local/bin/vconfig add eth0 1`;
    `ifconfig vlan1 192.168.200.200`;
    `ifconfig vlan1 up`;
    `ifconfig vlan1 down`;
    
    `/usr/local/bin/vconfig add eth0 2`;
    `ifconfig vlan2 192.168.202.202`;
    `ifconfig vlan2 up`;
    `ifconfig vlan2 down`;
    
    `/usr/local/bin/vconfig rem vlan2`;
    `/usr/local/bin/vconfig rem vlan1`;
  }
  $n = time();
  $diff = $n - $p;
  print "Done adding/removing 2 VLAN interfaces $tmp times in $diff seconds.\n";
}

print "Re-installing /sbin/hotplug";
`echo /sbin/hotplug > /proc/sys/kernel/hotplug`;

