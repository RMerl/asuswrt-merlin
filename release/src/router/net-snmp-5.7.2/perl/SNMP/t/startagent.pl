# common parameters used in SNMP::Session creation and tests
$agent_host = 'localhost';
$agent_port = 8765;
$trap_port = 8764;
$mibdir = '/usr/local/share/snmp/mibs';
$comm = 'v1_private';
$comm2 = 'v2c_private';
$comm3 = 'v3_private';
$sec_name = 'v3_user';
$oid = '.1.3.6.1.2.1.1.1';
$name = 'sysDescr';
$name_module = 'RFC1213-MIB::sysDescr';
$name_module2 = 'SNMPv2-MIB::sysDescr';
$name_long = '.iso.org.dod.internet.mgmt.mib-2.system.sysDescr';
$name_module_long = 'RFC1213-MIB::.iso.org.dod.internet.mgmt.mib-2.system.sysDescr';
$name_module_long2 = 'SNMPv2-MIB::.iso.org.dod.internet.mgmt.mib-2.system.sysDescr';
$auth_pass = 'test_pass_auth';
$priv_pass = 'test_pass_priv';

# don't use any .conf files other than those specified.
$ENV{'SNMPCONFPATH'} |= "bogus";

# erroneous input to test failure cases
$bad_comm = 'BAD_COMMUNITY';
$bad_name = "badName";
$bad_oid = ".1.3.6.1.2.1.1.1.1.1.1";
$bad_host = 'bad.host.here';
$bad_port = '9999999';
$bad_auth_pass = 'bad_auth_pass';
$bad_priv_pass = 'bad_priv_pass';
$bad_sec_name = 'bad_sec_name';
$bad_version = 7;

local $snmpd_cmd;
local $snmptrapd_cmd;
my $line;

if ($^O =~ /win32/i) {
  require Win32::Process;
}

sub run_async {
  my ($pidfile, $cmd, @args) = @_;
  if (-r "$cmd" and -x "$cmd") {
    if ($^O =~ /win32/i) {
      $cmd =~ s/\//\\/g;
      system "start \"$cmd\" /min cmd /c \"$cmd @args 2>&1\"";
    } else {
      system "$cmd @args 2>&1";
    }
    # Wait at most three seconds for the pid file to appear.
    for ($i = 0; ($i < 3) && ! (-r "$pidfile"); ++$i) {
      sleep 1;
    }
  } else {
    warn "Couldn't run $cmd\n";
  }
}

sub snmptest_cleanup {
  kill_by_pid_file("t/snmpd.pid");
  unlink("t/snmpd.pid");
  kill_by_pid_file("t/snmptrapd.pid");
  unlink("t/snmptrapd.pid");
}

sub kill_by_pid_file {
  if ((-e "$_[0]") && (-r "$_[0]")) {
    if ($^O !~ /win32/i) {
      # Unix or Windows + Cygwin.
      system "kill `cat $_[0]` > /dev/null 2>&1";
    } else {
      # Windows + MSVC or Windows + MinGW.
      open(H, "<$_[0]");
      my $pid = (<H>);
      close (H);
      if ($pid > 0) {
        Win32::Process::KillProcess($pid, 0)
      }
    }
  }
}


# Stop any processes started during a previous test.
snmptest_cleanup();

#Open the snmptest.cmd file and get the info
if (open(CMD, "<t/snmptest.cmd")) {
  while ($line = <CMD>) {
    if ($line =~ /HOST\s*=>\s*(.*?)\s+$/) {
      $agent_host = $1;
    } elsif ($line =~ /MIBDIR\s*=>\s*(.*?)\s+$/) {
      $mibdir = $1;
    } elsif ($line =~ /AGENT_PORT\s*=>\s*(.*?)\s+$/) {
      $agent_port = $1;
    } elsif ($line =~ /SNMPD\s*=>\s*(.*?)\s+$/) {
      $snmpd_cmd = $1;
    } elsif ($line =~ /SNMPTRAPD\s*=>\s*(.*?)\s+$/) {
      $snmptrapd_cmd = $1;
    }
  } # end of while
  close CMD;
} else {
  die ("Could not start agent. Couldn't find snmptest.cmd file\n");
}

# Start snmpd and snmptrapd.

#warn "\nStarting agents for test script $0\n";

my $scriptname = "snmptest";
if ($0 =~ /^t[\/\\](.*)\.t$/) {
  $scriptname = $1;
}

if ($snmpd_cmd) {
  run_async("t/snmpd.pid", "$snmpd_cmd", "-r -d -Lf t/snmpd-$scriptname.log -M+$mibdir -C -c t/snmptest.conf -p t/snmpd.pid ${agent_host}:${agent_port} >t/snmpd-$scriptname.stderr");
}
if ($snmptrapd_cmd) {
  run_async("t/snmptrapd.pid", "$snmptrapd_cmd", "-d -Lf t/snmptrapd-$scriptname.log -p t/snmptrapd.pid -M+$mibdir -C -c t/snmptest.conf -C ${agent_host}:${trap_port} >t/snmptrapd-$scriptname.stderr");
}

1;

