include(CheckCSourceCompiles)
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckVariableExists)
include(CheckTypeSize)
include(CheckLibraryExists)
include(CMakeDetermineCCompiler)
include(FindThreads)
include(FindPkgConfig)

include(LighttpdMacros)

add_definitions(-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGE_FILES)

option(WITH_XATTR "with xattr-support for the stat-cache [default: off]")
option(WITH_MYSQL "with mysql-support for the mod_sql_vhost [default: off]")
# option(WITH_POSTGRESQL "with postgress-support for the mod_sql_vhost [default: off]")
option(WITH_OPENSSL "with openssl-support [default: off]")
option(WITH_PCRE "with regex support [default: on]" ON)
option(WITH_WEBDAV_PROPS "with property-support for mod_webdav [default: off]")
option(WITH_WEBDAV_LOCKS "locks in webdav [default: off]")
option(WITH_BZIP "with bzip2-support for mod_compress [default: off]")
option(WITH_ZLIB "with deflate-support for mod_compress [default: on]" ON)
option(WITH_LDAP "with LDAP-support for the mod_auth [default: off]")
option(WITH_LUA "with lua 5.1 for mod_magnet [default: off]")
# option(WITH_VALGRIND "with internal support for valgrind [default: off]")
# option(WITH_KERBEROS5 "use Kerberos5 support with OpenSSL [default: off]")
option(WITH_FAM "fam/gamin for reducing number of stat() calls [default: off]")
option(WITH_GDBM "gdbm storage for mod_trigger_b4_dl [default: off]")
option(WITH_MEMCACHE "memcached storage for mod_trigger_b4_dl [default: off]")
option(WITH_LIBEV "libev support for fdevent handlers [default: off]")
option(WITH_LIBUNWIND "with libunwind to print backtraces in asserts [default: off]")

if(CMAKE_COMPILER_IS_GNUCC)
	option(BUILD_EXTRA_WARNINGS "extra warnings")

	if(BUILD_EXTRA_WARNINGS)
		set(WARN_CFLAGS "-g -g2 -Wall -Wmissing-declarations -Wdeclaration-after-statement -Wcast-align -Wsign-compare -Wnested-externs -Wpointer-arith -D_FORTIFY_SOURCE=2 -fstack-protector --param=ssp-buffer-size=4 -Wformat -Werror=format-security")
		set(WARN_LDFLAGS "-Wl,--as-needed")
		# -Werror -Wbad-function-cast -Wmissing-prototypes
	endif()
endif()

option(BUILD_STATIC "build a static lighttpd with all modules added")

if(BUILD_STATIC)
	set(LIGHTTPD_STATIC 1)
else()
	set(CMAKE_SHARED_LIBRARY_PREFIX "")
endif()

if(WITH_LIBEV)
	find_package(LibEV REQUIRED)
	set(HAVE_LIBEV 1)
endif()

if(WITH_LIBUNWIND)
	pkg_check_modules(LIBUNWIND REQUIRED libunwind)
	set(HAVE_LIBUNWIND 1)
endif()

if(WITH_WEBDAV_PROPS)
	set(WITH_XML 1)
	set(WITH_SQLITE3 1)
	set(WITH_UUID 1)
endif()

check_include_files(sys/devpoll.h HAVE_SYS_DEVPOLL_H)
check_include_files(sys/epoll.h HAVE_SYS_EPOLL_H)
check_include_files(sys/event.h HAVE_SYS_EVENT_H)
check_include_files(sys/mman.h HAVE_SYS_MMAN_H)
check_include_files(sys/poll.h HAVE_SYS_POLL_H)
check_include_files(sys/port.h HAVE_SYS_PORT_H)
check_include_files(sys/prctl.h HAVE_SYS_PRCTL_H)
check_include_files(sys/resource.h HAVE_SYS_RESOURCE_H)
check_include_files(sys/sendfile.h HAVE_SYS_SENDFILE_H)
check_include_files(sys/select.h HAVE_SYS_SELECT_H)
check_include_files(sys/types.h HAVE_SYS_TYPES_H)
check_include_files(sys/uio.h HAVE_SYS_UIO_H)
check_include_files(sys/un.h HAVE_SYS_UN_H)
check_include_files(sys/wait.h HAVE_SYS_WAIT_H)
check_include_files(sys/time.h HAVE_SYS_TIME_H)
check_include_files(unistd.h HAVE_UNISTD_H)
check_include_files(pthread.h HAVE_PTHREAD_H)
check_include_files(getopt.h HAVE_GETOPT_H)
check_include_files(inttypes.h HAVE_INTTYPES_H)
check_include_files(poll.h HAVE_POLL_H)
check_include_files(pwd.h HAVE_PWD_H)
check_include_files(stddef.h HAVE_STDDEF_H)
check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(syslog.h HAVE_SYSLOG_H)

# check for fastcgi lib, for the tests only
check_include_files(fastcgi.h HAVE_FASTCGI_H)
check_include_files(fastcgi/fastcgi.h HAVE_FASTCGI_FASTCGI_H)

# will be needed for auth
check_include_files(crypt.h HAVE_CRYPT_H)
# check if we need libcrypt for crypt_r()
check_library_exists(crypt crypt_r "" HAVE_LIBCRYPT_CRYPT_R)
if(HAVE_LIBCRYPT_CRYPT_R)
	set(HAVE_CRYPT_R 1)
	set(HAVE_LIBCRYPT 1)
else()
	check_library_exists(crypt crypt "" HAVE_LIBCRYPT)
endif()
check_function_exists(crypt_r HAVE_CRYPT_R)
check_function_exists(crypt HAVE_CRYPT)

check_include_files(sys/inotify.h HAVE_SYS_INOTIFY_H)
if(HAVE_SYS_INOTIFY_H)
	check_function_exists(inotify_init HAVE_INOTIFY_INIT)
endif()

set(CMAKE_EXTRA_INCLUDE_FILES sys/socket.h)
check_type_size(socklen_t HAVE_SOCKLEN_T)
set(CMAKE_EXTRA_INCLUDE_FILES)

check_type_size(long SIZEOF_LONG)
check_type_size(off_t SIZEOF_OFF_T)

check_function_exists(chroot HAVE_CHROOT)
check_function_exists(epoll_ctl HAVE_EPOLL_CTL)
check_function_exists(fork HAVE_FORK)
check_function_exists(getrlimit HAVE_GETRLIMIT)
check_function_exists(getuid HAVE_GETUID)
check_function_exists(gmtime_r HAVE_GMTIME_R)
check_function_exists(inet_ntop HAVE_INET_NTOP)
check_function_exists(kqueue HAVE_KQUEUE)
check_function_exists(localtime_r HAVE_LOCALTIME_R)
check_function_exists(lstat HAVE_LSTAT)
check_function_exists(madvise HAVE_MADVISE)
check_function_exists(memcpy HAVE_MEMCPY)
check_function_exists(memset HAVE_MEMSET)
check_function_exists(mmap HAVE_MMAP)
check_function_exists(pathconf HAVE_PATHCONF)
check_function_exists(poll HAVE_POLL)
check_function_exists(port_create HAVE_PORT_CREATE)
check_function_exists(prctl HAVE_PRCTL)
check_function_exists(pread HAVE_PREAD)
check_function_exists(posix_fadvise HAVE_POSIX_FADVISE)
check_function_exists(select HAVE_SELECT)
check_function_exists(sendfile HAVE_SENDFILE)
check_function_exists(send_file HAVE_SEND_FILE)
check_function_exists(sendfile64 HAVE_SENDFILE64)
check_function_exists(sendfilev HAVE_SENDFILEV)
check_function_exists(sigaction HAVE_SIGACTION)
check_function_exists(signal HAVE_SIGNAL)
check_function_exists(sigtimedwait HAVE_SIGTIMEDWAIT)
check_function_exists(strptime HAVE_STRPTIME)
check_function_exists(syslog HAVE_SYSLOG)
check_function_exists(writev HAVE_WRITEV)
check_function_exists(inet_aton HAVE_INET_ATON)
check_function_exists(issetugid HAVE_ISSETUGID)
check_function_exists(inet_pton HAVE_INET_PTON)
check_function_exists(memset_s HAVE_MEMSET_S)
check_function_exists(explicit_bzero HAVE_EXPLICIT_BZERO)
check_c_source_compiles("
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>

	int main() {
		struct sockaddr_in6 s; struct in6_addr t=in6addr_any; int i=AF_INET6; s; t.s6_addr[0] = 0;
		return 0;
	}" HAVE_IPV6)
check_c_source_compiles("
	__attribute__((weak)) void __dummy(void *x) { }
	int main() {
		void *x;
		__dummy(x);
	}
	" HAVE_WEAK_SYMBOLS)

## refactor me
macro(XCONFIG _package _include_DIR _link_DIR _link_FLAGS _cflags)
# reset the variables at the beginning
	set(${_include_DIR})
	set(${_link_DIR})
	set(${_link_FLAGS})
	set(${_cflags})

	find_program(${_package}CONFIG_EXECUTABLE NAMES ${_package} PATHS /usr/local/bin )

	# if pkg-config has been found
	if(${_package}CONFIG_EXECUTABLE)
		set(XCONFIG_EXECUTABLE "${${_package}CONFIG_EXECUTABLE}")
		message(STATUS "found ${_package}: ${XCONFIG_EXECUTABLE}")

		exec_program(${XCONFIG_EXECUTABLE} ARGS --libs OUTPUT_VARIABLE __link_FLAGS)
		string(REPLACE "\n" "" ${_link_FLAGS} ${__link_FLAGS})
		exec_program(${XCONFIG_EXECUTABLE} ARGS --cflags OUTPUT_VARIABLE __cflags)
		string(REPLACE "\n" "" ${_cflags} ${__cflags})
	else()
		message(STATUS "found ${_package}: no")
	endif()
endmacro(XCONFIG _package _include_DIR _link_DIR _link_FLAGS _cflags)

if(WITH_XATTR)
	check_include_files(attr/attributes.h HAVE_ATTR_ATTRIBUTES_H)
	if(HAVE_ATTR_ATTRIBUTES_H)
		check_library_exists(attr attr_get "" HAVE_XATTR)
	endif()
else()
	unset(HAVE_ATTR_ATTRIBUTES_H)
	unset(HAVE_XATTR)
endif()

if(WITH_MYSQL)
	xconfig(mysql_config MYSQL_INCDIR MYSQL_LIBDIR MYSQL_LDFLAGS MYSQL_CFLAGS)

	set(CMAKE_REQUIRED_INCLUDES /usr/include/mysql)
	check_include_files(mysql.h HAVE_MYSQL_H)
	set(CMAKE_REQUIRED_INCLUDES)
	if(HAVE_MYSQL_H)
		check_library_exists(mysqlclient mysql_real_connect "" HAVE_MYSQL)
	endif()
else()
	unset(HAVE_MYSQL_H)
	unset(HAVE_MYSQL)
endif()

if(WITH_OPENSSL)
	check_include_files(openssl/ssl.h HAVE_OPENSSL_SSL_H)
	if(HAVE_OPENSSL_SSL_H)
		check_library_exists(crypto BIO_f_base64 "" HAVE_LIBCRYPTO)
		if(HAVE_LIBCRYPTO)
			set(OPENSSL_NO_KRB5 1)
			check_library_exists(ssl SSL_new "" HAVE_LIBSSL)
		endif()
	endif()
else()
	unset(HAVE_OPENSSL_SSL_H)
	unset(HAVE_LIBCRYPTO)
	unset(HAVE_LIBSSL)
endif()

if(WITH_PCRE)
	## if we have pcre-config, use it
	xconfig(pcre-config PCRE_INCDIR PCRE_LIBDIR PCRE_LDFLAGS PCRE_CFLAGS)
	if(PCRE_LDFLAGS OR PCRE_CFLAGS)
		message(STATUS "found pcre at: LDFLAGS: ${PCRE_LDFLAGS} CFLAGS: ${PCRE_CFLAGS}")

		if(NOT PCRE_CFLAGS STREQUAL "\n")
			## if it is empty we'll get newline returned
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${PCRE_CFLAGS}")
		endif()

		set(HAVE_PCRE_H 1)
		set(HAVE_LIBPCRE 1)
	else()
		if(NOT WIN32)
			check_include_files(pcre.h HAVE_PCRE_H)
			check_library_exists(pcre pcre_exec "" HAVE_LIBPCRE)
			set(PCRE_LDFLAGS -lpcre)
		else()
			find_path(PCRE_INCLUDE_DIR pcre.h
			/usr/local/include
			/usr/include
			)

			set(PCRE_NAMES pcre)
			find_library(PCRE_LIBRARY
			NAMES ${PCRE_NAMES}
			PATHS /usr/lib /usr/local/lib
			)

			if(PCRE_INCLUDE_DIR AND PCRE_LIBRARY)
				set(CMAKE_REQUIRED_INCLUDES ${PCRE_INCLUDE_DIR})
				set(CMAKE_REQUIRED_LIBRARIES ${PCRE_LIBRARY})
				check_include_files(pcre.h HAVE_PCRE_H)
				check_library_exists(pcre pcre_exec "" HAVE_LIBPCRE)
				set(CMAKE_REQUIRED_INCLUDES)
				set(CMAKE_REQUIRED_LIBRARIES)
				include_directories(${PCRE_INCLUDE_DIR})
			endif()
		endif()
	endif()

	if(NOT HAVE_PCRE_H)
		message(FATAL_ERROR "pcre.h couldn't be found")
	endif()
	if(NOT HAVE_LIBPCRE)
		message(FATAL_ERROR "libpcre couldn't be found")
	endif()
else()
	unset(HAVE_PCRE_H)
	unset(HAVE_LIBPCRE)
endif()


if(WITH_XML)
	xconfig(xml2-config XML2_INCDIR XML2_LIBDIR XML2_LDFLAGS XML2_CFLAGS)
	if(XML2_LDFLAGS OR XML2_CFLAGS)
		message(STATUS "found xml2 at: LDFLAGS: ${XML2_LDFLAGS} CFLAGS: ${XML2_CFLAGS}")

		## if it is empty we'll get newline returned
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${XML2_CFLAGS}")

		check_include_files(libxml/tree.h HAVE_LIBXML_H)

		set(CMAKE_REQUIRED_FLAGS ${XML2_LDFLAGS})
		check_library_exists(xml2 xmlParseChunk "" HAVE_LIBXML)
		set(CMAKE_REQUIRED_FLAGS)
	else()
		check_include_files(libxml.h HAVE_LIBXML_H)
		check_library_exists(xml2 xmlParseChunk "" HAVE_LIBXML)
	endif()

	if(NOT HAVE_LIBXML_H)
		message(FATAL_ERROR "libxml/tree.h couldn't be found")
	endif()
	if(NOT HAVE_LIBXML)
		message(FATAL_ERROR "libxml2 couldn't be found")
	endif()
else()
	unset(HAVE_LIBXML_H)
	unset(HAVE_LIBXML)
endif()

if(WITH_SQLITE3)
	check_include_files(sqlite3.h HAVE_SQLITE3_H)
	check_library_exists(sqlite3 sqlite3_reset "" HAVE_SQLITE3)
else()
	unset(HAVE_SQLITE3_H)
	unset(HAVE_SQLITE3)
endif()

if(WITH_UUID)
	check_include_files(uuid/uuid.h HAVE_UUID_UUID_H)
	check_library_exists(uuid uuid_generate "" NEED_LIBUUID)
	if(NOT NEED_LIBUUID)
		check_function_exists(uuid_generate HAVE_LIBUUID)
	else()
		set(HAVE_LIBUUID 1)
	endif()
else()
	unset(HAVE_UUID_UUID_H)
	unset(NEED_LIBUUID)
	unset(HAVE_LIBUUID)
endif()

if(WITH_ZLIB)
	if(NOT WIN32)
		check_include_files(zlib.h HAVE_ZLIB_H)
		check_library_exists(z deflate "" HAVE_LIBZ)
		set(ZLIB_LIBRARY z)
	else()
		find_path(ZLIB_INCLUDE_DIR zlib.h
			/usr/local/include
			/usr/include
		)

		set(ZLIB_NAMES z zlib zdll)
			find_library(ZLIB_LIBRARY
			NAMES ${ZLIB_NAMES}
			PATHS /usr/lib /usr/local/lib
		)

		if(ZLIB_INCLUDE_DIR AND ZLIB_LIBRARY)
			set(CMAKE_REQUIRED_INCLUDES ${ZLIB_INCLUDE_DIR})
			set(CMAKE_REQUIRED_LIBRARIES ${ZLIB_LIBRARY})
			get_filename_component(ZLIB_NAME ${ZLIB_LIBRARY} NAME)
			check_include_files(zlib.h HAVE_ZLIB_H)
			check_library_exists(${ZLIB_NAME} deflate "" HAVE_LIBZ)
			set(CMAKE_REQUIRED_INCLUDES)
			set(CMAKE_REQUIRED_LIBRARIES)
			include_directories(${ZLIB_INCLUDE_DIR})
		endif()
	endif()
else()
	unset(HAVE_ZLIB_H)
	unset(HAVE_LIBZ)
	unset(ZLIB_INCLUDE_DIR)
	unset(ZLIB_LIBRARY)
endif()

if(WITH_BZIP)
	check_include_files(bzlib.h HAVE_BZLIB_H)
	check_library_exists(bz2 BZ2_bzCompress "" HAVE_LIBBZ2)
else()
	unset(HAVE_BZLIB_H)
	unset(HAVE_LIBBZ2)
endif()

if(WITH_LDAP)
	check_include_files(ldap.h HAVE_LDAP_H)
	check_library_exists(ldap ldap_bind "" HAVE_LIBLDAP)
	check_include_files(lber.h HAVE_LBER_H)
	check_library_exists(lber ber_printf "" HAVE_LIBLBER)
	set(LDAP_DEPRECATED 1) # Using deprecated ldap api
else()
	unset(HAVE_LDAP_H)
	unset(HAVE_LIBLDAP)
	unset(HAVE_LBER_H)
	unset(HAVE_LIBLBER)
endif()

if(WITH_LUA)
	pkg_search_module(LUA REQUIRED lua lua5.1 lua-5.1)
	message(STATUS "found lua at: INCDIR: ${LUA_INCLUDE_DIRS} LIBDIR: ${LUA_LIBRARY_DIRS} LDFLAGS: ${LUA_LDFLAGS} CFLAGS: ${LUA_CFLAGS}")
	set(HAVE_LIBLUA 1 "Have liblua")
	set(HAVE_LUA_H  1 "Have liblua header")
else()
	unset(HAVE_LIBLUA)
	unset(HAVE_LUA_H)
endif()

if(WITH_FAM)
	check_include_files(fam.h HAVE_FAM_H)
	check_library_exists(fam FAMOpen2 "" HAVE_LIBFAM)
	if(HAVE_LIBFAM)
		set(CMAKE_REQUIRED_LIBRARIES fam)
		check_function_exists(FAMNoExists HAVE_FAMNOEXISTS)
	endif()
else()
	unset(HAVE_FAM_H)
	unset(HAVE_LIBFAM)
	unset(HAVE_FAMNOEXISTS)
endif()

if(WITH_GDBM)
	check_include_files(gdbm.h HAVE_GDBM_H)
	check_library_exists(gdbm gdbm_open "" HAVE_GDBM)
else()
	unset(HAVE_GDBM_H)
	unset(HAVE_GDBM)
endif()

if(WITH_MEMCACHE)
	check_include_files(memcache.h HAVE_MEMCACHE_H)
	check_library_exists(memcache mc_new "" HAVE_MEMCACHE)
else()
	unset(HAVE_MEMCACHE_H)
	unset(HAVE_MEMCACHE)
endif()

if(NOT BUILD_STATIC)
	check_include_files(dlfcn.h HAVE_DLFCN_H)
else()
	unset(HAVE_DLFCN_H)
endif()

if(HAVE_DLFCN_H)
	check_library_exists(dl dlopen "" HAVE_LIBDL)
else()
	unset(HAVE_LIBDL)
endif()

set(LIGHTTPD_VERSION_ID 10400)
set(PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(PACKAGE_VERSION "${CPACK_PACKAGE_VERSION}")

if(NOT SBINDIR)
	set(SBINDIR "sbin")
endif()

if(NOT LIGHTTPD_MODULES_DIR)
	set(LIGHTTPD_MODULES_DIR "lib${LIB_SUFFIX}/lighttpd")
endif()

if(NOT WIN32)
	set(LIGHTTPD_LIBRARY_DIR "${CMAKE_INSTALL_PREFIX}/${LIGHTTPD_MODULES_DIR}")
else()
	## We use relative path in windows
	set(LIGHTTPD_LIBRARY_DIR "lib")
endif()

## Write out config.h
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)

add_definitions(-DHAVE_CONFIG_H)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

set(COMMON_SRC
	base64.c buffer.c log.c
	keyvalue.c chunk.c
	http_chunk.c stream.c fdevent.c
	stat_cache.c plugin.c joblist.c etag.c array.c
	data_string.c data_count.c data_array.c
	data_integer.c md5.c data_fastcgi.c
	fdevent_select.c fdevent_libev.c
	fdevent_poll.c fdevent_linux_sysepoll.c
	fdevent_solaris_devpoll.c fdevent_solaris_port.c
	fdevent_freebsd_kqueue.c
	data_config.c
	inet_ntop_cache.c crc32.c
	connections-glue.c
	configfile-glue.c
	http-header-glue.c
	splaytree.c network_writev.c
	network_write_mmap.c network_write_no_mmap.c
	network_write.c network_linux_sendfile.c
	network_freebsd_sendfile.c
	network_solaris_sendfilev.c network_openssl.c
	status_counter.c safe_memclear.c network_darwin_sendfile.c
)

if(WIN32)
	message(STATUS "Adding local getopt implementation.")
	set(COMMON_SRC ${COMMON_SRC} xgetopt.c)
endif()

add_executable(lemon lemon.c)

## Build parsers by using lemon...
lemon_parser(configparser.y)
lemon_parser(mod_ssi_exprparser.y)

set(L_INSTALL_TARGETS)

add_executable(lighttpd-angel lighttpd-angel.c)
set(L_INSTALL_TARGETS ${L_INSTALL_TARGETS} lighttpd-angel)
add_target_properties(lighttpd-angel COMPILE_FLAGS "-DSBIN_DIR=\\\\\"${CMAKE_INSTALL_PREFIX}/${SBINDIR}\\\\\"")

add_executable(lighttpd
	server.c
	response.c
	connections.c
	network.c
	configfile.c
	configparser.c
	request.c
	proc_open.c
	${COMMON_SRC}
)
set(L_INSTALL_TARGETS ${L_INSTALL_TARGETS} lighttpd)

add_and_install_library(mod_access mod_access.c)
add_and_install_library(mod_accesslog mod_accesslog.c)
add_and_install_library(mod_alias mod_alias.c)
add_and_install_library(mod_auth "mod_auth.c;http_auth.c")
if(NOT WIN32)
	add_and_install_library(mod_cgi mod_cgi.c)
endif()
add_and_install_library(mod_cml "mod_cml.c;mod_cml_lua.c;mod_cml_funcs.c")
add_and_install_library(mod_compress mod_compress.c)
add_and_install_library(mod_dirlisting mod_dirlisting.c)
add_and_install_library(mod_evasive mod_evasive.c)
add_and_install_library(mod_evhost mod_evhost.c)
add_and_install_library(mod_expire mod_expire.c)
add_and_install_library(mod_extforward mod_extforward.c)
add_and_install_library(mod_fastcgi mod_fastcgi.c)
add_and_install_library(mod_flv_streaming mod_flv_streaming.c)
add_and_install_library(mod_indexfile mod_indexfile.c)
add_and_install_library(mod_magnet "mod_magnet.c;mod_magnet_cache.c")
add_and_install_library(mod_mysql_vhost mod_mysql_vhost.c)
add_and_install_library(mod_proxy mod_proxy.c)
add_and_install_library(mod_redirect mod_redirect.c)
add_and_install_library(mod_rewrite mod_rewrite.c)
add_and_install_library(mod_rrdtool mod_rrdtool.c)
add_and_install_library(mod_scgi mod_scgi.c)
add_and_install_library(mod_secdownload mod_secdownload.c)
add_and_install_library(mod_setenv mod_setenv.c)
add_and_install_library(mod_simple_vhost mod_simple_vhost.c)
add_and_install_library(mod_ssi "mod_ssi_exprparser.c;mod_ssi_expr.c;mod_ssi.c")
add_and_install_library(mod_staticfile mod_staticfile.c)
add_and_install_library(mod_status mod_status.c)
add_and_install_library(mod_trigger_b4_dl mod_trigger_b4_dl.c)
# add_and_install_library(mod_uploadprogress mod_uploadprogress.c)
add_and_install_library(mod_userdir mod_userdir.c)
add_and_install_library(mod_usertrack mod_usertrack.c)
add_and_install_library(mod_webdav mod_webdav.c)

if(HAVE_PCRE_H)
	target_link_libraries(lighttpd ${PCRE_LDFLAGS})
	add_target_properties(lighttpd COMPILE_FLAGS ${PCRE_CFLAGS})
	target_link_libraries(mod_rewrite ${PCRE_LDFLAGS})
	add_target_properties(mod_rewrite COMPILE_FLAGS ${PCRE_CFLAGS})
	target_link_libraries(mod_dirlisting ${PCRE_LDFLAGS})
	add_target_properties(mod_dirlisting COMPILE_FLAGS ${PCRE_CFLAGS})
	target_link_libraries(mod_redirect ${PCRE_LDFLAGS})
	add_target_properties(mod_redirect COMPILE_FLAGS ${PCRE_CFLAGS})
	target_link_libraries(mod_ssi ${PCRE_LDFLAGS})
	add_target_properties(mod_ssi COMPILE_FLAGS ${PCRE_CFLAGS})
	target_link_libraries(mod_trigger_b4_dl ${PCRE_LDFLAGS})
	add_target_properties(mod_trigger_b4_dl COMPILE_FLAGS ${PCRE_CFLAGS})
endif()

target_link_libraries(mod_magnet ${LUA_LDFLAGS})
add_target_properties(mod_magnet COMPILE_FLAGS ${LUA_CFLAGS})

target_link_libraries(mod_cml ${LUA_LDFLAGS})
add_target_properties(mod_cml COMPILE_FLAGS ${LUA_CFLAGS})

if(HAVE_MYSQL_H AND HAVE_MYSQL)
	target_link_libraries(mod_mysql_vhost mysqlclient)
	include_directories(/usr/include/mysql)
endif()

set(L_MOD_WEBDAV)
if(HAVE_SQLITE3_H)
	set(L_MOD_WEBDAV ${L_MOD_WEBDAV} sqlite3)
endif()
if(HAVE_LIBXML_H)
	target_link_libraries(mod_webdav ${XML2_LDFLAGS})
endif()
if(HAVE_UUID_H)
	if(NEED_LIBUUID)
		set(L_MOD_WEBDAV ${L_MOD_WEBDAV} uuid)
	endif()
endif()

target_link_libraries(mod_webdav ${L_MOD_WEBDAV})

set(L_MOD_AUTH)
if(HAVE_LIBCRYPT)
	set(L_MOD_AUTH ${L_MOD_AUTH} crypt)
endif()

if(HAVE_LDAP_H)
	set(L_MOD_AUTH ${L_MOD_AUTH} ldap lber)
endif()
target_link_libraries(mod_auth ${L_MOD_AUTH})

if(HAVE_ZLIB_H)
	if(HAVE_BZLIB_H)
		target_link_libraries(mod_compress ${ZLIB_LIBRARY} bz2)
	else()
		target_link_libraries(mod_compress ${ZLIB_LIBRARY})
	endif()
endif()

if(HAVE_LIBFAM)
	target_link_libraries(lighttpd fam)
endif()

if(HAVE_GDBM_H)
	target_link_libraries(mod_trigger_b4_dl gdbm)
endif()

if(HAVE_MEMCACHE_H)
	target_link_libraries(mod_trigger_b4_dl memcache)
endif()

if(CMAKE_COMPILER_IS_GNUCC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wall -g -Wshadow -W -pedantic ${WARN_CFLAGS}")
	set(CMAKE_C_FLAGS_RELEASE        "${CMAKE_C_FLAGS_RELEASE}     -O2")
	set(CMAKE_C_FLAGS_DEBUG          "${CMAKE_C_FLAGS_DEBUG}       -O0")
	set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_WITHDEBINFO} -O2")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${WARN_LDFLAGS}")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${WARN_LDFLAGS}")
	add_definitions(-D_GNU_SOURCE)
endif()

add_target_properties(lighttpd LINK_FLAGS "-Wl,-export-dynamic")

set_target_properties(lighttpd PROPERTIES CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})

if(WIN32)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNVALGRIND")
	add_target_properties(lighttpd COMPILE_FLAGS "-DLI_DECLARE_EXPORTS")
	target_link_libraries(lighttpd ws2_32)
	target_link_libraries(mod_proxy ws2_32)
	target_link_libraries(mod_fcgi ws2_32)
	target_link_libraries(mod_scgi ws2_32)
	target_link_libraries(mod_ssi ws2_32)

	if(MINGW)
		target_link_libraries(lighttpd msvcr70)
		add_target_properties(lighttpd LINK_FLAGS "-Wl,-subsystem,console")
	endif()
endif()

if(NOT BUILD_STATIC)
	if(HAVE_LIBDL)
		target_link_libraries(lighttpd dl)
	endif()
endif()

if(HAVE_LIBSSL AND HAVE_LIBCRYPTO)
	target_link_libraries(lighttpd ssl)
	target_link_libraries(lighttpd crypto)
endif()

if(WITH_LIBEV)
	target_link_libraries(lighttpd ${LIBEV_LDFLAGS})
	add_target_properties(lighttpd COMPILE_FLAGS ${LIBEV_CFLAGS})
endif()

if(WITH_LIBUNWIND)
	target_link_libraries(lighttpd ${LIBUNWIND_LDFLAGS})
	add_target_properties(lighttpd COMPILE_FLAGS ${LIBUNWIND_CFLAGS})
endif()

if(NOT WIN32)
install(TARGETS ${L_INSTALL_TARGETS}
	RUNTIME DESTINATION ${SBINDIR}
	LIBRARY DESTINATION ${LIGHTTPD_MODULES_DIR}
	ARCHIVE DESTINATION ${LIGHTTPD_MODULES_DIR}/static)
else()
## HACK to make win32 to install our libraries in desired directory..
install(TARGETS lighttpd
	RUNTIME DESTINATION ${SBINDIR}
	ARCHIVE DESTINATION lib/static)
list(REMOVE_ITEM L_INSTALL_TARGETS lighttpd)
install(TARGETS ${L_INSTALL_TARGETS}
	RUNTIME DESTINATION ${SBINDIR}/lib
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib/static)
endif()
