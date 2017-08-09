import string, Utils

# list of directory options to offer in configure
dir_options = {
    'with-cachedir'                       : [ '${PREFIX}/var/locks', 'where to put temporary cache files' ],
    'with-codepagedir'                    : [ '${PREFIX}/lib/samba', 'where to put codepages' ],
    'with-configdir'                      : [ '${PREFIX}/etc/samba', 'Where to put configuration files' ],
    'with-lockdir'                        : [ '${PREFIX}/var/locks', 'where to put lock files' ],
    'with-logfilebase'                    : [ '${PREFIX}/var/log/samba', 'Where to put log files' ],
    'with-ncalrpcdir'                     : [ '${PREFIX}/var/ncalrpc', 'where to put ncalrpc sockets' ],
    'with-nmbdsocketdir'                  : [ '${PREFIX}/var/locks/.nmbd', 'Where to put the nmbd socket directory' ],
    'with-ntp-signd-socket-dir'           : [ '${PREFIX}/var/run/ntp_signd', 'NTP signed directory'],
    'with-pammodulesdir'                  : [ '', 'Which directory to use for PAM modules' ],
    'with-piddir'                         : [ '${PREFIX}/var/locks', 'where to put pid files' ],
    'with-privatedir'                     : [ '${PREFIX}/private', 'where to put smbpasswd' ],
    'with-selftest-prefix'                : [ '', 'The prefix where make test will be run' ],
    'with-selftest-shrdir'                : [ '', 'The share directory that make test will be run against' ],
    'with-statedir'                       : [ '${PREFIX}/var/locks', 'where to put persistent state files' ],
    'with-swatdir'                        : [ '${PREFIX}/swat', 'Where to put SWAT files' ],
    'with-winbindd-privileged-socket-dir' : [ '${PREFIX}/var/lib/winbindd_privileged', 'winbind privileged socket directory'],
    'with-winbindd-socket-dir'            : [ '${PREFIX}/var/lib/winbindd', 'winbind socket directory' ],
    }

# list of cflags to use for dynconfig.c
dyn_cflags = {
    'BINDIR'                         : '${BINDIR}',
    'CACHEDIR'                       : '${CACHEDIR}',
    'CODEPAGEDIR'                    : '${CODEPAGEDIR}',
    'CONFIGDIR'                      : '${SYSCONFDIR}',
    'CONFIGFILE'                     : '${SYSCONFDIR}/smb.conf',
    'DATADIR'                        : '${DATADIR}',
    'LIBDIR'                         : '${LIBDIR}',
    'LOCALEDIR'                      : '${LOCALEDIR}',
    'LMHOSTSFILE'                    : '${SYSCONFDIR}/lmhosts',
    'LOCKDIR'                        : '${LOCALSTATEDIR}/locks',
    'LOGFILEBASE'                    : '${LOCALSTATEDIR}',
    'MODULESDIR'                     : '${PREFIX}/modules',
    'NCALRPCDIR'                     : '${LOCALSTATEDIR}/ncalrpc',
    'NMBDSOCKETDIR'                  : '${LOCKDIR}/.nmbd',
    'NTP_SIGND_SOCKET_DIR'           : '${NTP_SIGND_SOCKET_DIR}',
    'PIDDIR'                         : '${LOCALSTATEDIR}/run',
    'PKGCONFIGDIR'                   : '${LIBDIR}/pkgconfigdir',
    'PRIVATE_DIR'                    : '${PRIVATEDIR}',
    'SBINDIR'                        : '${SBINDIR}',
    'SETUPDIR'                       : '${DATADIR}/setup',
    'SMB_PASSWD_FILE'                : '${PRIVATEDIR}/smbpasswd',
    'STATEDIR'                       : '${LOCALSTATEDIR}',
    'SWATDIR'                        : '${PREFIX}/swat',
    'WINBINDD_PRIVILEGED_SOCKET_DIR' : '${WINBINDD_PRIVILEGED_SOCKET_DIR}',
    'WINBINDD_SOCKET_DIR'            : '${WINBINDD_SOCKET_DIR}',
    }

def get_varname(v):
    '''work out a variable name from a configure option name'''
    if v.startswith('with-'):
        v = v[5:]
    v = v.upper()
    v = string.replace(v, '-', '_')
    return v


def dynconfig_cflags(bld):
    '''work out the extra CFLAGS for dynconfig.c'''
    cflags = []
    for f in dyn_cflags.keys():
        # substitute twice, as we could have substitutions containing variables
        v = Utils.subst_vars(dyn_cflags[f], bld.env)
        v = Utils.subst_vars(v, bld.env)
        bld.ASSERT(v != '', "Empty dynconfig value for %s" % f)
        bld.ASSERT(v.find('${') == -1, "Unsubstituted variable in %s : %s : %s" % (f, dyn_cflags[f], v))
        cflags.append('-D%s="%s"' % (f, v))
    return cflags
