#!/usr/bin/env python

'''automated testing library for testing Samba against windows'''

import pexpect, subprocess
import optparse
import sys, os, time, re

class wintest():
    '''testing of Samba against windows VMs'''

    def __init__(self):
        self.vars = {}
        self.list_mode = False
        self.vms = None
        os.environ['PYTHONUNBUFFERED'] = '1'
        self.parser = optparse.OptionParser("wintest")

    def check_prerequesites(self):
        self.info("Checking prerequesites")
        self.setvar('HOSTNAME', self.cmd_output("hostname -s").strip())
        if os.getuid() != 0:
            raise Exception("You must run this script as root")
        self.run_cmd('ifconfig ${INTERFACE} ${INTERFACE_NET} up')
        if self.getvar('INTERFACE_IPV6'):
            self.run_cmd('ifconfig ${INTERFACE} inet6 del ${INTERFACE_IPV6}/64', checkfail=False)
            self.run_cmd('ifconfig ${INTERFACE} inet6 add ${INTERFACE_IPV6}/64 up')

    def stop_vms(self):
        '''Shut down any existing alive VMs, so they do not collide with what we are doing'''
        self.info('Shutting down any of our VMs already running')
        vms = self.get_vms()
        for v in vms:
            self.vm_poweroff(v, checkfail=False)

    def setvar(self, varname, value):
        '''set a substitution variable'''
        self.vars[varname] = value

    def getvar(self, varname):
        '''return a substitution variable'''
        if not varname in self.vars:
            return None
        return self.vars[varname]

    def setwinvars(self, vm, prefix='WIN'):
        '''setup WIN_XX vars based on a vm name'''
        for v in ['VM', 'HOSTNAME', 'USER', 'PASS', 'SNAPSHOT', 'REALM', 'DOMAIN', 'IP']:
            vname = '%s_%s' % (vm, v)
            if vname in self.vars:
                self.setvar("%s_%s" % (prefix,v), self.substitute("${%s}" % vname))
            else:
                self.vars.pop("%s_%s" % (prefix,v), None)

        if self.getvar("WIN_REALM"):
            self.setvar("WIN_REALM", self.getvar("WIN_REALM").upper())
            self.setvar("WIN_LCREALM", self.getvar("WIN_REALM").lower())
            dnsdomain = self.getvar("WIN_REALM")
            self.setvar("WIN_BASEDN", "DC=" + dnsdomain.replace(".", ",DC="))
        if self.getvar("WIN_USER") is None:
            self.setvar("WIN_USER", "administrator")

    def info(self, msg):
        '''print some information'''
        if not self.list_mode:
            print(self.substitute(msg))

    def load_config(self, fname):
        '''load the config file'''
        f = open(fname)
        for line in f:
            line = line.strip()
            if len(line) == 0 or line[0] == '#':
                continue
            colon = line.find(':')
            if colon == -1:
                raise RuntimeError("Invalid config line '%s'" % line)
            varname = line[0:colon].strip()
            value   = line[colon+1:].strip()
            self.setvar(varname, value)

    def list_steps_mode(self):
        '''put wintest in step listing mode'''
        self.list_mode = True

    def set_skip(self, skiplist):
        '''set a list of tests to skip'''
        self.skiplist = skiplist.split(',')

    def set_vms(self, vms):
        '''set a list of VMs to test'''
        if vms is not None:
            self.vms = []
            for vm in vms.split(','):
                vm = vm.upper()
                self.vms.append(vm)

    def skip(self, step):
        '''return True if we should skip a step'''
        if self.list_mode:
            print("\t%s" % step)
            return True
        return step in self.skiplist

    def substitute(self, text):
        """Substitute strings of the form ${NAME} in text, replacing
        with substitutions from vars.
        """
        if isinstance(text, list):
            ret = text[:]
            for i in range(len(ret)):
                ret[i] = self.substitute(ret[i])
            return ret

        """We may have objects such as pexpect.EOF that are not strings"""
        if not isinstance(text, str):
            return text
        while True:
            var_start = text.find("${")
            if var_start == -1:
                return text
            var_end = text.find("}", var_start)
            if var_end == -1:
                return text
            var_name = text[var_start+2:var_end]
            if not var_name in self.vars:
                raise RuntimeError("Unknown substitution variable ${%s}" % var_name)
            text = text.replace("${%s}" % var_name, self.vars[var_name])
        return text

    def have_var(self, varname):
        '''see if a variable has been set'''
        return varname in self.vars

    def have_vm(self, vmname):
        '''see if a VM should be used'''
        if not self.have_var(vmname + '_VM'):
            return False
        if self.vms is None:
            return True
        return vmname in self.vms

    def putenv(self, key, value):
        '''putenv with substitution'''
        os.environ[key] = self.substitute(value)

    def chdir(self, dir):
        '''chdir with substitution'''
        os.chdir(self.substitute(dir))

    def del_files(self, dirs):
        '''delete all files in the given directory'''
        for d in dirs:
            self.run_cmd("find %s -type f | xargs rm -f" % d)

    def write_file(self, filename, text, mode='w'):
        '''write to a file'''
        f = open(self.substitute(filename), mode=mode)
        f.write(self.substitute(text))
        f.close()

    def run_cmd(self, cmd, dir=".", show=None, output=False, checkfail=True):
        '''run a command'''
        cmd = self.substitute(cmd)
        if isinstance(cmd, list):
            self.info('$ ' + " ".join(cmd))
        else:
            self.info('$ ' + cmd)
        if output:
            return subprocess.Popen([cmd], shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=dir).communicate()[0]
        if isinstance(cmd, list):
            shell=False
        else:
            shell=True
        if checkfail:
            return subprocess.check_call(cmd, shell=shell, cwd=dir)
        else:
            return subprocess.call(cmd, shell=shell, cwd=dir)


    def run_child(self, cmd, dir="."):
        '''create a child and return the Popen handle to it'''
        cwd = os.getcwd()
        cmd = self.substitute(cmd)
        if isinstance(cmd, list):
            self.info('$ ' + " ".join(cmd))
        else:
            self.info('$ ' + cmd)
        if isinstance(cmd, list):
            shell=False
        else:
            shell=True
        os.chdir(dir)
        ret = subprocess.Popen(cmd, shell=shell, stderr=subprocess.STDOUT)
        os.chdir(cwd)
        return ret

    def cmd_output(self, cmd):
        '''return output from and command'''
        cmd = self.substitute(cmd)
        return self.run_cmd(cmd, output=True)

    def cmd_contains(self, cmd, contains, nomatch=False, ordered=False, regex=False,
                     casefold=True):
        '''check that command output contains the listed strings'''

        if isinstance(contains, str):
            contains = [contains]

        out = self.cmd_output(cmd)
        self.info(out)
        for c in self.substitute(contains):
            if regex:
                if casefold:
                    c = c.upper()
                    out = out.upper()
                m = re.search(c, out)
                if m is None:
                    start = -1
                    end = -1
                else:
                    start = m.start()
                    end = m.end()
            elif casefold:
                start = out.upper().find(c.upper())
                end = start + len(c)
            else:
                start = out.find(c)
                end = start + len(c)
            if nomatch:
                if start != -1:
                    raise RuntimeError("Expected to not see %s in %s" % (c, cmd))
            else:
                if start == -1:
                    raise RuntimeError("Expected to see %s in %s" % (c, cmd))
            if ordered and start != -1:
                out = out[end:]

    def retry_cmd(self, cmd, contains, retries=30, delay=2, wait_for_fail=False,
                  ordered=False, regex=False, casefold=True):
        '''retry a command a number of times'''
        while retries > 0:
            try:
                self.cmd_contains(cmd, contains, nomatch=wait_for_fail,
                                  ordered=ordered, regex=regex, casefold=casefold)
                return
            except:
                time.sleep(delay)
                retries -= 1
                self.info("retrying (retries=%u delay=%u)" % (retries, delay))
        raise RuntimeError("Failed to find %s" % contains)

    def pexpect_spawn(self, cmd, timeout=60, crlf=True, casefold=True):
        '''wrapper around pexpect spawn'''
        cmd = self.substitute(cmd)
        self.info("$ " + cmd)
        ret = pexpect.spawn(cmd, logfile=sys.stdout, timeout=timeout)

        def sendline_sub(line):
            line = self.substitute(line)
            if crlf:
                line = line.replace('\n', '\r\n') + '\r'
            return ret.old_sendline(line)

        def expect_sub(line, timeout=ret.timeout, casefold=casefold):
            line = self.substitute(line)
            if casefold:
                if isinstance(line, list):
                    for i in range(len(line)):
                        if isinstance(line[i], str):
                            line[i] = '(?i)' + line[i]
                elif isinstance(line, str):
                    line = '(?i)' + line
            return ret.old_expect(line, timeout=timeout)

        ret.old_sendline = ret.sendline
        ret.sendline = sendline_sub
        ret.old_expect = ret.expect
        ret.expect = expect_sub

        return ret

    def get_nameserver(self):
        '''Get the current nameserver from /etc/resolv.conf'''
        child = self.pexpect_spawn('cat /etc/resolv.conf', crlf=False)
        i = child.expect(['Generated by wintest', 'nameserver'])
        if i == 0:
            child.expect('your original resolv.conf')
            child.expect('nameserver')
        child.expect('\d+.\d+.\d+.\d+')
        return child.after

    def rndc_cmd(self, cmd, checkfail=True):
        '''run a rndc command'''
        self.run_cmd("${RNDC} -c ${PREFIX}/etc/rndc.conf %s" % cmd, checkfail=checkfail)

    def named_supports_gssapi_keytab(self):
        '''see if named supports tkey-gssapi-keytab'''
        self.write_file("${PREFIX}/named.conf.test",
                     'options { tkey-gssapi-keytab "test"; };')
        try:
            self.run_cmd("${NAMED_CHECKCONF} ${PREFIX}/named.conf.test")
        except subprocess.CalledProcessError:
            return False
        return True

    def set_nameserver(self, nameserver):
        '''set the nameserver in resolv.conf'''
        self.write_file("/etc/resolv.conf.wintest", '''
# Generated by wintest, the Samba v Windows automated testing system
nameserver %s

# your original resolv.conf appears below:
''' % self.substitute(nameserver))
        child = self.pexpect_spawn("cat /etc/resolv.conf", crlf=False)
        i = child.expect(['your original resolv.conf appears below:', pexpect.EOF])
        if i == 0:
            child.expect(pexpect.EOF)
        contents = child.before.lstrip().replace('\r', '')
        self.write_file('/etc/resolv.conf.wintest', contents, mode='a')
        self.write_file('/etc/resolv.conf.wintest-bak', contents)
        self.run_cmd("mv -f /etc/resolv.conf.wintest /etc/resolv.conf")
        self.resolv_conf_backup = '/etc/resolv.conf.wintest-bak';

    def configure_bind(self, kerberos_support=False, include=None):
        self.chdir('${PREFIX}')

        nameserver = self.get_nameserver()
        if nameserver == self.getvar('INTERFACE_IP'):
            raise RuntimeError("old /etc/resolv.conf must not contain %s as a nameserver, this will create loops with the generated dns configuration" % nameserver)
        self.setvar('DNSSERVER', nameserver)

        if self.getvar('INTERFACE_IPV6'):
            ipv6_listen = 'listen-on-v6 port 53 { ${INTERFACE_IPV6}; };'
        else:
            ipv6_listen = ''
        self.setvar('BIND_LISTEN_IPV6', ipv6_listen)

        if not kerberos_support:
            self.setvar("NAMED_TKEY_OPTION", "")
        else:
            if self.named_supports_gssapi_keytab():
                self.setvar("NAMED_TKEY_OPTION",
                         'tkey-gssapi-keytab "${PREFIX}/private/dns.keytab";')
            else:
                self.info("LCREALM=${LCREALM}")
                self.setvar("NAMED_TKEY_OPTION",
                         '''tkey-gssapi-credential "DNS/${LCREALM}";
                            tkey-domain "${LCREALM}";
                 ''')
            self.putenv('KEYTAB_FILE', '${PREFIX}/private/dns.keytab')
            self.putenv('KRB5_KTNAME', '${PREFIX}/private/dns.keytab')

        if include:
            self.setvar("NAMED_INCLUDE", 'include "%s";' % include)
        else:
            self.setvar("NAMED_INCLUDE", '')

        self.run_cmd("mkdir -p ${PREFIX}/etc")

        self.write_file("etc/named.conf", '''
options {
	listen-on port 53 { ${INTERFACE_IP};  };
	${BIND_LISTEN_IPV6}
	directory 	"${PREFIX}/var/named";
	dump-file 	"${PREFIX}/var/named/data/cache_dump.db";
	pid-file 	"${PREFIX}/var/named/named.pid";
        statistics-file "${PREFIX}/var/named/data/named_stats.txt";
        memstatistics-file "${PREFIX}/var/named/data/named_mem_stats.txt";
	allow-query     { any; };
	recursion yes;
	${NAMED_TKEY_OPTION}
        max-cache-ttl 10;
        max-ncache-ttl 10;

	forward only;
	forwarders {
		  ${DNSSERVER};
	};

};

key "rndc-key" {
	algorithm hmac-md5;
	secret "lA/cTrno03mt5Ju17ybEYw==";
};

controls {
	inet ${INTERFACE_IP} port 953
	allow { any; } keys { "rndc-key"; };
};

${NAMED_INCLUDE}
''')

        # add forwarding for the windows domains
        domains = self.get_domains()
        for d in domains:
            self.write_file('etc/named.conf',
                         '''
zone "%s" IN {
      type forward;
      forward only;
      forwarders {
         %s;
      };
};
''' % (d, domains[d]),
                     mode='a')


        self.write_file("etc/rndc.conf", '''
# Start of rndc.conf
key "rndc-key" {
	algorithm hmac-md5;
	secret "lA/cTrno03mt5Ju17ybEYw==";
};

options {
	default-key "rndc-key";
	default-server  ${INTERFACE_IP};
	default-port 953;
};
''')


    def stop_bind(self):
        '''Stop our private BIND from listening and operating'''
        self.rndc_cmd("stop", checkfail=False)
        self.port_wait("${INTERFACE_IP}", 53, wait_for_fail=True)

        self.run_cmd("rm -rf var/named")


    def start_bind(self):
        '''restart the test environment version of bind'''
        self.info("Restarting bind9")
        self.chdir('${PREFIX}')

        self.set_nameserver(self.getvar('INTERFACE_IP'))

        self.run_cmd("mkdir -p var/named/data")
        self.run_cmd("chown -R ${BIND_USER} var/named")

        self.bind_child = self.run_child("${BIND9} -u ${BIND_USER} -n 1 -c ${PREFIX}/etc/named.conf -g")

        self.port_wait("${INTERFACE_IP}", 53)
        self.rndc_cmd("flush")

    def restart_bind(self, kerberos_support=False, include=None):
        self.configure_bind(kerberos_support=kerberos_support, include=include)
        self.stop_bind()
        self.start_bind()

    def restore_resolv_conf(self):
        '''restore the /etc/resolv.conf after testing is complete'''
        if getattr(self, 'resolv_conf_backup', False):
            self.info("restoring /etc/resolv.conf")
            self.run_cmd("mv -f %s /etc/resolv.conf" % self.resolv_conf_backup)


    def vm_poweroff(self, vmname, checkfail=True):
        '''power off a VM'''
        self.setvar('VMNAME', vmname)
        self.run_cmd("${VM_POWEROFF}", checkfail=checkfail)

    def vm_reset(self, vmname):
        '''reset a VM'''
        self.setvar('VMNAME', vmname)
        self.run_cmd("${VM_RESET}")

    def vm_restore(self, vmname, snapshot):
        '''restore a VM'''
        self.setvar('VMNAME', vmname)
        self.setvar('SNAPSHOT', snapshot)
        self.run_cmd("${VM_RESTORE}")

    def ping_wait(self, hostname):
        '''wait for a hostname to come up on the network'''
        hostname = self.substitute(hostname)
        loops=10
        while loops > 0:
            try:
                self.run_cmd("ping -c 1 -w 10 %s" % hostname)
                break
            except:
                loops = loops - 1
        if loops == 0:
            raise RuntimeError("Failed to ping %s" % hostname)
        self.info("Host %s is up" % hostname)

    def port_wait(self, hostname, port, retries=200, delay=3, wait_for_fail=False):
        '''wait for a host to come up on the network'''
        self.retry_cmd("nc -v -z -w 1 %s %u" % (hostname, port), ['succeeded'],
                       retries=retries, delay=delay, wait_for_fail=wait_for_fail)

    def run_net_time(self, child):
        '''run net time on windows'''
        child.sendline("net time \\\\${HOSTNAME} /set")
        child.expect("Do you want to set the local computer")
        child.sendline("Y")
        child.expect("The command completed successfully")

    def run_date_time(self, child, time_tuple=None):
        '''run date and time on windows'''
        if time_tuple is None:
            time_tuple = time.localtime()
        child.sendline("date")
        child.expect("Enter the new date:")
        i = child.expect(["dd-mm-yy", "mm-dd-yy"])
        if i == 0:
            child.sendline(time.strftime("%d-%m-%y", time_tuple))
        else:
            child.sendline(time.strftime("%m-%d-%y", time_tuple))
        child.expect("C:")
        child.sendline("time")
        child.expect("Enter the new time:")
        child.sendline(time.strftime("%H:%M:%S", time_tuple))
        child.expect("C:")

    def get_ipconfig(self, child):
        '''get the IP configuration of the child'''
        child.sendline("ipconfig /all")
        child.expect('Ethernet adapter ')
        child.expect("[\w\s]+")
        self.setvar("WIN_NIC", child.after)
        child.expect(['IPv4 Address', 'IP Address'])
        child.expect('\d+.\d+.\d+.\d+')
        self.setvar('WIN_IPV4_ADDRESS', child.after)
        child.expect('Subnet Mask')
        child.expect('\d+.\d+.\d+.\d+')
        self.setvar('WIN_SUBNET_MASK', child.after)
        child.expect('Default Gateway')
        child.expect('\d+.\d+.\d+.\d+')
        self.setvar('WIN_DEFAULT_GATEWAY', child.after)
        child.expect("C:")

    def get_is_dc(self, child):
        '''check if a windows machine is a domain controller'''
        child.sendline("dcdiag")
        i = child.expect(["is not a Directory Server",
                          "is not recognized as an internal or external command",
                          "Home Server = ",
                          "passed test Replications"])
        if i == 0:
            return False
        if i == 1 or i == 3:
            child.expect("C:")
            child.sendline("net config Workstation")
            child.expect("Workstation domain")
            child.expect('[\S]+')
            domain = child.after
            i = child.expect(["Workstation Domain DNS Name", "Logon domain"])
            '''If we get the Logon domain first, we are not in an AD domain'''
            if i == 1:
                return False
            if domain.upper() == self.getvar("WIN_DOMAIN").upper():
                return True

        child.expect('[\S]+')
        hostname = child.after
        if hostname.upper() == self.getvar("WIN_HOSTNAME").upper():
            return True

    def set_noexpire(self, child, username):
        '''Ensure this user's password does not expire'''
        child.sendline('wmic useraccount where name="%s" set PasswordExpires=FALSE' % username)
        child.expect("update successful")
        child.expect("C:")

    def run_tlntadmn(self, child):
        '''remove the annoying telnet restrictions'''
        child.sendline('tlntadmn config maxconn=1024')
        child.expect("The settings were successfully updated")
        child.expect("C:")

    def disable_firewall(self, child):
        '''remove the annoying firewall'''
        child.sendline('netsh advfirewall set allprofiles state off')
        i = child.expect(["Ok", "The following command was not found: advfirewall set allprofiles state off"])
        child.expect("C:")
        if i == 1:
            child.sendline('netsh firewall set opmode mode = DISABLE profile = ALL')
            i = child.expect(["Ok", "The following command was not found"])
            if i != 0:
                self.info("Firewall disable failed - ignoring")
            child.expect("C:")

    def set_dns(self, child):
        child.sendline('netsh interface ip set dns "${WIN_NIC}" static ${INTERFACE_IP} primary')
        i = child.expect(['C:', pexpect.EOF, pexpect.TIMEOUT], timeout=5)
        if i > 0:
            return True
        else:
            return False

    def set_ip(self, child):
        """fix the IP address to the same value it had when we
        connected, but don't use DHCP, and force the DNS server to our
        DNS server.  This allows DNS updates to run"""
        self.get_ipconfig(child)
        if self.getvar("WIN_IPV4_ADDRESS") != self.getvar("WIN_IP"):
            raise RuntimeError("ipconfig address %s != nmblookup address %s" % (self.getvar("WIN_IPV4_ADDRESS"),
                                                                                self.getvar("WIN_IP")))
        child.sendline('netsh')
        child.expect('netsh>')
        child.sendline('offline')
        child.expect('netsh>')
        child.sendline('routing ip add persistentroute dest=0.0.0.0 mask=0.0.0.0 name="${WIN_NIC}" nhop=${WIN_DEFAULT_GATEWAY}')
        child.expect('netsh>')
        child.sendline('interface ip set address "${WIN_NIC}" static ${WIN_IPV4_ADDRESS} ${WIN_SUBNET_MASK} ${WIN_DEFAULT_GATEWAY} 1 store=persistent')
        i = child.expect(['The syntax supplied for this command is not valid. Check help for the correct syntax', 'netsh>', pexpect.EOF, pexpect.TIMEOUT], timeout=5)
        if i == 0:
            child.sendline('interface ip set address "${WIN_NIC}" static ${WIN_IPV4_ADDRESS} ${WIN_SUBNET_MASK} ${WIN_DEFAULT_GATEWAY} 1')
            child.expect('netsh>')
        child.sendline('commit')
        child.sendline('online')
        child.sendline('exit')

        child.expect([pexpect.EOF, pexpect.TIMEOUT], timeout=5)
        return True


    def resolve_ip(self, hostname, retries=60, delay=5):
        '''resolve an IP given a hostname, assuming NBT'''
        while retries > 0:
            child = self.pexpect_spawn("bin/nmblookup %s" % hostname)
            i = 0
            while i == 0:
                i = child.expect(["querying", '\d+.\d+.\d+.\d+', hostname, "Lookup failed"])
                if i == 0:
                    child.expect("\r")
            if i == 1:
                return child.after
            retries -= 1
            time.sleep(delay)
            self.info("retrying (retries=%u delay=%u)" % (retries, delay))
        raise RuntimeError("Failed to resolve IP of %s" % hostname)


    def open_telnet(self, hostname, username, password, retries=60, delay=5, set_time=False, set_ip=False,
                    disable_firewall=True, run_tlntadmn=True, set_noexpire=False):
        '''open a telnet connection to a windows server, return the pexpect child'''
        set_route = False
        set_dns = False
        if self.getvar('WIN_IP'):
            ip = self.getvar('WIN_IP')
        else:
            ip = self.resolve_ip(hostname)
            self.setvar('WIN_IP', ip)
        while retries > 0:
            child = self.pexpect_spawn("telnet " + ip + " -l '" + username + "'")
            i = child.expect(["Welcome to Microsoft Telnet Service",
                              "Denying new connections due to the limit on number of connections",
                              "No more connections are allowed to telnet server",
                              "Unable to connect to remote host",
                              "No route to host",
                              "Connection refused",
                              pexpect.EOF])
            if i != 0:
                child.close()
                time.sleep(delay)
                retries -= 1
                self.info("retrying (retries=%u delay=%u)" % (retries, delay))
                continue
            child.expect("password:")
            child.sendline(password)
            i = child.expect(["C:",
                              "Denying new connections due to the limit on number of connections",
                              "No more connections are allowed to telnet server",
                              "Unable to connect to remote host",
                              "No route to host",
                              "Connection refused",
                              pexpect.EOF])
            if i != 0:
                child.close()
                time.sleep(delay)
                retries -= 1
                self.info("retrying (retries=%u delay=%u)" % (retries, delay))
                continue
            if set_dns:
                set_dns = False
                if self.set_dns(child):
                    continue;
            if set_route:
                child.sendline('route add 0.0.0.0 mask 0.0.0.0 ${WIN_DEFAULT_GATEWAY}')
                child.expect("C:")
                set_route = False
            if set_time:
                self.run_date_time(child, None)
                set_time = False
            if run_tlntadmn:
                self.run_tlntadmn(child)
                run_tlntadmn = False
            if set_noexpire:
                self.set_noexpire(child, username)
                set_noexpire = False
            if disable_firewall:
                self.disable_firewall(child)
                disable_firewall = False
            if set_ip:
                set_ip = False
                if self.set_ip(child):
                    set_route = True
                    set_dns = True
                continue
            return child
        raise RuntimeError("Failed to connect with telnet")

    def kinit(self, username, password):
        '''use kinit to setup a credentials cache'''
        self.run_cmd("kdestroy")
        self.putenv('KRB5CCNAME', "${PREFIX}/ccache.test")
        username = self.substitute(username)
        s = username.split('@')
        if len(s) > 0:
            s[1] = s[1].upper()
        username = '@'.join(s)
        child = self.pexpect_spawn('kinit ' + username)
        child.expect("Password")
        child.sendline(password)
        child.expect(pexpect.EOF)
        child.close()
        if child.exitstatus != 0:
            raise RuntimeError("kinit failed with status %d" % child.exitstatus)

    def get_domains(self):
        '''return a dictionary of DNS domains and IPs for named.conf'''
        ret = {}
        for v in self.vars:
            if v[-6:] == "_REALM":
                base = v[:-6]
                if base + '_IP' in self.vars:
                    ret[self.vars[base + '_REALM']] = self.vars[base + '_IP']
        return ret

    def wait_reboot(self, retries=3):
        '''wait for a VM to reboot'''

        # first wait for it to shutdown
        self.port_wait("${WIN_IP}", 139, wait_for_fail=True, delay=6)

        # now wait for it to come back. If it fails to come back
        # then try resetting it
        while retries > 0:
            try:
                self.port_wait("${WIN_IP}", 139)
                return
            except:
                retries -= 1
                self.vm_reset("${WIN_VM}")
                self.info("retrying reboot (retries=%u)" % retries)
        raise RuntimeError(self.substitute("VM ${WIN_VM} failed to reboot"))

    def get_vms(self):
        '''return a dictionary of all the configured VM names'''
        ret = []
        for v in self.vars:
            if v[-3:] == "_VM":
                ret.append(self.vars[v])
        return ret


    def run_dcpromo_as_first_dc(self, vm, func_level=None):
        self.setwinvars(vm)
        self.info("Configuring a windows VM ${WIN_VM} at the first DC in the domain using dcpromo")
        child = self.open_telnet("${WIN_HOSTNAME}", "administrator", "${WIN_PASS}", set_time=True)
        if self.get_is_dc(child):
            return

        if func_level == '2008r2':
            self.setvar("FUNCTION_LEVEL_INT", str(4))
        elif func_level == '2003':
            self.setvar("FUNCTION_LEVEL_INT", str(1))
        else:
            self.setvar("FUNCTION_LEVEL_INT", str(0))

        child = self.open_telnet("${WIN_HOSTNAME}", "administrator", "${WIN_PASS}", set_ip=True, set_noexpire=True)

        """This server must therefore not yet be a directory server, so we must promote it"""
        child.sendline("copy /Y con answers.txt")
        child.sendline('''
[DCInstall]
; New forest promotion
ReplicaOrNewDomain=Domain
NewDomain=Forest
NewDomainDNSName=${WIN_REALM}
ForestLevel=${FUNCTION_LEVEL_INT}
DomainNetbiosName=${WIN_DOMAIN}
DomainLevel=${FUNCTION_LEVEL_INT}
InstallDNS=Yes
ConfirmGc=Yes
CreateDNSDelegation=No
DatabasePath="C:\Windows\NTDS"
LogPath="C:\Windows\NTDS"
SYSVOLPath="C:\Windows\SYSVOL"
; Set SafeModeAdminPassword to the correct value prior to using the unattend file
SafeModeAdminPassword=${WIN_PASS}
; Run-time flags (optional)
RebootOnCompletion=No

''')
        child.expect("copied.")
        child.expect("C:")
        child.expect("C:")
        child.sendline("dcpromo /answer:answers.txt")
        i = child.expect(["You must restart this computer", "failed", "Active Directory Domain Services was not installed", "C:"], timeout=240)
        if i == 1 or i == 2:
            raise Exception("dcpromo failed")
        child.sendline("shutdown -r -t 0")
        self.port_wait("${WIN_IP}", 139, wait_for_fail=True)
        self.port_wait("${WIN_IP}", 139)
        self.retry_cmd("host -t SRV _ldap._tcp.${WIN_REALM} ${WIN_IP}", ['has SRV record'] )


    def start_winvm(self, vm):
        '''start a Windows VM'''
        self.setwinvars(vm)
        
        self.info("Joining a windows box to the domain")
        self.vm_poweroff("${WIN_VM}", checkfail=False)
        self.vm_restore("${WIN_VM}", "${WIN_SNAPSHOT}")

    def run_winjoin(self, vm, domain, username="administrator", password="${PASSWORD1}"):
        '''join a windows box to a domain'''
        child = self.open_telnet("${WIN_HOSTNAME}", "${WIN_USER}", "${WIN_PASS}", set_time=True, set_ip=True, set_noexpire=True)
        child.sendline("ipconfig /flushdns")
        child.expect("C:")
        child.sendline("netdom join ${WIN_HOSTNAME} /Domain:%s /UserD:%s /PasswordD:%s" % (domain, username, password))
        child.expect("The command completed successfully")
        child.expect("C:")
        child.sendline("shutdown /r -t 0")
        self.wait_reboot()
        child = self.open_telnet("${WIN_HOSTNAME}", "${WIN_USER}", "${WIN_PASS}", set_time=True, set_ip=True)
        child.sendline("ipconfig /registerdns")
        child.expect("Registration of the DNS resource records for all adapters of this computer has been initiated. Any errors will be reported in the Event Viewer")
        child.expect("C:")


    def test_remote_smbclient(self, vm, username="${WIN_USER}", password="${WIN_PASS}", args=""):
        '''test smbclient against remote server'''
        self.setwinvars(vm)
        self.info('Testing smbclient')
        self.chdir('${PREFIX}')
        self.cmd_contains("bin/smbclient --version", ["${SAMBA_VERSION}"])
        self.retry_cmd('bin/smbclient -L ${WIN_HOSTNAME} -U%s%%%s %s' % (username, password, args), ["IPC"])


    def setup(self, testname, subdir):
        '''setup for main tests, parsing command line'''
        self.parser.add_option("--conf", type='string', default='', help='config file')
        self.parser.add_option("--skip", type='string', default='', help='list of steps to skip (comma separated)')
        self.parser.add_option("--vms", type='string', default=None, help='list of VMs to use (comma separated)')
        self.parser.add_option("--list", action='store_true', default=False, help='list the available steps')
        self.parser.add_option("--rebase", action='store_true', default=False, help='do a git pull --rebase')
        self.parser.add_option("--clean", action='store_true', default=False, help='clean the tree')
        self.parser.add_option("--prefix", type='string', default=None, help='override install prefix')
        self.parser.add_option("--sourcetree", type='string', default=None, help='override sourcetree location')
        self.parser.add_option("--nocleanup", action='store_true', default=False, help='disable cleanup code')

        self.opts, self.args = self.parser.parse_args()

        if not self.opts.conf:
            print("Please specify a config file with --conf")
            sys.exit(1)

        # we don't need fsync safety in these tests
        self.putenv('TDB_NO_FSYNC', '1')

        self.load_config(self.opts.conf)

        self.set_skip(self.opts.skip)
        self.set_vms(self.opts.vms)

        if self.opts.list:
            self.list_steps_mode()

        if self.opts.prefix:
            self.setvar('PREFIX', self.opts.prefix)

        if self.opts.sourcetree:
            self.setvar('SOURCETREE', self.opts.sourcetree)

        if self.opts.rebase:
            self.info('rebasing')
            self.chdir('${SOURCETREE}')
            self.run_cmd('git pull --rebase')

        if self.opts.clean:
            self.info('cleaning')
            self.chdir('${SOURCETREE}/' + subdir)
            self.run_cmd('make clean')
