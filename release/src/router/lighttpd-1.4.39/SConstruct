import os
import sys
import re
import string
from stat import *

package = 'lighttpd'
version = '1.4.39'

def checkCHeaders(autoconf, hdrs):
	p = re.compile('[^A-Z0-9]')
	for hdr in hdrs:
		if not hdr:
			continue
		_hdr = Split(hdr)
		if autoconf.CheckCHeader(_hdr):
			autoconf.env.Append(CPPFLAGS = [ '-DHAVE_' + p.sub('_', _hdr[-1].upper()) ])

def checkFuncs(autoconf, funcs):
	p = re.compile('[^A-Z0-9]')
	for func in funcs:
		if autoconf.CheckFunc(func):
			autoconf.env.Append(CPPFLAGS = [ '-DHAVE_' + p.sub('_', func.upper()) ])

def checkTypes(autoconf, types):
	p = re.compile('[^A-Z0-9]')
	for type in types:
		if autoconf.CheckType(type, '#include <sys/types.h>'):
			autoconf.env.Append(CPPFLAGS = [ '-DHAVE_' + p.sub('_', type.upper()) ])

def checkGmtOffInStructTm(context):
	source = """
#include <time.h>
int main() {
	struct tm a;
	a.tm_gmtoff = 0;
	return 0;
}
"""
	context.Message('Checking for tm_gmtoff in struct tm...')
	result = context.TryLink(source, '.c')
	context.Result(result)

	return result

def checkIPv6(context):
	source = """
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main() {
	struct sockaddr_in6 s; struct in6_addr t=in6addr_any; int i=AF_INET6; s; t.s6_addr[0] = 0;
	return 0;
}
"""
	context.Message('Checking for IPv6 support...')
	result = context.TryLink(source, '.c')
	context.Result(result)

	return result

def checkWeakSymbols(context):
	source = """
__attribute__((weak)) void __dummy(void *x) { }
int main() {
	void *x;
	__dummy(x);
}
"""
	context.Message('Checking for weak symbol support...')
	result = context.TryLink(source, '.c')
	context.Result(result)

	return result

def checkProgram(env, withname, progname):
	withname = 'with_' + withname
	binpath = None

	if env[withname] != 1:
		binpath = env[withname]
	else:
		prog = env.Detect(progname)
		if prog:
			binpath = env.WhereIs(prog)

	if binpath:
		mode = os.stat(binpath)[ST_MODE]
		if S_ISDIR(mode):
			print >> sys.stderr, "* error: path `%s' is a directory" % (binpath)
			env.Exit(-1)
		if not S_ISREG(mode):
			print >> sys.stderr, "* error: path `%s' is not a file or not exists" % (binpath)
			env.Exit(-1)

	if not binpath:
		print >> sys.stderr, "* error: can't find program `%s'" % (progname)
		env.Exit(-1)

	return binpath

VariantDir('sconsbuild/build', 'src', duplicate = 0)
VariantDir('sconsbuild/tests', 'tests', duplicate = 0)

vars = Variables() #('config.py')
vars.AddVariables(
	('prefix', 'prefix', '/usr/local'),
	('bindir', 'binary directory', '${prefix}/bin'),
	('sbindir', 'binary directory', '${prefix}/sbin'),
	('libdir', 'library directory', '${prefix}/lib'),
	PackageVariable('with_mysql', 'enable mysql support', 'no'),
	PackageVariable('with_xml', 'enable xml support', 'no'),
	PackageVariable('with_pcre', 'enable pcre support', 'yes'),
	PathVariable('CC', 'path to the c-compiler', None),
	BoolVariable('build_dynamic', 'enable dynamic build', 'yes'),
	BoolVariable('build_static', 'enable static build', 'no'),
	BoolVariable('build_fullstatic', 'enable fullstatic build', 'no'),
	BoolVariable('with_sqlite3', 'enable sqlite3 support', 'no'),
	BoolVariable('with_memcache', 'enable memcache support', 'no'),
	BoolVariable('with_fam', 'enable FAM/gamin support', 'no'),
	BoolVariable('with_openssl', 'enable memcache support', 'no'),
	BoolVariable('with_gzip', 'enable gzip compression', 'no'),
	BoolVariable('with_bzip2', 'enable bzip2 compression', 'no'),
	BoolVariable('with_lua', 'enable lua support for mod_cml', 'no'),
	BoolVariable('with_ldap', 'enable ldap auth support', 'no'))

env = Environment(
	ENV = os.environ,
	variables = vars,
	CPPPATH = Split('#sconsbuild/build')
)

env.Help(vars.GenerateHelpText(env))

if env.subst('${CC}') is not '':
	env['CC'] = env.subst('${CC}')

env['package'] = package
env['version'] = version
if env['CC'] == 'gcc':
	## we need x-open 6 and bsd 4.3 features
	env.Append(CCFLAGS = Split('-Wall -O2 -g -W -pedantic -Wunused -Wshadow -std=gnu99'))

# cache configure checks
if 1:
	autoconf = Configure(env, custom_tests = {
		'CheckGmtOffInStructTm': checkGmtOffInStructTm,
		'CheckIPv6': checkIPv6,
		'CheckWeakSymbols': checkWeakSymbols,
	})

	if 'CFLAGS' in os.environ:
		autoconf.env.Append(CCFLAGS = os.environ['CFLAGS'])
		print(">> Appending custom build flags : " + os.environ['CFLAGS'])

	if 'LDFLAGS' in os.environ:
		autoconf.env.Append(LINKFLAGS = os.environ['LDFLAGS'])
		print(">> Appending custom link flags : " + os.environ['LDFLAGS'])

	if 'LIBS' in os.environ:
		autoconf.env.Append(APPEND_LIBS = os.environ['LIBS'])
		print(">> Appending custom libraries : " + os.environ['LIBS'])
	else:
		autoconf.env.Append(APPEND_LIBS = '')

	autoconf.headerfile = "foo.h"
	checkCHeaders(autoconf, string.split("""
			arpa/inet.h
			crypt.h
			fcntl.h
			getopt.h
			inttypes.h
			netinet/in.h
			poll.h
			pwd.h
			stdint.h
			stdlib.h
			string.h
			sys/devpoll.h
			sys/epoll.h
			sys/event.h
			sys/filio.h
			sys/mman.h
			sys/poll.h
			sys/port.h
			sys/prctl.h
			sys/resource.h
			sys/select.h
			sys/sendfile.h
			sys/socket.h
			sys/time.h
			sys/time.h sys/types.h sys/resource.h
			sys/types.h netinet/in.h
			sys/types.h sys/event.h
			sys/types.h sys/mman.h
			sys/types.h sys/select.h
			sys/types.h sys/socket.h
			sys/types.h sys/uio.h
			sys/types.h sys/un.h
			sys/uio.h
			sys/un.h
			sys/wait.h
			syslog.h
			unistd.h
			winsock2.h""", "\n"))

	checkFuncs(autoconf, Split('fork stat lstat strftime dup2 getcwd inet_ntoa inet_ntop memset mmap munmap strchr \
			strdup strerror strstr strtol sendfile getopt socket \
			gethostbyname poll epoll_ctl getrlimit chroot \
			getuid select signal pathconf madvise prctl\
			writev sigaction sendfile64 send_file kqueue port_create localtime_r posix_fadvise issetugid inet_pton \
			memset_s explicit_bzero'))

	checkTypes(autoconf, Split('pid_t size_t off_t'))

	autoconf.env.Append( LIBSQLITE3 = '', LIBXML2 = '', LIBMYSQL = '', LIBZ = '',
		LIBBZ2 = '', LIBCRYPT = '', LIBMEMCACHE = '', LIBFCGI = '', LIBPCRE = '',
		LIBLDAP = '', LIBLBER = '', LIBLUA = '', LIBDL = '', LIBUUID = '')

	if env['with_fam']:
		if autoconf.CheckLibWithHeader('fam', 'fam.h', 'C'):
			autoconf.env.Append(CPPFLAGS = [ '-DHAVE_FAM_H', '-DHAVE_LIBFAM' ], LIBS = 'fam')
			checkFuncs(autoconf, ['FAMNoExists']);

	if autoconf.CheckLib('crypt'):
		autoconf.env.Append(CPPFLAGS = [ '-DHAVE_LIBCRYPT' ], LIBCRYPT = 'crypt')
		oldlib = env['LIBS']
		env['LIBS'] += ['crypt']
		checkFuncs(autoconf, ['crypt', 'crypt_r']);
		env['LIBS'] = oldlib
	else:
		checkFuncs(autoconf, ['crypt', 'crypt_r']);

	if autoconf.CheckLibWithHeader('uuid', 'uuid/uuid.h', 'C'):
		autoconf.env.Append(CPPFLAGS = [ '-DHAVE_UUID_UUID_H', '-DHAVE_LIBUUID' ], LIBUUID = 'uuid')

	if env['with_openssl']:
		if autoconf.CheckLibWithHeader('ssl', 'openssl/ssl.h', 'C'):
			autoconf.env.Append(CPPFLAGS = [ '-DHAVE_OPENSSL_SSL_H', '-DHAVE_LIBSSL'] , LIBS = [ 'ssl', 'crypto' ])

	if env['with_gzip']:
		if autoconf.CheckLibWithHeader('z', 'zlib.h', 'C'):
			autoconf.env.Append(CPPFLAGS = [ '-DHAVE_ZLIB_H', '-DHAVE_LIBZ' ], LIBZ = 'z')

	if env['with_ldap']:
		if autoconf.CheckLibWithHeader('ldap', 'ldap.h', 'C'):
			autoconf.env.Append(CPPFLAGS = [ '-DHAVE_LDAP_H', '-DHAVE_LIBLDAP' ], LIBLDAP = 'ldap')
		if autoconf.CheckLibWithHeader('lber', 'lber.h', 'C'):
			autoconf.env.Append(CPPFLAGS = [ '-DHAVE_LBER_H', '-DHAVE_LIBLBER' ], LIBLBER = 'lber')

	if env['with_bzip2']:
		if autoconf.CheckLibWithHeader('bz2', 'bzlib.h', 'C'):
			autoconf.env.Append(CPPFLAGS = [ '-DHAVE_BZLIB_H', '-DHAVE_LIBBZ2' ], LIBBZ2 = 'bz2')

	if env['with_memcache']:
		if autoconf.CheckLibWithHeader('memcache', 'memcache.h', 'C'):
			autoconf.env.Append(CPPFLAGS = [ '-DHAVE_MEMCACHE_H', '-DHAVE_LIBMEMCACHE' ], LIBMEMCACHE = 'memcache')

	if env['with_sqlite3']:
		if autoconf.CheckLibWithHeader('sqlite3', 'sqlite3.h', 'C'):
			autoconf.env.Append(CPPFLAGS = [ '-DHAVE_SQLITE3_H', '-DHAVE_LIBSQLITE3' ], LIBSQLITE3 = 'sqlite3')

	ol = env['LIBS']
	if autoconf.CheckLibWithHeader('fcgi', 'fastcgi.h', 'C'):
		autoconf.env.Append(LIBFCGI = 'fcgi')
	env['LIBS'] = ol

	ol = env['LIBS']
	if autoconf.CheckLibWithHeader('dl', 'dlfcn.h', 'C'):
		autoconf.env.Append(LIBDL = 'dl')
	env['LIBS'] = ol

	if autoconf.CheckType('socklen_t', '#include <unistd.h>\n#include <sys/socket.h>\n#include <sys/types.h>'):
		autoconf.env.Append(CPPFLAGS = [ '-DHAVE_SOCKLEN_T' ])

	if autoconf.CheckType('struct sockaddr_storage', '#include <sys/socket.h>\n'):
		autoconf.env.Append(CPPFLAGS = [ '-DHAVE_STRUCT_SOCKADDR_STORAGE' ])

	if autoconf.CheckGmtOffInStructTm():
		autoconf.env.Append(CPPFLAGS = [ '-DHAVE_STRUCT_TM_GMTOFF' ])

	if autoconf.CheckIPv6():
		autoconf.env.Append(CPPFLAGS = [ '-DHAVE_IPV6' ])

	if autoconf.CheckWeakSymbols():
		autoconf.env.Append(CPPFLAGS = [ '-DHAVE_WEAK_SYMBOLS' ])

	env = autoconf.Finish()

def TryLua(env, name):
	result = False
	oldlibs = env['LIBS']
	try:
		print("Searching for lua: " + name + " >= 5.0")
		env.ParseConfig("pkg-config '" + name + " >= 5.0' --cflags --libs")
		env.Append(LIBLUA = env['LIBS'][len(oldlibs):])
		env.Append(CPPFLAGS = [ '-DHAVE_LUA_H' ])
		result = True
	except:
		pass
	env['LIBS'] = oldlibs
	return result

if env['with_lua']:
	found_lua = False
	for lua_name in ['lua5.1', 'lua5.0', 'lua']:
		if TryLua(env, lua_name):
			found_lua = True
			break
	if not found_lua:
		raise RuntimeError("Couldn't find any lua implementation")

if env['with_pcre']:
	pcre_config = checkProgram(env, 'pcre', 'pcre-config')
	env.ParseConfig(pcre_config + ' --cflags --libs')
	env.Append(CPPFLAGS = [ '-DHAVE_PCRE_H', '-DHAVE_LIBPCRE' ], LIBPCRE = 'pcre')

if env['with_xml']:
	xml2_config = checkProgram(env, 'xml', 'xml2-config')
	oldlib = env['LIBS']
	env['LIBS'] = []
	env.ParseConfig(xml2_config + ' --cflags --libs')
	env.Append(CPPFLAGS = [ '-DHAVE_LIBXML_H', '-DHAVE_LIBXML2' ], LIBXML2 = env['LIBS'])
	env['LIBS'] = oldlib

if env['with_mysql']:
	mysql_config = checkProgram(env, 'mysql', 'mysql_config')
	oldlib = env['LIBS']
	env['LIBS'] = []
	env.ParseConfig(mysql_config + ' --cflags --libs')
	env.Append(CPPFLAGS = [ '-DHAVE_MYSQL_H', '-DHAVE_LIBMYSQL' ], LIBMYSQL = 'mysqlclient')
	env['LIBS'] = oldlib

if re.compile("cygwin|mingw").search(env['PLATFORM']):
	env.Append(COMMON_LIB = 'bin')
elif re.compile("darwin|aix").search(env['PLATFORM']):
	env.Append(COMMON_LIB = 'lib')
else:
	env.Append(COMMON_LIB = False)

versions = string.split(version, '.')
version_id = int(versions[0]) << 16 | int(versions[1]) << 8 | int(versions[2])
env.Append(CPPFLAGS = [
		'-DLIGHTTPD_VERSION_ID=' + hex(version_id),
		'-DPACKAGE_NAME=\\"' + package + '\\"',
		'-DPACKAGE_VERSION=\\"' + version + '\\"',
		'-DLIBRARY_DIR="\\"${libdir}\\""',
		'-D_FILE_OFFSET_BITS=64', '-D_LARGEFILE_SOURCE', '-D_LARGE_FILES'
		] )

SConscript('src/SConscript', exports = 'env', variant_dir = 'sconsbuild/build', duplicate = 0)
SConscript('tests/SConscript', exports = 'env', variant_dir = 'sconsbuild/tests')
