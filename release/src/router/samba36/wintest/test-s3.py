#!/usr/bin/env python

'''automated testing of Samba3 against windows'''

import sys, os
import optparse
import wintest

def set_libpath(t):
    t.putenv("LD_LIBRARY_PATH", "${PREFIX}/lib")

def set_krb5_conf(t):
    t.run_cmd("mkdir -p ${PREFIX}/etc")
    t.write_file("${PREFIX}/etc/krb5.conf", 
                    '''[libdefaults]
	dns_lookup_realm = false
	dns_lookup_kdc = true''')

    t.putenv("KRB5_CONFIG", '${PREFIX}/etc/krb5.conf')

def build_s3(t):
    '''build samba3'''
    t.info('Building s3')
    t.chdir('${SOURCETREE}/source3')
    t.putenv('CC', 'ccache gcc')
    t.run_cmd("./autogen.sh")
    t.run_cmd("./configure -C --prefix=${PREFIX} --enable-developer")
    t.run_cmd('make basics')
    t.run_cmd('make -j4')
    t.run_cmd('rm -rf ${PREFIX}')
    t.run_cmd('make install')

def start_s3(t):
    t.info('Starting Samba3')
    t.chdir("${PREFIX}")
    t.run_cmd('killall -9 -q samba smbd nmbd winbindd', checkfail=False)
    t.run_cmd("rm -f var/locks/*.pid")
    t.run_cmd(['sbin/nmbd', "-D"])
    t.run_cmd(['sbin/winbindd', "-D"])
    t.run_cmd(['sbin/smbd', "-D"])
    t.port_wait("${INTERFACE_IP}", 139)

def test_wbinfo(t):
    t.info('Testing wbinfo')
    t.chdir('${PREFIX}')
    t.cmd_contains("bin/wbinfo --version", ["Version 3."])
    t.cmd_contains("bin/wbinfo -p", ["Ping to winbindd succeeded"])
    t.retry_cmd("bin/wbinfo --online-status",
                ["BUILTIN : online",
                 "${HOSTNAME} : online",
                 "${WIN_DOMAIN} : online"],
                casefold=True)
    t.cmd_contains("bin/wbinfo -u",
                   ["${WIN_DOMAIN}/administrator",
                    "${WIN_DOMAIN}/krbtgt" ],
                   casefold=True)
    t.cmd_contains("bin/wbinfo -g",
                   ["${WIN_DOMAIN}/domain users",
                    "${WIN_DOMAIN}/domain guests",
                    "${WIN_DOMAIN}/domain admins"],
                   casefold=True)
    t.cmd_contains("bin/wbinfo --name-to-sid administrator",
                   "S-1-5-.*-500 SID_USER .1",
                   regex=True)
    t.cmd_contains("bin/wbinfo --name-to-sid 'domain users'",
                   "S-1-5-.*-513 SID_DOM_GROUP .2",
                   regex=True)

    t.retry_cmd("bin/wbinfo --authenticate=${WIN_DOMAIN}/administrator%${WIN_PASS}",
                ["plaintext password authentication succeeded",
                 "challenge/response password authentication succeeded"])


def test_smbclient(t):
    t.info('Testing smbclient')
    t.chdir('${PREFIX}')
    t.cmd_contains("bin/smbclient --version", ["Version 3."])
    t.cmd_contains('bin/smbclient -L ${INTERFACE_IP} -U%', ["Domain=[${WIN_DOMAIN}]", "test", "IPC$", "Samba 3."],
                   casefold=True)
    child = t.pexpect_spawn('bin/smbclient //${HOSTNAME}.${WIN_REALM}/test -Uroot@${WIN_REALM}%${PASSWORD2}')
    child.expect("smb:")
    child.sendline("dir")
    child.expect("blocks available")
    child.sendline("mkdir testdir")
    child.expect("smb:")
    child.sendline("cd testdir")
    child.expect('testdir')
    child.sendline("cd ..")
    child.sendline("rmdir testdir")

    child = t.pexpect_spawn('bin/smbclient //${HOSTNAME}.${WIN_REALM}/test -Uroot@${WIN_REALM}%${PASSWORD2} -k')
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
    t.info("Adding test shares")
    t.chdir('${PREFIX}')
    t.write_file("lib/smb.conf", '''
[test]
       path = ${PREFIX}/test
       read only = no
       ''',
                 mode='a')
    t.run_cmd("mkdir -p test")


def prep_join_as_member(t, vm):
    '''prepare to join a windows domain as a member server'''
    t.setwinvars(vm)
    t.info("Starting VMs for joining ${WIN_VM} as a member using net ads join")
    t.chdir('${PREFIX}')
    t.run_cmd('killall -9 -q samba smbd nmbd winbindd', checkfail=False)
    t.vm_poweroff("${WIN_VM}", checkfail=False)
    t.vm_restore("${WIN_VM}", "${WIN_SNAPSHOT}")
    child = t.open_telnet("${WIN_HOSTNAME}", "administrator", "${WIN_PASS}", set_time=True)
    t.get_ipconfig(child)
    t.del_files(["var", "private"])
    t.write_file("lib/smb.conf", '''
[global]
	netbios name = ${HOSTNAME}
	log level = ${DEBUGLEVEL}
        realm = ${WIN_REALM}
        workgroup = ${WIN_DOMAIN}
        security = ADS
        bind interfaces only = yes
        interfaces = ${INTERFACE}
        winbind separator = /
        idmap uid = 1000000-2000000
        idmap gid = 1000000-2000000
        winbind enum users = yes
        winbind enum groups = yes
        max protocol = SMB2
        map hidden = no
        map system = no
        ea support = yes
        panic action = xterm -e gdb --pid %d
    ''')

def join_as_member(t, vm):
    '''join a windows domain as a member server'''
    t.setwinvars(vm)
    t.info("Joining ${WIN_VM} as a member using net ads join")
    t.port_wait("${WIN_IP}", 389)
    t.retry_cmd("host -t SRV _ldap._tcp.${WIN_REALM} ${WIN_IP}", ['has SRV record'] )
    t.cmd_contains("bin/net ads join -Uadministrator%${WIN_PASS}", ["Joined"])
    t.cmd_contains("bin/net ads testjoin", ["Join is OK"])
    t.cmd_contains("bin/net ads dns register ${HOSTNAME}.${WIN_REALM} -P", ["Successfully registered hostname with DNS"])
    t.cmd_contains("host -t A ${HOSTNAME}.${WIN_REALM}",
                 ['${HOSTNAME}.${WIN_REALM} has address'])


def test_join_as_member(t, vm):
    '''test the domain join'''
    t.setwinvars(vm)
    t.info('Testing join as member')
    t.chdir('${PREFIX}')
    t.run_cmd('bin/net ads user add root -Uadministrator%${WIN_PASS}')
    child = t.pexpect_spawn('bin/net ads password root -Uadministrator%${WIN_PASS}')
    child.expect("Enter new password for root")
    child.sendline("${PASSWORD2}")
    child.expect("Password change for ");
    child.expect(" completed")
    child = t.pexpect_spawn('bin/net rpc shell -S ${WIN_HOSTNAME}.${WIN_REALM} -Uadministrator%${WIN_PASS}')
    child.expect("net rpc>")
    child.sendline("user edit disabled root no")
    child.expect("Set root's disabled flag")
    test_wbinfo(t)
    test_smbclient(t)


def test_s3(t):
    '''basic s3 testing'''

    t.setvar("SAMBA_VERSION", "Version 3")
    t.check_prerequesites()
    set_libpath(t)

    if not t.skip("configure_bind"):
        t.configure_bind()
    if not t.skip("stop_bind"):
        t.stop_bind()
    if not t.skip("stop_vms"):
        t.stop_vms()

    if not t.skip("build"):
        build_s3(t)

    set_krb5_conf(t)
    if not t.skip("configure_bind2"):
        t.configure_bind()
    if not t.skip("start_bind"):
        t.start_bind()

    dc_started = False
    if t.have_var('W2K8R2A_VM') and not t.skip("join_w2k8r2"):
        t.start_winvm('W2K8R2A')
        dc_started = True
        prep_join_as_member(t, "W2K8R2A")
        t.run_dcpromo_as_first_dc("W2K8R2A", func_level='2008r2')
        join_as_member(t, "W2K8R2A")
        create_shares(t)
        start_s3(t)
        test_join_as_member(t, "W2K8R2A")

    if t.have_var('WINDOWS7_VM') and t.have_var('W2K8R2A_VM') and not t.skip("join_windows7_2008r2"):
        if not dc_started:
            t.start_winvm('W2K8R2A')
            t.run_dcpromo_as_first_dc("W2K8R2A", func_level='2008r2')
            dc_started = True
        else:
            t.setwinvars('W2K8R2A')
        realm = t.getvar("WIN_REALM")
        dom_username = t.getvar("WIN_USER")
        dom_password = t.getvar("WIN_PASS")
        dom_realm = t.getvar("WIN_REALM")
        t.start_winvm('WINDOWS7')
        t.test_remote_smbclient("WINDOWS7")
        t.run_winjoin('WINDOWS7', realm, username=dom_username, password=dom_password)
        t.test_remote_smbclient("WINDOWS7", dom_username, dom_password)
        t.test_remote_smbclient('WINDOWS7', dom_username, dom_password, args='--option=clientntlmv2auth=no')
        t.test_remote_smbclient('WINDOWS7', "%s@%s" % (dom_username, dom_realm), dom_password, args="-k")
        t.test_remote_smbclient('WINDOWS7', "%s@%s" % (dom_username, dom_realm), dom_password, args="-k --option=clientusespnegoprincipal=yes")

    if t.have_var('WINXP_VM') and t.have_var('W2K8R2A_VM') and not t.skip("join_winxp_2008r2"):
        if not dc_started:
            t.start_winvm('W2K8R2A')
            t.run_dcpromo_as_first_dc("W2K8R2A", func_level='2008r2')
            dc_started = True
        else:
            t.setwinvars('W2K8R2A')
        realm = t.getvar("WIN_REALM")
        dom_username = t.getvar("WIN_USER")
        dom_password = t.getvar("WIN_PASS")
        dom_realm = t.getvar("WIN_REALM")
        t.start_winvm('WINXP')
        t.run_winjoin('WINXP', realm, username=dom_username, password=dom_password)
        t.test_remote_smbclient('WINXP', dom_username, dom_password)
        t.test_remote_smbclient('WINXP', dom_username, dom_password, args='--option=clientntlmv2auth=no')
        t.test_remote_smbclient('WINXP', "%s@%s" % (dom_username, dom_realm), dom_password, args="-k")
        t.test_remote_smbclient('WINXP', "%s@%s" % (dom_username, dom_realm), dom_password, args="-k --clientusespnegoprincipal=yes")

    t.info("S3 test: All OK")


def test_cleanup(t):
    '''cleanup after tests'''
    t.info("Cleaning up ...")
    t.restore_resolv_conf()
    if getattr(t, 'bind_child', False):
        t.bind_child.kill()


if __name__ == '__main__':
    t = wintest.wintest()

    t.setup("test-s3.py", "source3")

    try:
        test_s3(t)
    except:
        if not t.opts.nocleanup:
            test_cleanup(t)
        raise

    if not t.opts.nocleanup:
        test_cleanup(t)
    t.info("S3 test: All OK")
