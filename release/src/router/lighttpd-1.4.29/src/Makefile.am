AM_CFLAGS = $(FAM_CFLAGS)

MAKE_SMBDAV_MODULE = 0

noinst_PROGRAMS=proc_open lemon # simple-fcgi #graphic evalo bench ajp ssl error_test adserver gen-license
sbin_PROGRAMS=lighttpd lighttpd-angel lighttpd-monitor lighttpd-arpping
LEMON=$(top_builddir)/src/lemon$(EXEEXT)

lemon_SOURCES=lemon.c

lighttpd_angel_SOURCES=lighttpd-angel.c

lighttpd_monitor_SOURCES=log.c buffer.c lighttpd-monitor.c nvram_control.c

lighttpd_arpping_SOURCES=log.c buffer.c arpping_thread.c nvram_control.c
lighttpd_arpping_LDFLAGS= -lsmbclient

.PHONY: versionstamp parsers

versionstamp:
	@test -f versionstamp.h || touch versionstamp.h; \
	REVISION=""; \
	if test -d "$(top_srcdir)/.svn" -a -x "`which svnversion`"; then \
		REVISION="$$(LANG= LC_ALL=C svnversion "$(top_srcdir)" 2>/dev/null || echo exported)"; \
		if test "$$REVISION" = "exported"; then \
			REVISION=""; \
		fi; \
	fi; \
	if test -z "$$REVISION" -a -d "$(top_srcdir)/.git" -a -x "`which git`"; then \
		REVISION="$$(cd "$(top_srcdir)"; LANG= LC_ALL=C git describe --always 2>/dev/null || echo)"; \
	fi; \
	if test -n "$$REVISION"; then \
		echo "#define REPO_VERSION \"-devel-$$REVISION\"" > versionstamp.h.tmp; \
	else \
		echo "#define REPO_VERSION \"\"" > versionstamp.h.tmp; \
	fi; \
	if ! diff versionstamp.h.tmp versionstamp.h >/dev/null 2>/dev/null; then \
		mv versionstamp.h.tmp versionstamp.h; \
	else \
		rm versionstamp.h.tmp; \
	fi

if CROSS_COMPILING
configparser.c configparser.h:
mod_ssi_exprparser.c mod_ssi_exprparser.h:

parsers:
else
configparser.h: configparser.c
configparser.c: $(srcdir)/configparser.y $(srcdir)/lempar.c lemon$(EXEEXT)
	rm -f configparser.h
	$(LEMON) -q $(srcdir)/configparser.y $(srcdir)/lempar.c

mod_ssi_exprparser.h: mod_ssi_exprparser.c
mod_ssi_exprparser.c: $(srcdir)/mod_ssi_exprparser.y $(srcdir)/lempar.c lemon$(EXEEXT)
	rm -f mod_ssi_exprparser.h
	$(LEMON) -q $(srcdir)/mod_ssi_exprparser.y $(srcdir)/lempar.c

parsers: configparser.c mod_ssi_exprparser.c
endif

#20120613 Jerry add
copy-prebuild:
	if test -d '../prebuild' ; then \
		cp ../prebuild/mod_smbdav.so.l .libs/mod_smbdav.so; \
		cp ../prebuild/mod_aidisk_access.so.l .libs/mod_aidisk_access.so; \
		cp ../prebuild/mod_sharelink.so.l .libs/mod_sharelink.so; \
		cp ../prebuild/mod_aicloud_auth.so.l .libs/mod_aicloud_auth.so; \
		cp ../prebuild/mod_query_field_json.so.l .libs/mod_query_field_json.so; \
	fi

copy-prebuild-arm:
	if test -d '../prebuild_arm' ; then \
		cp ../prebuild_arm/mod_smbdav.so.l .libs/mod_smbdav.so; \
		cp ../prebuild_arm/mod_aidisk_access.so.l .libs/mod_aidisk_access.so; \
		cp ../prebuild_arm/mod_sharelink.so.l .libs/mod_sharelink.so; \
		cp ../prebuild_arm/mod_aicloud_auth.so.l .libs/mod_aicloud_auth.so; \
		cp ../prebuild_arm/mod_query_field_json.so.l .libs/mod_query_field_json.so; \
	fi

copy-prebuild-arm-qtn:
	if test -d '../prebuild_arm_qtn' ; then \
		cp ../prebuild_arm_qtn/mod_smbdav.so.l .libs/mod_smbdav.so; \
		cp ../prebuild_arm_qtn/mod_aidisk_access.so.l .libs/mod_aidisk_access.so; \
		cp ../prebuild_arm_qtn/mod_sharelink.so.l .libs/mod_sharelink.so; \
		cp ../prebuild_arm_qtn/mod_aicloud_auth.so.l .libs/mod_aicloud_auth.so; \
		cp ../prebuild_arm_qtn/mod_query_field_json.so.l .libs/mod_query_field_json.so; \
	fi
BUILT_SOURCES = parsers versionstamp 
MAINTAINERCLEANFILES = configparser.c configparser.h mod_ssi_exprparser.c mod_ssi_exprparser.h
CLEANFILES = versionstamp.h versionstamp.h.tmp

common_src=buffer.c log.c \
      keyvalue.c chunk.c  \
      http_chunk.c stream.c fdevent.c \
      stat_cache.c plugin.c joblist.c etag.c array.c \
      data_string.c data_count.c data_array.c \
      data_integer.c md5.c data_fastcgi.c \
      fdevent_select.c fdevent_libev.c \
      fdevent_poll.c fdevent_linux_sysepoll.c \
      fdevent_solaris_devpoll.c fdevent_solaris_port.c \
      fdevent_freebsd_kqueue.c \
      data_config.c bitset.c \
      inet_ntop_cache.c crc32.c \
      connections-glue.c \
      configfile-glue.c \
      http-header-glue.c \
      network_write.c network_linux_sendfile.c \
      network_freebsd_sendfile.c network_writev.c \
      network_solaris_sendfilev.c network_openssl.c \
      splaytree.c status_counter.c asus_util.c \
	    mod_smbdav_ntlm.c mod_smbdav_basic.c smb_auth.c nvram_control.c

src = server.c response.c connections.c network.c \
      configfile.c configparser.c request.c proc_open.c

lib_LTLIBRARIES =

if NO_RDYNAMIC
# if the linker doesn't allow referencing symbols of the binary
# we have to put everything into a shared-lib and link it into
# everything
lib_LTLIBRARIES += liblightcomp.la
liblightcomp_la_SOURCES=$(common_src)
liblightcomp_la_CFLAGS=$(AM_CFLAGS) $(LIBEV_CFLAGS)
liblightcomp_la_LDFLAGS = -avoid-version -no-undefined
liblightcomp_la_LIBADD = $(PCRE_LIB) $(SSL_LIB) $(FAM_LIBS) $(LIBEV_LIBS)
common_libadd = liblightcomp.la
else
src += $(common_src)
common_libadd =
endif

lib_LTLIBRARIES += liblightsql.la
liblightsql_la_SOURCES = sql.c
liblightsql_la_CFLAGS=$(SQLITE_CFLAGS)
liblightsql_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
liblightsql_la_LIBADD = $(common_libadd) $(SQLITE_LIBS)
sql_libadd = liblightsql.la

lib_LTLIBRARIES += mod_flv_streaming.la
mod_flv_streaming_la_SOURCES = mod_flv_streaming.c
mod_flv_streaming_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_flv_streaming_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_evasive.la
mod_evasive_la_SOURCES = mod_evasive.c
mod_evasive_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_evasive_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_webdav.la
mod_webdav_la_SOURCES = mod_webdav.c
mod_webdav_la_CFLAGS = $(AM_CFLAGS) $(XML_CFLAGS) $(SQLITE_CFLAGS) $(CURL_LIBS)
mod_webdav_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined -lcurl
mod_webdav_la_LIBADD = $(common_libadd) $(XML_LIBS) $(SQLITE_LIBS) $(UUID_LIBS) $(sql_libadd) $(CURL_CFLAGS)


LIBS += $(LIBSMBCLIENT_LIBS) $(XML_LIBS) $(SHARED_LIBS)
CFLAGS += $(LIBSMBCLIENT_CFLAGS) $(XML_CFLAGS)

lib_LTLIBRARIES += mod_magnet.la
mod_magnet_la_SOURCES = mod_magnet.c mod_magnet_cache.c
mod_magnet_la_CFLAGS = $(AM_CFLAGS) $(LUA_CFLAGS)
mod_magnet_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_magnet_la_LIBADD = $(common_libadd) $(LUA_LIBS) -lm

lib_LTLIBRARIES += mod_cml.la
mod_cml_la_SOURCES = mod_cml.c mod_cml_lua.c mod_cml_funcs.c
mod_cml_la_CFLAGS = $(AM_CFLAGS) $(LUA_CFLAGS)
mod_cml_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_cml_la_LIBADD = $(MEMCACHE_LIB) $(common_libadd) $(LUA_LIBS) -lm

lib_LTLIBRARIES += mod_trigger_b4_dl.la
mod_trigger_b4_dl_la_SOURCES = mod_trigger_b4_dl.c
mod_trigger_b4_dl_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_trigger_b4_dl_la_LIBADD = $(GDBM_LIB) $(MEMCACHE_LIB) $(PCRE_LIB) $(common_libadd)

lib_LTLIBRARIES += mod_mysql_vhost.la
mod_mysql_vhost_la_SOURCES = mod_mysql_vhost.c
mod_mysql_vhost_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_mysql_vhost_la_LIBADD = $(MYSQL_LIBS) $(common_libadd)
mod_mysql_vhost_la_CPPFLAGS = $(MYSQL_INCLUDE)

lib_LTLIBRARIES += mod_cgi.la
mod_cgi_la_SOURCES = mod_cgi.c
mod_cgi_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_cgi_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_scgi.la
mod_scgi_la_SOURCES = mod_scgi.c
mod_scgi_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_scgi_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_staticfile.la
mod_staticfile_la_SOURCES = mod_staticfile.c
mod_staticfile_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_staticfile_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_dirlisting.la
mod_dirlisting_la_SOURCES = mod_dirlisting.c
mod_dirlisting_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_dirlisting_la_LIBADD = $(common_libadd) $(PCRE_LIB)

lib_LTLIBRARIES += mod_indexfile.la
mod_indexfile_la_SOURCES = mod_indexfile.c
mod_indexfile_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_indexfile_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_setenv.la
mod_setenv_la_SOURCES = mod_setenv.c
mod_setenv_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_setenv_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_alias.la
mod_alias_la_SOURCES = mod_alias.c
mod_alias_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_alias_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_userdir.la
mod_userdir_la_SOURCES = mod_userdir.c
mod_userdir_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_userdir_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_rrdtool.la
mod_rrdtool_la_SOURCES = mod_rrdtool.c
mod_rrdtool_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_rrdtool_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_usertrack.la
mod_usertrack_la_SOURCES = mod_usertrack.c
mod_usertrack_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_usertrack_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_proxy.la
mod_proxy_la_SOURCES = mod_proxy.c
mod_proxy_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_proxy_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_ssi.la
mod_ssi_la_SOURCES = mod_ssi_exprparser.c mod_ssi_expr.c mod_ssi.c
mod_ssi_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_ssi_la_LIBADD = $(common_libadd) $(PCRE_LIB)

lib_LTLIBRARIES += mod_secdownload.la
mod_secdownload_la_SOURCES = mod_secure_download.c
mod_secdownload_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_secdownload_la_LIBADD = $(common_libadd)

#lib_LTLIBRARIES += mod_httptls.la
#mod_httptls_la_SOURCES = mod_httptls.c
#mod_httptls_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
#mod_httptls_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_expire.la
mod_expire_la_SOURCES = mod_expire.c
mod_expire_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_expire_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_evhost.la
mod_evhost_la_SOURCES = mod_evhost.c
mod_evhost_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_evhost_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_simple_vhost.la
mod_simple_vhost_la_SOURCES = mod_simple_vhost.c
mod_simple_vhost_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_simple_vhost_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_fastcgi.la
mod_fastcgi_la_SOURCES = mod_fastcgi.c
mod_fastcgi_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_fastcgi_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_extforward.la
mod_extforward_la_SOURCES = mod_extforward.c
mod_extforward_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_extforward_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_access.la
mod_access_la_SOURCES = mod_access.c
mod_access_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_access_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_compress.la
mod_compress_la_SOURCES = mod_compress.c
mod_compress_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_compress_la_LIBADD = $(Z_LIB) $(BZ_LIB) $(common_libadd)

lib_LTLIBRARIES += mod_auth.la
mod_auth_la_SOURCES = mod_auth.c http_auth_digest.c http_auth.c
mod_auth_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_auth_la_LIBADD = $(CRYPT_LIB) $(LDAP_LIB) $(LBER_LIB) $(common_libadd)

lib_LTLIBRARIES += mod_rewrite.la
mod_rewrite_la_SOURCES = mod_rewrite.c
mod_rewrite_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_rewrite_la_LIBADD = $(PCRE_LIB) $(common_libadd)

lib_LTLIBRARIES += mod_redirect.la
mod_redirect_la_SOURCES = mod_redirect.c
mod_redirect_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_redirect_la_LIBADD = $(PCRE_LIB) $(common_libadd)

lib_LTLIBRARIES += mod_status.la
mod_status_la_SOURCES = mod_status.c
mod_status_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_status_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_accesslog.la
mod_accesslog_la_SOURCES = mod_accesslog.c
mod_accesslog_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_accesslog_la_LIBADD = $(common_libadd)

lib_LTLIBRARIES += mod_create_captcha_image.la
mod_create_captcha_image_la_SOURCES = mod_create_captcha_image.c
mod_create_captcha_image_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
mod_create_captcha_image_la_LIBADD = $(common_libadd)

hdr = server.h buffer.h network.h log.h keyvalue.h \
      response.h request.h fastcgi.h chunk.h \
      settings.h http_chunk.h http_auth_digest.h \
      md5.h http_auth.h stream.h \
      fdevent.h connections.h base.h stat_cache.h \
      plugin.h mod_auth.h \
      etag.h joblist.h array.h crc32.h \
      network_backends.h configfile.h bitset.h \
      mod_ssi.h mod_ssi_expr.h inet_ntop_cache.h \
      configparser.h mod_ssi_exprparser.h \
      sys-mmap.h sys-socket.h mod_cml.h mod_cml_funcs.h \
      splaytree.h proc_open.h status_counter.h \
      mod_magnet_cache.h \
      version.h asus_util.h smb_auth.h nvram_control.h

DEFS= @DEFS@ -DHAVE_VERSION_H -DLIBRARY_DIR="\"$(libdir)\"" -DSBIN_DIR="\"$(sbindir)\""

lighttpd_SOURCES = $(src)
lighttpd_LDADD = $(PCRE_LIB) $(DL_LIB) $(SENDFILE_LIB) $(ATTR_LIB) $(common_libadd) $(SSL_LIB) $(FAM_LIBS) $(LIBEV_LIBS)
lighttpd_LDFLAGS = -export-dynamic
lighttpd_CCPFLAGS = $(FAM_CFLAGS) $(LIBEV_CFLAGS)

proc_open_SOURCES = proc_open.c buffer.c
proc_open_CPPFLAGS= -DDEBUG_PROC_OPEN

#gen_license_SOURCES = license.c md5.c buffer.c gen_license.c

#ssl_SOURCES = ssl.c


#adserver_SOURCES = buffer.c iframe.c
#adserver_LDADD = -lfcgi -lmysqlclient

#error_test_SOURCES = error_test.c

#evalo_SOURCES = buffer.c eval.c
#bench_SOURCES = buffer.c bench.c
#ajp_SOURCES = ajp.c

noinst_HEADERS   = $(hdr)
EXTRA_DIST = mod_skeleton.c configparser.y mod_ssi_exprparser.y lempar.c SConscript

