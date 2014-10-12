#!/usr/bin/env python

'''automated testing of the steps of the Samba4 HOWTO'''

import sys, os
import wintest, pexpect, time, subprocess

def set_krb5_conf(t):
    t.putenv("KRB5_CONFIG", '${PREFIX}/private/krb5.conf')

def build_s4(t):
    '''build samba4'''
    t.info('Building s4')
    t.chdir('${SOURCETREE}')
    t.putenv('CC', 'ccache gcc')
    t.run_cmd('make reconfigure || ./configure --enable-auto-reconfigure --enable-developer --prefix=${PREFIX} -C')
    t.run_cmd('make -j')
    t.run_cmd('rm -rf ${PREFIX}')
    t.run_cmd('make -j install')


def provision_s4(t, func_level="2008"):
    '''provision s4 as a DC'''
    t.info('Provisioning s4')
    t.chdir('${PREFIX}')
    t.del_files(["var", "private"])
    t.run_cmd("rm -f etc/smb.conf")
    provision=['sbin/provision',
               '--realm=${LCREALM}',
               '--domain=${DOMAIN}',
               '--adminpass=${PASSWORD1}',
               '--server-role=domain controller',
               '--function-level=%s' % func_level,
               '-d${DEBUGLEVEL}',
               '--option=interfaces=${INTERFACE}',
               '--host-ip=${INTERFACE_IP}',
               '--option=bind interfaces only=yes',
               '--option=rndc command=${RNDC} -c${PREFIX}/etc/rndc.conf']
    if t.getvar('INTERFACE_IPV6'):
        provision.append('--host-ip6=${INTERFACE_IPV6}')
    t.run_cmd(provision)
    t.run_cmd('bin/samba-tool newuser testallowed ${PASSWORD1}')
    t.run_cmd('bin/samba-tool newuser testdenied ${PASSWORD1}')
    t.run_cmd('bin/samba-tool group addmembers "Allowed RODC Password Replication Group" testallowed')


def start_s4(t):
    '''startup samba4'''
    t.info('Starting Samba4')
    t.chdir("${PREFIX}")
    t.run_cmd('killall -9 -q samba smbd nmbd winbindd', checkfail=False)
    t.run_cmd(['sbin/samba',
             '--option', 'panic action=gnome-terminal -e "gdb --pid %PID%"'])
    t.port_wait("${INTERFACE_IP}", 139)

def test_smbclient(t):
    '''test smbclient against localhost'''
    t.info('Testing smbclient')
    t.chdir('${PREFIX}')
    t.cmd_contains("bin/smbclient --version", ["Version 4.0"])
    t.retry_cmd('bin/smbclient -L ${INTERFACE_IP} -U%', ["netlogon", "sysvol", "IPC Service"])
    child = t.pexpect_spawn('bin/smbclient //${INTERFACE_IP}/netlogon -Uadministrator%${PASSWORD1}')
    child.expect("smb:")
    child.sendline("dir")
    child.expect("blocks available")
    child.sendline("mkdir testdir")
    child.expect("smb:")
    child.sendline("cd testdir")
    child.expect('testdir')
    child.sendline("cd ..")
    child.sendline("rmdir testdir")


def create_shares(t):
    '''create some test shares'''
    t.info("Adding test shares")
    t.chdir('${PREFIX}')
    t.write_file("etc/smb.conf", '''
[test]
       path = ${PREFIX}/test
       read only = no
[profiles]
       path = ${PREFIX}/var/profiles
       read only = no
    ''',
                 mode='a')
    t.run_cmd("mkdir -p test")
    t.run_cmd("mkdir -p var/profiles")


def test_dns(t):
    '''test that DNS is OK'''
    t.info("Testing DNS")
    t.cmd_contains("host -t SRV _ldap._tcp.${LCREALM}.",
                 ['_ldap._tcp.${LCREALM} has SRV record 0 100 389 ${HOSTNAME}.${LCREALM}'])
    t.cmd_contains("host -t SRV  _kerberos._udp.${LCREALM}.",
                 ['_kerberos._udp.${LCREALM} has SRV record 0 100 88 ${HOSTNAME}.${LCREALM}'])
    t.cmd_contains("host -t A ${HOSTNAME}.${LCREALM}",
                 ['${HOSTNAME}.${LCREALM} has address'])

def test_kerberos(t):
    '''test that kerberos is OK'''
    t.info("Testing kerberos")
    t.run_cmd("kdestroy")
    t.kinit("administrator@${REALM}", "${PASSWORD1}")
    # this copes with the differences between MIT and Heimdal klist
    t.cmd_contains("klist", ["rincipal", "administrator@${REALM}"])


def test_dyndns(t):
    '''test that dynamic DNS is working'''
    t.chdir('${PREFIX}')
    t.run_cmd("sbin/samba_dnsupdate --fail-immediately")
    t.rndc_cmd("flush")


def run_winjoin(t, vm):
    '''join a windows box to our domain'''
    t.setwinvars(vm)

    t.run_winjoin(t, "${LCREALM}")

def test_winjoin(t, vm):
    t.info("Checking the windows join is OK")
    t.chdir('${PREFIX}')
    t.port_wait("${WIN_IP}", 139)
    t.retry_cmd('bin/smbclient -L ${WIN_HOSTNAME}.${LCREALM} -Uadministrator@${LCREALM}%${PASSWORD1}', ["C$", "IPC$", "Sharename"], retries=100)
    t.cmd_contains("host -t A ${WIN_HOSTNAME}.${LCREALM}.", ['has address'])
    t.cmd_contains('bin/smbclient -L ${WIN_HOSTNAME}.${LCREALM} -Utestallowed@${LCREALM}%${PASSWORD1}', ["C$", "IPC$", "Sharename"])
    t.cmd_contains('bin/smbclient -L ${WIN_HOSTNAME}.${LCREALM} -k no -Utestallowed@${LCREALM}%${PASSWORD1}', ["C$", "IPC$", "Sharename"])
    t.cmd_contains('bin/smbclient -L ${WIN_HOSTNAME}.${LCREALM} -k yes -Utestallowed@${LCREALM}%${PASSWORD1}', ["C$", "IPC$", "Sharename"])
    child = t.open_telnet("${WIN_HOSTNAME}", "${DOMAIN}\\administrator", "${PASSWORD1}")
    child.sendline("net use t: \\\\${HOSTNAME}.${LCREALM}\\test")
    child.expect("The command completed successfully")


def run_dcpromo(t, vm):
    '''run a dcpromo on windows'''
    t.setwinvars(vm)

    t.info("Joining a windows VM ${WIN_VM} to the domain as a DC using dcpromo")
    child = t.open_telnet("${WIN_HOSTNAME}", "administrator", "${WIN_PASS}", set_ip=True, set_noexpire=True)
    child.sendline("copy /Y con answers.txt")
    child.sendline('''
[DCINSTALL]
RebootOnSuccess=Yes
RebootOnCompletion=Yes
ReplicaOrNewDomain=Replica
ReplicaDomainDNSName=${LCREALM}
SiteName=Default-First-Site-Name
InstallDNS=No
ConfirmGc=Yes
CreateDNSDelegation=No
UserDomain=${LCREALM}
UserName=${LCREALM}\\administrator
Password=${PASSWORD1}
DatabasePath="C:\Windows\NTDS"
LogPath="C:\Windows\NTDS"
SYSVOLPath="C:\Windows\SYSVOL"
SafeModeAdminPassword=${PASSWORD1}

''')
    child.expect("copied.")
    child.expect("C:")
    child.expect("C:")
    child.sendline("dcpromo /answer:answers.txt")
    i = child.expect(["You must restart this computer", "failed", "Active Directory Domain Services was not installed", "C:"], timeout=120)
    if i == 1 or i == 2:
        child.sendline("echo off")
        child.sendline("echo START DCPROMO log")
        child.sendline("more c:\windows\debug\dcpromoui.log")
        child.sendline("echo END DCPROMO log")
        child.expect("END DCPROMO")
        raise Exception("dcpromo failed")
    t.wait_reboot()


def test_dcpromo(t, vm):
    '''test that dcpromo worked'''
    t.info("Checking the dcpromo join is OK")
    t.chdir('${PREFIX}')
    t.port_wait("${WIN_IP}", 139)
    t.retry_cmd("host -t A ${WIN_HOSTNAME}.${LCREALM}. ${INTERFACE_IP}",
                ['${WIN_HOSTNAME}.${LCREALM} has address'],
                retries=30, delay=10, casefold=True)
    t.retry_cmd('bin/smbclient -L ${WIN_HOSTNAME}.${LCREALM} -Uadministrator@${LCREALM}%${PASSWORD1}', ["C$", "IPC$", "Sharename"])
    t.cmd_contains("host -t A ${WIN_HOSTNAME}.${LCREALM}.", ['has address'])
    t.cmd_contains('bin/smbclient -L ${WIN_HOSTNAME}.${LCREALM} -Utestallowed@${LCREALM}%${PASSWORD1}', ["C$", "IPC$", "Sharename"])

    t.cmd_contains("bin/samba-tool drs kcc ${HOSTNAME}.${LCREALM} -Uadministrator@${LCREALM}%${PASSWORD1}", ['Consistency check', 'successful'])
    t.retry_cmd("bin/samba-tool drs kcc ${WIN_HOSTNAME}.${LCREALM} -Uadministrator@${LCREALM}%${PASSWORD1}", ['Consistency check', 'successful'])

    t.kinit("administrator@${REALM}", "${PASSWORD1}")

    # the first replication will transfer the dnsHostname attribute
    t.cmd_contains("bin/samba-tool drs replicate ${HOSTNAME}.${LCREALM} ${WIN_HOSTNAME} CN=Configuration,${BASEDN} -k yes", ["was successful"])

    for nc in [ '${BASEDN}', 'CN=Configuration,${BASEDN}', 'CN=Schema,CN=Configuration,${BASEDN}' ]:
        t.cmd_contains("bin/samba-tool drs replicate ${HOSTNAME}.${LCREALM} ${WIN_HOSTNAME}.${LCREALM} %s -k yes" % nc, ["was successful"])
        t.cmd_contains("bin/samba-tool drs replicate ${WIN_HOSTNAME}.${LCREALM} ${HOSTNAME}.${LCREALM} %s -k yes" % nc, ["was successful"])

    t.cmd_contains("bin/samba-tool drs showrepl ${HOSTNAME}.${LCREALM} -k yes",
                 [ "INBOUND NEIGHBORS",
                   "${BASEDN}",
                   "Last attempt .* was successful",
                   "CN=Configuration,${BASEDN}",
                   "Last attempt .* was successful",
                   "CN=Configuration,${BASEDN}", # cope with either order
                   "Last attempt .* was successful",
                   "OUTBOUND NEIGHBORS",
                   "${BASEDN}",
                   "Last success",
                   "CN=Configuration,${BASEDN}",
                   "Last success",
                   "CN=Configuration,${BASEDN}",
                   "Last success"],
                   ordered=True,
                   regex=True)

    t.cmd_contains("bin/samba-tool drs showrepl ${WIN_HOSTNAME}.${LCREALM} -k yes",
                 [ "INBOUND NEIGHBORS",
                   "${BASEDN}",
                   "Last attempt .* was successful",
                   "CN=Configuration,${BASEDN}",
                   "Last attempt .* was successful",
                   "CN=Configuration,${BASEDN}",
                   "Last attempt .* was successful",
                   "OUTBOUND NEIGHBORS",
                   "${BASEDN}",
                   "Last success",
                   "CN=Configuration,${BASEDN}",
                   "Last success",
                   "CN=Configuration,${BASEDN}",
                   "Last success" ],
                   ordered=True,
                   regex=True)

    child = t.open_telnet("${WIN_HOSTNAME}", "${DOMAIN}\\administrator", "${PASSWORD1}", set_time=True)
    child.sendline("net use t: \\\\${HOSTNAME}.${LCREALM}\\test")

    retries = 10
    i = child.expect(["The command completed successfully", "The network path was not found"])
    while i == 1 and retries > 0:
        child.expect("C:")
        time.sleep(2)
        child.sendline("net use t: \\\\${HOSTNAME}.${LCREALM}\\test")
        i = child.expect(["The command completed successfully", "The network path was not found"])
        retries -=1

    t.run_net_time(child)

    t.info("Checking if showrepl is happy")
    child.sendline("repadmin /showrepl")
    child.expect("${BASEDN}")
    child.expect("was successful")
    child.expect("CN=Configuration,${BASEDN}")
    child.expect("was successful")
    child.expect("CN=Schema,CN=Configuration,${BASEDN}")
    child.expect("was successful")

    t.info("Checking if new users propogate to windows")
    t.retry_cmd('bin/samba-tool newuser test2 ${PASSWORD2}', ["created successfully"])
    t.retry_cmd("bin/smbclient -L ${WIN_HOSTNAME}.${LCREALM} -Utest2%${PASSWORD2} -k no", ['Sharename', 'Remote IPC'])
    t.retry_cmd("bin/smbclient -L ${WIN_HOSTNAME}.${LCREALM} -Utest2%${PASSWORD2} -k yes", ['Sharename', 'Remote IPC'])

    t.info("Checking if new users on windows propogate to samba")
    child.sendline("net user test3 ${PASSWORD3} /add")
    while True:
        i = child.expect(["The command completed successfully",
                          "The directory service was unable to allocate a relative identifier"])
        if i == 0:
            break
        time.sleep(2)

    t.retry_cmd("bin/smbclient -L ${HOSTNAME}.${LCREALM} -Utest3%${PASSWORD3} -k no", ['Sharename', 'IPC'])
    t.retry_cmd("bin/smbclient -L ${HOSTNAME}.${LCREALM} -Utest3%${PASSWORD3} -k yes", ['Sharename', 'IPC'])

    t.info("Checking propogation of user deletion")
    t.run_cmd('bin/samba-tool user delete test2 -Uadministrator@${LCREALM}%${PASSWORD1}')
    child.sendline("net user test3 /del")
    child.expect("The command completed successfully")

    t.retry_cmd("bin/smbclient -L ${WIN_HOSTNAME}.${LCREALM} -Utest2%${PASSWORD2} -k no", ['LOGON_FAILURE'])
    t.retry_cmd("bin/smbclient -L ${HOSTNAME}.${LCREALM} -Utest3%${PASSWORD3} -k no", ['LOGON_FAILURE'])
    t.retry_cmd("bin/smbclient -L ${WIN_HOSTNAME}.${LCREALM} -Utest2%${PASSWORD2} -k yes", ['LOGON_FAILURE'])
    t.retry_cmd("bin/smbclient -L ${HOSTNAME}.${LCREALM} -Utest3%${PASSWORD3} -k yes", ['LOGON_FAILURE'])
    t.vm_poweroff("${WIN_VM}")


def run_dcpromo_rodc(t, vm):
    '''run a RODC dcpromo to join a windows DC to the samba domain'''
    t.setwinvars(vm)
    t.info("Joining a w2k8 box to the domain as a RODC")
    t.vm_poweroff("${WIN_VM}", checkfail=False)
    t.vm_restore("${WIN_VM}", "${WIN_SNAPSHOT}")
    child = t.open_telnet("${WIN_HOSTNAME}", "administrator", "${WIN_PASS}", set_ip=True)
    child.sendline("copy /Y con answers.txt")
    child.sendline('''
[DCInstall]
ReplicaOrNewDomain=ReadOnlyReplica
ReplicaDomainDNSName=${LCREALM}
PasswordReplicationDenied="BUILTIN\Administrators"
PasswordReplicationDenied="BUILTIN\Server Operators"
PasswordReplicationDenied="BUILTIN\Backup Operators"
PasswordReplicationDenied="BUILTIN\Account Operators"
PasswordReplicationDenied="${DOMAIN}\Denied RODC Password Replication Group"
PasswordReplicationAllowed="${DOMAIN}\Allowed RODC Password Replication Group"
DelegatedAdmin="${DOMAIN}\\Administrator"
SiteName=Default-First-Site-Name
InstallDNS=No
ConfirmGc=Yes
CreateDNSDelegation=No
UserDomain=${LCREALM}
UserName=${LCREALM}\\administrator
Password=${PASSWORD1}
DatabasePath="C:\Windows\NTDS"
LogPath="C:\Windows\NTDS"
SYSVOLPath="C:\Windows\SYSVOL"
SafeModeAdminPassword=${PASSWORD1}
RebootOnCompletion=No

''')
    child.expect("copied.")
    child.sendline("dcpromo /answer:answers.txt")
    i = child.expect(["You must restart this computer", "failed", "could not be located in this domain"], timeout=120)
    if i != 0:
        child.sendline("echo off")
        child.sendline("echo START DCPROMO log")
        child.sendline("more c:\windows\debug\dcpromoui.log")
        child.sendline("echo END DCPROMO log")
        child.expect("END DCPROMO")
        raise Exception("dcpromo failed")
    child.sendline("shutdown -r -t 0")
    t.wait_reboot()



def test_dcpromo_rodc(t, vm):
    '''test the RODC dcpromo worked'''
    t.info("Checking the w2k8 RODC join is OK")
    t.chdir('${PREFIX}')
    t.port_wait("${WIN_IP}", 139)
    child = t.open_telnet("${WIN_HOSTNAME}", "${DOMAIN}\\administrator", "${PASSWORD1}", set_time=True)
    child.sendline("ipconfig /registerdns")
    t.retry_cmd('bin/smbclient -L ${WIN_HOSTNAME}.${LCREALM} -Uadministrator@${LCREALM}%${PASSWORD1}', ["C$", "IPC$", "Sharename"])
    t.cmd_contains("host -t A ${WIN_HOSTNAME}.${LCREALM}.", ['has address'])
    t.cmd_contains('bin/smbclient -L ${WIN_HOSTNAME}.${LCREALM} -Utestallowed@${LCREALM}%${PASSWORD1}', ["C$", "IPC$", "Sharename"])
    child.sendline("net use t: \\\\${HOSTNAME}.${LCREALM}\\test")
    child.expect("The command completed successfully")

    t.info("Checking if showrepl is happy")
    child.sendline("repadmin /showrepl")
    child.expect("${BASEDN}")
    child.expect("was successful")
    child.expect("CN=Configuration,${BASEDN}")
    child.expect("was successful")
    child.expect("CN=Configuration,${BASEDN}")
    child.expect("was successful")

    for nc in [ '${BASEDN}', 'CN=Configuration,${BASEDN}', 'CN=Schema,CN=Configuration,${BASEDN}' ]:
        t.cmd_contains("bin/samba-tool drs replicate --add-ref ${WIN_HOSTNAME}.${LCREALM} ${HOSTNAME}.${LCREALM} %s" % nc, ["was successful"])

    t.cmd_contains("bin/samba-tool drs showrepl ${HOSTNAME}.${LCREALM}",
                 [ "INBOUND NEIGHBORS",
                   "OUTBOUND NEIGHBORS",
                   "${BASEDN}",
                   "Last attempt.*was successful",
                   "CN=Configuration,${BASEDN}",
                   "Last attempt.*was successful",
                   "CN=Configuration,${BASEDN}",
                   "Last attempt.*was successful" ],
                   ordered=True,
                   regex=True)

    t.info("Checking if new users are available on windows")
    t.run_cmd('bin/samba-tool newuser test2 ${PASSWORD2}')
    t.retry_cmd("bin/smbclient -L ${WIN_HOSTNAME}.${LCREALM} -Utest2%${PASSWORD2} -k yes", ['Sharename', 'Remote IPC'])
    t.retry_cmd("bin/samba-tool drs replicate ${WIN_HOSTNAME}.${LCREALM} ${HOSTNAME}.${LCREALM} ${BASEDN}", ["was successful"])
    t.retry_cmd("bin/smbclient -L ${WIN_HOSTNAME}.${LCREALM} -Utest2%${PASSWORD2} -k no", ['Sharename', 'Remote IPC'])
    t.run_cmd('bin/samba-tool user delete test2 -Uadministrator@${LCREALM}%${PASSWORD1}')
    t.retry_cmd("bin/smbclient -L ${WIN_HOSTNAME}.${LCREALM} -Utest2%${PASSWORD2} -k yes", ['LOGON_FAILURE'])
    t.retry_cmd("bin/smbclient -L ${WIN_HOSTNAME}.${LCREALM} -Utest2%${PASSWORD2} -k no", ['LOGON_FAILURE'])
    t.vm_poweroff("${WIN_VM}")


def prep_join_as_dc(t, vm):
    '''start VM and shutdown Samba in preperation to join a windows domain as a DC'''
    t.info("Starting VMs for joining ${WIN_VM} as a second DC using samba-tool join DC")
    t.chdir('${PREFIX}')
    t.run_cmd('killall -9 -q samba smbd nmbd winbindd', checkfail=False)
    t.rndc_cmd('flush')
    t.run_cmd("rm -rf etc/smb.conf private")
    child = t.open_telnet("${WIN_HOSTNAME}", "${WIN_DOMAIN}\\administrator", "${WIN_PASS}", set_time=True)
    t.get_ipconfig(child)

def join_as_dc(t, vm):
    '''join a windows domain as a DC'''
    t.setwinvars(vm)
    t.info("Joining ${WIN_VM} as a second DC using samba-tool join DC")
    t.port_wait("${WIN_IP}", 389)
    t.retry_cmd("host -t SRV _ldap._tcp.${WIN_REALM} ${WIN_IP}", ['has SRV record'] )

    t.retry_cmd("bin/samba-tool drs showrepl ${WIN_HOSTNAME}.${WIN_REALM} -Uadministrator%${WIN_PASS}", ['INBOUND NEIGHBORS'] )
    t.run_cmd('bin/samba-tool join ${WIN_REALM} DC -Uadministrator%${WIN_PASS} -d${DEBUGLEVEL} --option=interfaces=${INTERFACE}')
    t.run_cmd('bin/samba-tool drs kcc ${WIN_HOSTNAME}.${WIN_REALM} -Uadministrator@${WIN_REALM}%${WIN_PASS}')


def test_join_as_dc(t, vm):
    '''test the join of a windows domain as a DC'''
    t.info("Checking the DC join is OK")
    t.chdir('${PREFIX}')
    t.retry_cmd('bin/smbclient -L ${HOSTNAME}.${WIN_REALM} -Uadministrator@${WIN_REALM}%${WIN_PASS}', ["C$", "IPC$", "Sharename"])
    t.cmd_contains("host -t A ${HOSTNAME}.${WIN_REALM}.", ['has address'])
    child = t.open_telnet("${WIN_HOSTNAME}", "${WIN_DOMAIN}\\administrator", "${WIN_PASS}", set_time=True)

    t.info("Forcing kcc runs, and replication")
    t.run_cmd('bin/samba-tool drs kcc ${WIN_HOSTNAME}.${WIN_REALM} -Uadministrator@${WIN_REALM}%${WIN_PASS}')
    t.run_cmd('bin/samba-tool drs kcc ${HOSTNAME}.${WIN_REALM} -Uadministrator@${WIN_REALM}%${WIN_PASS}')

    t.kinit("administrator@${WIN_REALM}", "${WIN_PASS}")
    for nc in [ '${WIN_BASEDN}', 'CN=Configuration,${WIN_BASEDN}', 'CN=Schema,CN=Configuration,${WIN_BASEDN}' ]:
        t.cmd_contains("bin/samba-tool drs replicate ${HOSTNAME}.${WIN_REALM} ${WIN_HOSTNAME}.${WIN_REALM} %s -k yes" % nc, ["was successful"])
        t.cmd_contains("bin/samba-tool drs replicate ${WIN_HOSTNAME}.${WIN_REALM} ${HOSTNAME}.${WIN_REALM} %s -k yes" % nc, ["was successful"])

    retries = 10
    i = 1
    while i == 1 and retries > 0:
        child.sendline("net use t: \\\\${HOSTNAME}.${WIN_REALM}\\test")
        i = child.expect(["The command completed successfully", "The network path was not found"])
        child.expect("C:")
        if i == 1:
            time.sleep(2)
        retries -=1

    t.info("Checking if showrepl is happy")
    child.sendline("repadmin /showrepl")
    child.expect("${WIN_BASEDN}")
    child.expect("was successful")
    child.expect("CN=Configuration,${WIN_BASEDN}")
    child.expect("was successful")
    child.expect("CN=Configuration,${WIN_BASEDN}")
    child.expect("was successful")

    t.info("Checking if new users propogate to windows")
    t.retry_cmd('bin/samba-tool newuser test2 ${PASSWORD2}', ["created successfully"])
    t.retry_cmd("bin/smbclient -L ${WIN_HOSTNAME}.${WIN_REALM} -Utest2%${PASSWORD2} -k no", ['Sharename', 'Remote IPC'])
    t.retry_cmd("bin/smbclient -L ${WIN_HOSTNAME}.${WIN_REALM} -Utest2%${PASSWORD2} -k yes", ['Sharename', 'Remote IPC'])

    t.info("Checking if new users on windows propogate to samba")
    child.sendline("net user test3 ${PASSWORD3} /add")
    child.expect("The command completed successfully")
    t.retry_cmd("bin/smbclient -L ${HOSTNAME}.${WIN_REALM} -Utest3%${PASSWORD3} -k no", ['Sharename', 'IPC'])
    t.retry_cmd("bin/smbclient -L ${HOSTNAME}.${WIN_REALM} -Utest3%${PASSWORD3} -k yes", ['Sharename', 'IPC'])

    t.info("Checking propogation of user deletion")
    t.run_cmd('bin/samba-tool user delete test2 -Uadministrator@${WIN_REALM}%${WIN_PASS}')
    child.sendline("net user test3 /del")
    child.expect("The command completed successfully")

    t.retry_cmd("bin/smbclient -L ${WIN_HOSTNAME}.${WIN_REALM} -Utest2%${PASSWORD2} -k no", ['LOGON_FAILURE'])
    t.retry_cmd("bin/smbclient -L ${HOSTNAME}.${WIN_REALM} -Utest3%${PASSWORD3} -k no", ['LOGON_FAILURE'])
    t.retry_cmd("bin/smbclient -L ${WIN_HOSTNAME}.${WIN_REALM} -Utest2%${PASSWORD2} -k yes", ['LOGON_FAILURE'])
    t.retry_cmd("bin/smbclient -L ${HOSTNAME}.${WIN_REALM} -Utest3%${PASSWORD3} -k yes", ['LOGON_FAILURE'])
    t.vm_poweroff("${WIN_VM}")


def join_as_rodc(t, vm):
    '''join a windows domain as a RODC'''
    t.setwinvars(vm)
    t.info("Joining ${WIN_VM} as a RODC using samba-tool join DC")
    t.port_wait("${WIN_IP}", 389)
    t.retry_cmd("host -t SRV _ldap._tcp.${WIN_REALM} ${WIN_IP}", ['has SRV record'] )
    t.retry_cmd("bin/samba-tool drs showrepl ${WIN_HOSTNAME}.${WIN_REALM} -Uadministrator%${WIN_PASS}", ['INBOUND NEIGHBORS'] )
    t.run_cmd('bin/samba-tool join ${WIN_REALM} RODC -Uadministrator%${WIN_PASS} -d${DEBUGLEVEL} --option=interfaces=${INTERFACE}')
    t.run_cmd('bin/samba-tool drs kcc ${WIN_HOSTNAME}.${WIN_REALM} -Uadministrator@${WIN_REALM}%${WIN_PASS}')


def test_join_as_rodc(t, vm):
    '''test a windows domain RODC join'''
    t.info("Checking the RODC join is OK")
    t.chdir('${PREFIX}')
    t.retry_cmd('bin/smbclient -L ${HOSTNAME}.${WIN_REALM} -Uadministrator@${WIN_REALM}%${WIN_PASS}', ["C$", "IPC$", "Sharename"])
    t.cmd_contains("host -t A ${HOSTNAME}.${WIN_REALM}.", ['has address'])
    child = t.open_telnet("${WIN_HOSTNAME}", "${WIN_DOMAIN}\\administrator", "${WIN_PASS}", set_time=True)

    t.info("Forcing kcc runs, and replication")
    t.run_cmd('bin/samba-tool drs kcc ${HOSTNAME}.${WIN_REALM} -Uadministrator@${WIN_REALM}%${WIN_PASS}')
    t.run_cmd('bin/samba-tool drs kcc ${WIN_HOSTNAME}.${WIN_REALM} -Uadministrator@${WIN_REALM}%${WIN_PASS}')

    t.kinit("administrator@${WIN_REALM}", "${WIN_PASS}")
    for nc in [ '${WIN_BASEDN}', 'CN=Configuration,${WIN_BASEDN}', 'CN=Schema,CN=Configuration,${WIN_BASEDN}' ]:
        t.cmd_contains("bin/samba-tool drs replicate ${HOSTNAME}.${WIN_REALM} ${WIN_HOSTNAME}.${WIN_REALM} %s -k yes" % nc, ["was successful"])

    retries = 10
    i = 1
    while i == 1 and retries > 0:
        child.sendline("net use t: \\\\${HOSTNAME}.${WIN_REALM}\\test")
        i = child.expect(["The command completed successfully", "The network path was not found"])
        child.expect("C:")
        if i == 1:
            time.sleep(2)
        retries -=1

    t.info("Checking if showrepl is happy")
    child.sendline("repadmin /showrepl")
    child.expect("DSA invocationID")

    t.cmd_contains("bin/samba-tool drs showrepl ${WIN_HOSTNAME}.${WIN_REALM} -k yes",
                 [ "INBOUND NEIGHBORS",
                   "OUTBOUND NEIGHBORS",
                   "${WIN_BASEDN}",
                   "Last attempt .* was successful",
                   "CN=Configuration,${WIN_BASEDN}",
                   "Last attempt .* was successful",
                   "CN=Configuration,${WIN_BASEDN}",
                   "Last attempt .* was successful" ],
                   ordered=True,
                   regex=True)

    t.info("Checking if new users on windows propogate to samba")
    child.sendline("net user test3 ${PASSWORD3} /add")
    child.expect("The command completed successfully")
    t.retry_cmd("bin/smbclient -L ${HOSTNAME}.${WIN_REALM} -Utest3%${PASSWORD3} -k no", ['Sharename', 'IPC'])
    t.retry_cmd("bin/smbclient -L ${HOSTNAME}.${WIN_REALM} -Utest3%${PASSWORD3} -k yes", ['Sharename', 'IPC'])

    # should this work?
    t.info("Checking if new users propogate to windows")
    t.cmd_contains('bin/samba-tool newuser test2 ${PASSWORD2}', ['No RID Set DN'])

    t.info("Checking propogation of user deletion")
    child.sendline("net user test3 /del")
    child.expect("The command completed successfully")

    t.retry_cmd("bin/smbclient -L ${HOSTNAME}.${WIN_REALM} -Utest3%${PASSWORD3} -k no", ['LOGON_FAILURE'])
    t.retry_cmd("bin/smbclient -L ${HOSTNAME}.${WIN_REALM} -Utest3%${PASSWORD3} -k yes", ['LOGON_FAILURE'])
    t.vm_poweroff("${WIN_VM}")


def test_howto(t):
    '''test the Samba4 howto'''

    t.setvar("SAMBA_VERSION", "Version 4")
    t.check_prerequesites()

    # we don't need fsync safety in these tests
    t.putenv('TDB_NO_FSYNC', '1')

    if not t.skip("configure_bind"):
        t.configure_bind(kerberos_support=True, include='${PREFIX}/private/named.conf')
    if not t.skip("stop_bind"):
        t.stop_bind()
    if not t.skip("stop_vms"):
        t.stop_vms()

    if not t.skip("build"):
        build_s4(t)

    if not t.skip("provision"):
        provision_s4(t)

    set_krb5_conf(t)

    if not t.skip("create-shares"):
        create_shares(t)

    if not t.skip("starts4"):
        start_s4(t)
    if not t.skip("smbclient"):
        test_smbclient(t)
    if not t.skip("configure_bind2"):
        t.configure_bind(kerberos_support=True, include='${PREFIX}/private/named.conf')
    if not t.skip("start_bind"):
        t.start_bind()
    if not t.skip("dns"):
        test_dns(t)
    if not t.skip("kerberos"):
        test_kerberos(t)
    if not t.skip("dyndns"):
        test_dyndns(t)

    if t.have_vm('WINDOWS7') and not t.skip("windows7"):
        t.start_winvm("WINDOWS7")
        t.test_remote_smbclient("WINDOWS7")
        run_winjoin(t, "WINDOWS7")
        test_winjoin(t, "WINDOWS7")
        t.vm_poweroff("${WIN_VM}")

    if t.have_vm('WINXP') and not t.skip("winxp"):
        t.start_winvm("WINXP")
        run_winjoin(t, "WINXP")
        test_winjoin(t, "WINXP")
        t.test_remote_smbclient("WINXP", "administrator", "${PASSWORD1}")
        t.vm_poweroff("${WIN_VM}")

    if t.have_vm('W2K3C') and not t.skip("win2k3_member"):
        t.start_winvm("W2K3C")
        run_winjoin(t, "W2K3C")
        test_winjoin(t, "W2K3C")
        t.test_remote_smbclient("W2K3C", "administrator", "${PASSWORD1}")
        t.vm_poweroff("${WIN_VM}")

    if t.have_vm('W2K8R2C') and not t.skip("dcpromo_rodc"):
        t.info("Testing w2k8r2 RODC dcpromo")
        t.start_winvm("W2K8R2C")
        t.test_remote_smbclient('W2K8R2C')
        run_dcpromo_rodc(t, "W2K8R2C")
        test_dcpromo_rodc(t, "W2K8R2C")

    if t.have_vm('W2K8R2B') and not t.skip("dcpromo_w2k8r2"):
        t.info("Testing w2k8r2 dcpromo")
        t.start_winvm("W2K8R2B")
        t.test_remote_smbclient('W2K8R2B')
        run_dcpromo(t, "W2K8R2B")
        test_dcpromo(t, "W2K8R2B")

    if t.have_vm('W2K8B') and not t.skip("dcpromo_w2k8"):
        t.info("Testing w2k8 dcpromo")
        t.start_winvm("W2K8B")
        t.test_remote_smbclient('W2K8B')
        run_dcpromo(t, "W2K8B")
        test_dcpromo(t, "W2K8B")

    if t.have_vm('W2K3B') and not t.skip("dcpromo_w2k3"):
        t.info("Testing w2k3 dcpromo")
        t.info("Changing to 2003 functional level")
        provision_s4(t, func_level='2003')
        create_shares(t)
        start_s4(t)
        test_smbclient(t)
        t.restart_bind(kerberos_support=True, include='${PREFIX}/private/named.conf')
        test_dns(t)
        test_kerberos(t)
        test_dyndns(t)
        t.start_winvm("W2K3B")
        t.test_remote_smbclient('W2K3B')
        run_dcpromo(t, "W2K3B")
        test_dcpromo(t, "W2K3B")

    if t.have_vm('W2K8R2A') and not t.skip("join_w2k8r2"):
        t.start_winvm("W2K8R2A")
        prep_join_as_dc(t, "W2K8R2A")
        t.run_dcpromo_as_first_dc("W2K8R2A", func_level='2008r2')
        join_as_dc(t, "W2K8R2A")
        create_shares(t)
        start_s4(t)
        test_dyndns(t)
        test_join_as_dc(t, "W2K8R2A")

    if t.have_vm('W2K8R2A') and not t.skip("join_rodc"):
        t.start_winvm("W2K8R2A")
        prep_join_as_dc(t, "W2K8R2A")
        t.run_dcpromo_as_first_dc("W2K8R2A", func_level='2008r2')
        join_as_rodc(t, "W2K8R2A")
        create_shares(t)
        start_s4(t)
        test_dyndns(t)
        test_join_as_rodc(t, "W2K8R2A")

    if t.have_vm('W2K3A') and not t.skip("join_w2k3"):
        prep_join_as_dc(t, "W2K3A")
        t.run_dcpromo_as_first_dc("W2K3A", func_level='2003')
        join_as_dc(t, "W2K3A")
        create_shares(t)
        start_s4(t)
        test_dyndns(t)
        test_join_as_dc(t, "W2K3A")

    t.info("Howto test: All OK")


def test_cleanup(t):
    '''cleanup after tests'''
    t.info("Cleaning up ...")
    t.restore_resolv_conf()
    if getattr(t, 'bind_child', False):
        t.bind_child.kill()


if __name__ == '__main__':
    t = wintest.wintest()

    t.setup("test-s4-howto.py", "source4")

    try:
        test_howto(t)
    except:
        if not t.opts.nocleanup:
            test_cleanup(t)
        raise

    if not t.opts.nocleanup:
        test_cleanup(t)
    t.info("S4 howto test: All OK")
