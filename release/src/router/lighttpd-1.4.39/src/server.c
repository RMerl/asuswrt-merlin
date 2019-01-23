#include "server.h"
#include "buffer.h"
#include "network.h"
#include "log.h"
#include "keyvalue.h"
#include "response.h"
#include "request.h"
#include "chunk.h"
#include "http_chunk.h"
#include "fdevent.h"
#include "connections.h"
#include "stat_cache.h"
#include "plugin.h"
#include "joblist.h"
#include "network_backends.h"
#include "version.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <assert.h>
#include <locale.h>

#include <stdio.h>

#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#ifdef HAVE_VALGRIND_VALGRIND_H
# include <valgrind/valgrind.h>
#endif

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#ifdef HAVE_PWD_H
# include <grp.h>
# include <pwd.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif

#ifdef HAVE_SYS_PRCTL_H
# include <sys/prctl.h>
#endif

#ifdef USE_OPENSSL
# include <openssl/err.h> 
#endif

#ifndef __sgi
/* IRIX doesn't like the alarm based time() optimization */
/* #define USE_ALARM */
#endif

#ifdef HAVE_GETUID
# ifndef HAVE_ISSETUGID

static int l_issetugid(void) {
	return (geteuid() != getuid() || getegid() != getgid());
}

#  define issetugid l_issetugid
# endif
#endif

static volatile sig_atomic_t srv_shutdown = 0;
static volatile sig_atomic_t graceful_shutdown = 0;
static volatile sig_atomic_t handle_sig_alarm = 1;
static volatile sig_atomic_t handle_sig_hup = 0;
static volatile sig_atomic_t forwarded_sig_hup = 0;

#if defined(HAVE_SIGACTION) && defined(SA_SIGINFO)
static volatile siginfo_t last_sigterm_info;
static volatile siginfo_t last_sighup_info;

static void sigaction_handler(int sig, siginfo_t *si, void *context) {
	static siginfo_t empty_siginfo;
	UNUSED(context);

	if (!si) si = &empty_siginfo;

	switch (sig) {
	case SIGTERM:
		srv_shutdown = 1;
		last_sigterm_info = *si;
		break;
	case SIGINT:
		if (graceful_shutdown) {
			srv_shutdown = 1;
		} else {
			graceful_shutdown = 1;
		}
		last_sigterm_info = *si;

		break;
	case SIGALRM: 
		handle_sig_alarm = 1; 
		break;
	case SIGHUP:
		/** 
		 * we send the SIGHUP to all procs in the process-group
		 * this includes ourself
		 * 
		 * make sure we only send it once and don't create a 
		 * infinite loop
		 */
		if (!forwarded_sig_hup) {
			handle_sig_hup = 1;
			last_sighup_info = *si;
		} else {
			forwarded_sig_hup = 0;
		}
		break;
	case SIGCHLD:
		break;
	case SIGUSR2:
		process_share_link_for_router_sync_use();
		break;
	}
}
#elif defined(HAVE_SIGNAL) || defined(HAVE_SIGACTION)
static void signal_handler(int sig) {
	switch (sig) {
	case SIGTERM: srv_shutdown = 1; break;
	case SIGINT:
	     if (graceful_shutdown) srv_shutdown = 1;
	     else graceful_shutdown = 1;

	     break;
	case SIGALRM: handle_sig_alarm = 1; break;
	case SIGHUP:  handle_sig_hup = 1; break;
	case SIGCHLD:  break;
	}
}
#endif

#ifdef HAVE_FORK
static void daemonize(void) {
#ifdef SIGTTOU
	signal(SIGTTOU, SIG_IGN);
#endif
#ifdef SIGTTIN
	signal(SIGTTIN, SIG_IGN);
#endif
#ifdef SIGTSTP
	signal(SIGTSTP, SIG_IGN);
#endif
	if (0 != fork()) exit(0);

	if (-1 == setsid()) exit(0);

	signal(SIGHUP, SIG_IGN);

	if (0 != fork()) exit(0);

	if (0 != chdir("/")) exit(0);
}
#endif

static server *server_init(void) {
	int i;
	FILE *frandom = NULL;

	server *srv = calloc(1, sizeof(*srv));
	force_assert(srv);
#define CLEAN(x) \
	srv->x = buffer_init();

	CLEAN(response_header);
	CLEAN(parse_full_path);
	CLEAN(ts_debug_str);
	CLEAN(ts_date_str);
	CLEAN(errorlog_buf);
	CLEAN(response_range);
	CLEAN(tmp_buf);
	srv->empty_string = buffer_init_string("");
	CLEAN(cond_check_buf);

	CLEAN(srvconf.errorlog_file);
	CLEAN(srvconf.breakagelog_file);
	CLEAN(srvconf.groupname);
	CLEAN(srvconf.username);
	CLEAN(srvconf.changeroot);
	CLEAN(srvconf.bindhost);
	CLEAN(srvconf.event_handler);
	CLEAN(srvconf.pid_file);

	//- Sungmin add 20111018
	CLEAN(srvconf.arpping_interface);
	CLEAN(srvconf.syslog_file);
	CLEAN(srvconf.product_image);
	CLEAN(srvconf.aicloud_version);
	CLEAN(srvconf.app_installation_url);
	CLEAN(syslog_buf);
	CLEAN(cur_login_info);
	CLEAN(last_login_info);
	srv->last_no_ssl_connection_ts = 0;
	srv->is_streaming_port_opend = 1;
	
	CLEAN(tmp_chunk_len);
#undef CLEAN

#define CLEAN(x) \
	srv->x = array_init();

	CLEAN(config_context);
	CLEAN(config_touched);
	CLEAN(status);
#undef CLEAN

	for (i = 0; i < FILE_CACHE_MAX; i++) {
		srv->mtime_cache[i].mtime = (time_t)-1;
		srv->mtime_cache[i].str = buffer_init();
	}

	if ((NULL != (frandom = fopen("/dev/urandom", "rb")) || NULL != (frandom = fopen("/dev/random", "rb")))
	            && 1 == fread(srv->entropy, sizeof(srv->entropy), 1, frandom)) {
		unsigned int e;
		memcpy(&e, srv->entropy, sizeof(e) < sizeof(srv->entropy) ? sizeof(e) : sizeof(srv->entropy));
		srand(e);
		srv->is_real_entropy = 1;
	} else {
		unsigned int j;
		srand(time(NULL) ^ getpid());
		srv->is_real_entropy = 0;
		for (j = 0; j < sizeof(srv->entropy); j++)
			srv->entropy[j] = rand();
	}
	if (frandom) fclose(frandom);

	srv->cur_ts = time(NULL);
	srv->startup_ts = srv->cur_ts;

	srv->conns = calloc(1, sizeof(*srv->conns));
	force_assert(srv->conns);

	srv->joblist = calloc(1, sizeof(*srv->joblist));
	force_assert(srv->joblist);

	srv->fdwaitqueue = calloc(1, sizeof(*srv->fdwaitqueue));
	force_assert(srv->fdwaitqueue);

	srv->srvconf.modules = array_init();
	srv->srvconf.modules_dir = buffer_init_string(LIBRARY_DIR);
	srv->srvconf.network_backend = buffer_init();
	srv->srvconf.upload_tempdirs = array_init();
	srv->srvconf.reject_expect_100_with_417 = 1;

	/* use syslog */
	srv->errorlog_fd = STDERR_FILENO;
	srv->errorlog_mode = ERRORLOG_FD;

	srv->split_vals = array_init();

	return srv;
}

static void server_free(server *srv) {
	size_t i;

	for (i = 0; i < FILE_CACHE_MAX; i++) {
		buffer_free(srv->mtime_cache[i].str);
	}

#define CLEAN(x) \
	buffer_free(srv->x);

	CLEAN(response_header);
	CLEAN(parse_full_path);
	CLEAN(ts_debug_str);
	CLEAN(ts_date_str);
	CLEAN(errorlog_buf);
	CLEAN(response_range);
	CLEAN(tmp_buf);
	CLEAN(empty_string);
	CLEAN(cond_check_buf);

	CLEAN(srvconf.errorlog_file);
	CLEAN(srvconf.breakagelog_file);
	CLEAN(srvconf.groupname);
	CLEAN(srvconf.username);
	CLEAN(srvconf.changeroot);
	CLEAN(srvconf.bindhost);
	CLEAN(srvconf.event_handler);
	CLEAN(srvconf.pid_file);
	CLEAN(srvconf.modules_dir);
	CLEAN(srvconf.network_backend);

	//- Sungmin add 20111018
	CLEAN(srvconf.arpping_interface);
	CLEAN(srvconf.syslog_file);
	CLEAN(srvconf.product_image);
	CLEAN(srvconf.aicloud_version);
	CLEAN(srvconf.app_installation_url);
	CLEAN(syslog_buf);
	CLEAN(cur_login_info);
	CLEAN(last_login_info);
	
	CLEAN(tmp_chunk_len);
#undef CLEAN

#if 0
	fdevent_unregister(srv->ev, srv->fd);
#endif
	fdevent_free(srv->ev);

	free(srv->conns);

	if (srv->config_storage) {
		for (i = 0; i < srv->config_context->used; i++) {
			specific_config *s = srv->config_storage[i];

			if (!s) continue;

			buffer_free(s->document_root);
			buffer_free(s->server_name);
			buffer_free(s->server_tag);
			buffer_free(s->ssl_pemfile);
			buffer_free(s->ssl_ca_file);
			buffer_free(s->ssl_cipher_list);
			buffer_free(s->ssl_dh_file);
			buffer_free(s->ssl_ec_curve);
			buffer_free(s->error_handler);
			buffer_free(s->errorfile_prefix);
			array_free(s->mimetypes);
			buffer_free(s->ssl_verifyclient_username);
#ifdef USE_OPENSSL
			SSL_CTX_free(s->ssl_ctx);
			EVP_PKEY_free(s->ssl_pemfile_pkey);
			X509_free(s->ssl_pemfile_x509);
			if (NULL != s->ssl_ca_file_cert_names) sk_X509_NAME_pop_free(s->ssl_ca_file_cert_names, X509_NAME_free);
#endif
			free(s);
		}
		free(srv->config_storage);
		srv->config_storage = NULL;
	}

#define CLEAN(x) \
	array_free(srv->x);

	CLEAN(config_context);
	CLEAN(config_touched);
	CLEAN(status);
	CLEAN(srvconf.upload_tempdirs);
#undef CLEAN

	joblist_free(srv, srv->joblist);
	fdwaitqueue_free(srv, srv->fdwaitqueue);

	if (srv->stat_cache) {
		stat_cache_free(srv->stat_cache);
	}

	array_free(srv->srvconf.modules);
	array_free(srv->split_vals);

#ifdef USE_OPENSSL
	if (srv->ssl_is_init) {
		CRYPTO_cleanup_all_ex_data();
		ERR_free_strings();
		ERR_remove_state(0);
		EVP_cleanup();
	}
#endif

	free(srv);
}

static void show_version (void) {
#ifdef USE_OPENSSL
# define TEXT_SSL " (ssl)"
#else
# define TEXT_SSL
#endif
	char *b = PACKAGE_DESC TEXT_SSL \
" - a light and fast webserver\n" \
"Build-Date: " __DATE__ " " __TIME__ "\n";
;
#undef TEXT_SSL
	write_all(STDOUT_FILENO, b, strlen(b));
}

static void show_features (void) {
  const char features[] = ""
#ifdef USE_SELECT
      "\t+ select (generic)\n"
#else
      "\t- select (generic)\n"
#endif
#ifdef USE_POLL
      "\t+ poll (Unix)\n"
#else
      "\t- poll (Unix)\n"
#endif
#ifdef USE_LINUX_SIGIO
      "\t+ rt-signals (Linux 2.4+)\n"
#else
      "\t- rt-signals (Linux 2.4+)\n"
#endif
#ifdef USE_LINUX_EPOLL
      "\t+ epoll (Linux 2.6)\n"
#else
      "\t- epoll (Linux 2.6)\n"
#endif
#ifdef USE_SOLARIS_DEVPOLL
      "\t+ /dev/poll (Solaris)\n"
#else
      "\t- /dev/poll (Solaris)\n"
#endif
#ifdef USE_SOLARIS_PORT
      "\t+ eventports (Solaris)\n"
#else
      "\t- eventports (Solaris)\n"
#endif
#ifdef USE_FREEBSD_KQUEUE
      "\t+ kqueue (FreeBSD)\n"
#else
      "\t- kqueue (FreeBSD)\n"
#endif
#ifdef USE_LIBEV
      "\t+ libev (generic)\n"
#else
      "\t- libev (generic)\n"
#endif
      "\nNetwork handler:\n\n"
#if defined USE_LINUX_SENDFILE
      "\t+ linux-sendfile\n"
#else
      "\t- linux-sendfile\n"
#endif
#if defined USE_FREEBSD_SENDFILE
      "\t+ freebsd-sendfile\n"
#else
      "\t- freebsd-sendfile\n"
#endif
#if defined USE_DARWIN_SENDFILE
      "\t+ darwin-sendfile\n"
#else
      "\t- darwin-sendfile\n"
#endif
#if defined USE_SOLARIS_SENDFILEV
      "\t+ solaris-sendfilev\n"
#else
      "\t- solaris-sendfilev\n"
#endif
#if defined USE_WRITEV
      "\t+ writev\n"
#else
      "\t- writev\n"
#endif
      "\t+ write\n"
#ifdef USE_MMAP
      "\t+ mmap support\n"
#else
      "\t- mmap support\n"
#endif
      "\nFeatures:\n\n"
#ifdef HAVE_IPV6
      "\t+ IPv6 support\n"
#else
      "\t- IPv6 support\n"
#endif
#if defined HAVE_ZLIB_H && defined HAVE_LIBZ
      "\t+ zlib support\n"
#else
      "\t- zlib support\n"
#endif
#if defined HAVE_BZLIB_H && defined HAVE_LIBBZ2
      "\t+ bzip2 support\n"
#else
      "\t- bzip2 support\n"
#endif
#if defined(HAVE_CRYPT) || defined(HAVE_CRYPT_R) || defined(HAVE_LIBCRYPT)
      "\t+ crypt support\n"
#else
      "\t- crypt support\n"
#endif
#ifdef USE_OPENSSL
      "\t+ SSL Support\n"
#else
      "\t- SSL Support\n"
#endif
#ifdef HAVE_LIBPCRE
      "\t+ PCRE support\n"
#else
      "\t- PCRE support\n"
#endif
#ifdef HAVE_MYSQL
      "\t+ mySQL support\n"
#else
      "\t- mySQL support\n"
#endif
#if defined(HAVE_LDAP_H) && defined(HAVE_LBER_H) && defined(HAVE_LIBLDAP) && defined(HAVE_LIBLBER)
      "\t+ LDAP support\n"
#else
      "\t- LDAP support\n"
#endif
#ifdef HAVE_MEMCACHE_H
      "\t+ memcached support\n"
#else
      "\t- memcached support\n"
#endif
#ifdef HAVE_FAM_H
      "\t+ FAM support\n"
#else
      "\t- FAM support\n"
#endif
#ifdef HAVE_LUA_H
      "\t+ LUA support\n"
#else
      "\t- LUA support\n"
#endif
#ifdef HAVE_LIBXML_H
      "\t+ xml support\n"
#else
      "\t- xml support\n"
#endif
#ifdef HAVE_SQLITE3_H
      "\t+ SQLite support\n"
#else
      "\t- SQLite support\n"
#endif
#ifdef HAVE_GDBM_H
      "\t+ GDBM support\n"
#else
      "\t- GDBM support\n"
#endif
      "\n";
  show_version();
  printf("\nEvent Handlers:\n\n%s", features);
}

static void show_help (void) {
#ifdef USE_OPENSSL
# define TEXT_SSL " (ssl)"
#else
# define TEXT_SSL
#endif
	char *b = PACKAGE_DESC TEXT_SSL " ("__DATE__ " " __TIME__ ")" \
" - a light and fast webserver\n" \
"usage:\n" \
" -f <name>  filename of the config-file\n" \
" -m <name>  module directory (default: "LIBRARY_DIR")\n" \
" -p         print the parsed config-file in internal form, and exit\n" \
" -t         test the config-file, and exit\n" \
" -D         don't go to background (default: go to background)\n" \
" -v         show version\n" \
" -V         show compile-time features\n" \
" -h         show this help\n" \
"\n"
;
#undef TEXT_SSL
#undef TEXT_IPV6
	write_all(STDOUT_FILENO, b, strlen(b));
}

int main (int argc, char **argv) {
	
	server *srv = NULL;
	int print_config = 0;
	int test_config = 0;
	int i_am_root;
	int o;
	int num_childs = 0;
	int pid_fd = -1, fd;
	size_t i;
#ifdef HAVE_SIGACTION
	struct sigaction act;
#endif
#ifdef HAVE_GETRLIMIT
	struct rlimit rlim;
#endif

#ifdef USE_ALARM
	struct itimerval interval;

	interval.it_interval.tv_sec = 1;
	interval.it_interval.tv_usec = 0;
	interval.it_value.tv_sec = 1;
	interval.it_value.tv_usec = 0;
#endif
	
	/* for nice %b handling in strfime() */
	setlocale(LC_TIME, "C");

	if (NULL == (srv = server_init())) {
		fprintf(stderr, "did this really happen?\n");
		return -1;
	}

	/* init structs done */

	srv->srvconf.port = 0;
#ifdef HAVE_GETUID
	i_am_root = (getuid() == 0);
#else
	i_am_root = 0;
#endif
	srv->srvconf.dont_daemonize = 0;

#ifdef APP_IPKG
    if(!access("/etc/server.pem",F_OK)){

    }
    else{
    	start_ssl();
    }
#endif

	while(-1 != (o = getopt(argc, argv, "f:m:hvVDpt"))) {
		switch(o) {
		case 'f':
			if (srv->config_storage) {
				log_error_write(srv, __FILE__, __LINE__, "s",
						"Can only read one config file. Use the include command to use multiple config files.");

				server_free(srv);
				return -1;
			}
			if (config_read(srv, optarg)) {
				server_free(srv);
				return -1;
			}			
			break;
		case 'm':
			buffer_copy_string(srv->srvconf.modules_dir, optarg);
			break;
		case 'p': print_config = 1; break;
		case 't': test_config = 1; break;
		case 'D': srv->srvconf.dont_daemonize = 1; break;
		case 'v': show_version(); return 0;
		case 'V': show_features(); return 0;
		case 'h': show_help(); return 0;
		default:
			show_help();
			server_free(srv);
			return -1;
		}
	}
	
	if (!srv->config_storage) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"No configuration available. Try using -f option.");

		server_free(srv);
		return -1;
	}

	if (print_config) {
		data_unset *dc = srv->config_context->data[0];
		if (dc) {
			dc->print(dc, 0);
			fprintf(stdout, "\n");
		} else {
			/* shouldn't happend */
			fprintf(stderr, "global config not found\n");
		}
	}

	if (test_config) {
		printf("Syntax OK\n");
	}

	if (test_config || print_config) {
		server_free(srv);
		return 0;
	}

	/* close stdin and stdout, as they are not needed */
	openDevNull(STDIN_FILENO);
	openDevNull(STDOUT_FILENO);

	if (0 != config_set_defaults(srv)) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"setting default values failed");
		server_free(srv);
		return -1;
	}

	/* UID handling */
#ifdef HAVE_GETUID
	if (!i_am_root && issetugid()) {
		/* we are setuid-root */

		log_error_write(srv, __FILE__, __LINE__, "s",
				"Are you nuts ? Don't apply a SUID bit to this binary");

		server_free(srv);
		return -1;
	}
#endif

	/* check document-root */
	if (buffer_string_is_empty(srv->config_storage[0]->document_root)) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"document-root is not set\n");

		server_free(srv);

		return -1;
	}

	if (plugins_load(srv)) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"loading plugins finally failed");

		plugins_free(srv);
		server_free(srv);

		return -1;
	}

	/* open pid file BEFORE chroot */
	if (!buffer_string_is_empty(srv->srvconf.pid_file)) {
		if (-1 == (pid_fd = open(srv->srvconf.pid_file->ptr, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) {
			struct stat st;
			if (errno != EEXIST) {
				log_error_write(srv, __FILE__, __LINE__, "sbs",
					"opening pid-file failed:", srv->srvconf.pid_file, strerror(errno));
				return -1;
			}

			if (0 != stat(srv->srvconf.pid_file->ptr, &st)) {
				log_error_write(srv, __FILE__, __LINE__, "sbs",
						"stating existing pid-file failed:", srv->srvconf.pid_file, strerror(errno));
			}

			if (!S_ISREG(st.st_mode)) {
				log_error_write(srv, __FILE__, __LINE__, "sb",
						"pid-file exists and isn't regular file:", srv->srvconf.pid_file);
				return -1;
			}

			if (-1 == (pid_fd = open(srv->srvconf.pid_file->ptr, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) {
				log_error_write(srv, __FILE__, __LINE__, "sbs",
						"opening pid-file failed:", srv->srvconf.pid_file, strerror(errno));
				return -1;
			}
		}
	}

	if (srv->event_handler == FDEVENT_HANDLER_SELECT) {
		/* select limits itself
		 *
		 * as it is a hard limit and will lead to a segfault we add some safety
		 * */
		srv->max_fds = FD_SETSIZE - 200;
	} else {
		srv->max_fds = 4096;
	}

	if (i_am_root) {
		struct group *grp = NULL;
		struct passwd *pwd = NULL;
		int use_rlimit = 1;

#ifdef HAVE_VALGRIND_VALGRIND_H
		if (RUNNING_ON_VALGRIND) use_rlimit = 0;
#endif

#ifdef HAVE_GETRLIMIT
		if (0 != getrlimit(RLIMIT_NOFILE, &rlim)) {
			log_error_write(srv, __FILE__, __LINE__,
					"ss", "couldn't get 'max filedescriptors'",
					strerror(errno));
			return -1;
		}

		if (use_rlimit && srv->srvconf.max_fds) {
			/* set rlimits */

			rlim.rlim_cur = srv->srvconf.max_fds;
			rlim.rlim_max = srv->srvconf.max_fds;

			if (0 != setrlimit(RLIMIT_NOFILE, &rlim)) {
				log_error_write(srv, __FILE__, __LINE__,
						"ss", "couldn't set 'max filedescriptors'",
						strerror(errno));
				return -1;
			}
		}

		if (srv->event_handler == FDEVENT_HANDLER_SELECT) {
			srv->max_fds = rlim.rlim_cur < ((int)FD_SETSIZE) - 200 ? rlim.rlim_cur : FD_SETSIZE - 200;
		} else {
			srv->max_fds = rlim.rlim_cur;
		}

		/* set core file rlimit, if enable_cores is set */
		if (use_rlimit && srv->srvconf.enable_cores && getrlimit(RLIMIT_CORE, &rlim) == 0) {
			rlim.rlim_cur = rlim.rlim_max;
			setrlimit(RLIMIT_CORE, &rlim);
		}
#endif
		if (srv->event_handler == FDEVENT_HANDLER_SELECT) {
			/* don't raise the limit above FD_SET_SIZE */
			if (srv->max_fds > ((int)FD_SETSIZE) - 200) {
				log_error_write(srv, __FILE__, __LINE__, "sd",
						"can't raise max filedescriptors above",  FD_SETSIZE - 200,
						"if event-handler is 'select'. Use 'poll' or something else or reduce server.max-fds.");
				return -1;
			}
		}


#ifdef HAVE_PWD_H
		/* set user and group */
		if (!buffer_string_is_empty(srv->srvconf.username)) {
			if (NULL == (pwd = getpwnam(srv->srvconf.username->ptr))) {
				log_error_write(srv, __FILE__, __LINE__, "sb",
						"can't find username", srv->srvconf.username);
				return -1;
			}

			if (pwd->pw_uid == 0) {
				log_error_write(srv, __FILE__, __LINE__, "s",
						"I will not set uid to 0\n");
				return -1;
			}
		}

		if (!buffer_string_is_empty(srv->srvconf.groupname)) {
			if (NULL == (grp = getgrnam(srv->srvconf.groupname->ptr))) {
				log_error_write(srv, __FILE__, __LINE__, "sb",
					"can't find groupname", srv->srvconf.groupname);
				return -1;
			}
			if (grp->gr_gid == 0) {
				log_error_write(srv, __FILE__, __LINE__, "s",
						"I will not set gid to 0\n");
				return -1;
			}
		}
#endif
		/* we need root-perms for port < 1024 */
		if (0 != network_init(srv)) {
			plugins_free(srv);
			server_free(srv);

			return -1;
		}
#ifdef HAVE_PWD_H
		/* 
		 * Change group before chroot, when we have access
		 * to /etc/group
		 * */
		if (NULL != grp) {
			if (-1 == setgid(grp->gr_gid)) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "setgid failed: ", strerror(errno));
				return -1;
			}
			if (-1 == setgroups(0, NULL)) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "setgroups failed: ", strerror(errno));
				return -1;
			}
			if (!buffer_string_is_empty(srv->srvconf.username)) {
				initgroups(srv->srvconf.username->ptr, grp->gr_gid);
			}
		}
#endif
#ifdef HAVE_CHROOT
		if (!buffer_string_is_empty(srv->srvconf.changeroot)) {
			tzset();

			if (-1 == chroot(srv->srvconf.changeroot->ptr)) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "chroot failed: ", strerror(errno));
				return -1;
			}
			if (-1 == chdir("/")) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "chdir failed: ", strerror(errno));
				return -1;
			}
		}
#endif
#ifdef HAVE_PWD_H
		/* drop root privs */
		if (NULL != pwd) {
			if (-1 == setuid(pwd->pw_uid)) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "setuid failed: ", strerror(errno));
				return -1;
			}
		}
#endif
#if defined(HAVE_SYS_PRCTL_H) && defined(PR_SET_DUMPABLE)
		/**
		 * on IRIX 6.5.30 they have prctl() but no DUMPABLE
		 */
		if (srv->srvconf.enable_cores) {
			prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);
		}
#endif
	} else {

#ifdef HAVE_GETRLIMIT
		if (0 != getrlimit(RLIMIT_NOFILE, &rlim)) {
			log_error_write(srv, __FILE__, __LINE__,
					"ss", "couldn't get 'max filedescriptors'",
					strerror(errno));
			return -1;
		}

		/**
		 * we are not root can can't increase the fd-limit, but we can reduce it
		 */
		if (srv->srvconf.max_fds && srv->srvconf.max_fds < rlim.rlim_cur) {
			/* set rlimits */

			rlim.rlim_cur = srv->srvconf.max_fds;

			if (0 != setrlimit(RLIMIT_NOFILE, &rlim)) {
				log_error_write(srv, __FILE__, __LINE__,
						"ss", "couldn't set 'max filedescriptors'",
						strerror(errno));
				return -1;
			}
		}

		if (srv->event_handler == FDEVENT_HANDLER_SELECT) {
			srv->max_fds = rlim.rlim_cur < ((int)FD_SETSIZE) - 200 ? rlim.rlim_cur : FD_SETSIZE - 200;
		} else {
			srv->max_fds = rlim.rlim_cur;
		}

		/* set core file rlimit, if enable_cores is set */
		if (srv->srvconf.enable_cores && getrlimit(RLIMIT_CORE, &rlim) == 0) {
			rlim.rlim_cur = rlim.rlim_max;
			setrlimit(RLIMIT_CORE, &rlim);
		}

#endif
		if (srv->event_handler == FDEVENT_HANDLER_SELECT) {
			/* don't raise the limit above FD_SET_SIZE */
			if (srv->max_fds > ((int)FD_SETSIZE) - 200) {
				log_error_write(srv, __FILE__, __LINE__, "sd",
						"can't raise max filedescriptors above",  FD_SETSIZE - 200,
						"if event-handler is 'select'. Use 'poll' or something else or reduce server.max-fds.");
				return -1;
			}
		}

		if (0 != network_init(srv)) {
			plugins_free(srv);
			server_free(srv);

			return -1;
		}
	}

	/* set max-conns */
	if (srv->srvconf.max_conns > srv->max_fds/2) {
		/* we can't have more connections than max-fds/2 */
		log_error_write(srv, __FILE__, __LINE__, "sdd", "can't have more connections than fds/2: ", srv->srvconf.max_conns, srv->max_fds);
		srv->max_conns = srv->max_fds/2;
	} else if (srv->srvconf.max_conns) {
		/* otherwise respect the wishes of the user */
		srv->max_conns = srv->srvconf.max_conns;
	} else {
		/* or use the default: we really don't want to hit max-fds */
		srv->max_conns = srv->max_fds/3;
	}
	
	if (HANDLER_GO_ON != plugins_call_init(srv)) {
		log_error_write(srv, __FILE__, __LINE__, "s", "Initialization of plugins failed. Going down.");

		plugins_free(srv);
		network_close(srv);
		server_free(srv);

		return -1;
	}

#ifdef HAVE_FORK
	/* network is up, let's deamonize ourself */
	if (srv->srvconf.dont_daemonize == 0) daemonize();
#endif


#ifdef HAVE_SIGACTION
	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &act, NULL);
	sigaction(SIGUSR1, &act, NULL);
# if defined(SA_SIGINFO)
	act.sa_sigaction = sigaction_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;
# else
	act.sa_handler = signal_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
# endif
	sigaction(SIGINT,  &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGHUP,  &act, NULL);
	sigaction(SIGALRM, &act, NULL);
	sigaction(SIGCHLD, &act, NULL);

#elif defined(HAVE_SIGNAL)
	/* ignore the SIGPIPE from sendfile() */
	signal(SIGPIPE, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGALRM, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGHUP,  signal_handler);
	signal(SIGCHLD,  signal_handler);
	signal(SIGINT,  signal_handler);
#endif

#if EMBEDDED_EANBLE
	sigset_t sigs_to_catch;	

	/* set the signal handler */ 
	sigemptyset(&sigs_to_catch); 
	sigaddset(&sigs_to_catch, SIGTERM);
	sigaddset(&sigs_to_catch, SIGUSR2);   
	sigprocmask(SIG_UNBLOCK, &sigs_to_catch, NULL);
	signal(SIGTERM, sigaction_handler);

	//- 20121108 Sungmin add
	signal(SIGUSR2, sigaction_handler);
#endif

#ifdef USE_ALARM
	signal(SIGALRM, signal_handler);

	/* setup periodic timer (1 second) */
	if (setitimer(ITIMER_REAL, &interval, NULL)) {
		log_error_write(srv, __FILE__, __LINE__, "s", "setting timer failed");
		return -1;
	}

	getitimer(ITIMER_REAL, &interval);
#endif


	srv->gid = getgid();
	srv->uid = getuid();

	/* write pid file */
	if (pid_fd != -1) {
		buffer_copy_int(srv->tmp_buf, getpid());
		buffer_append_string_len(srv->tmp_buf, CONST_STR_LEN("\n"));
		if (-1 == write_all(pid_fd, CONST_BUF_LEN(srv->tmp_buf))) {
			log_error_write(srv, __FILE__, __LINE__, "ss", "Couldn't write pid file:", strerror(errno));
			close(pid_fd);
			return -1;
		}
		close(pid_fd);
		pid_fd = -1;
	}

	/* Close stderr ASAP in the child process to make sure that nothing
	 * is being written to that fd which may not be valid anymore. */
	if (-1 == log_error_open(srv)) {
		log_error_write(srv, __FILE__, __LINE__, "s", "Opening errorlog failed. Going down.");

		plugins_free(srv);
		network_close(srv);
		server_free(srv);
		return -1;
	}
	
	//- Sungmin add	
	if (-1 == log_sys_open(srv)) {
		log_error_write(srv, __FILE__, __LINE__, "s", "Opening syslog failed. Going down.");

		plugins_free(srv);
		network_close(srv);
		server_free(srv);
		return -1;
	}

	#if EMBEDDED_EANBLE
	#ifndef APP_IPKG
	buffer_copy_string( srv->last_login_info, nvram_get_webdav_last_login_info() );
	buffer_copy_string( srv->cur_login_info, nvram_get_webdav_last_login_info() );
	#else
	char *last_login_info = nvram_get_webdav_last_login_info();
	fprintf(stderr,"last_login_info=%s\n",last_login_info);
	if(last_login_info == NULL || *last_login_info == '(')
	{
		fprintf(stderr,"111\n");
	}
	else
	{
		buffer_copy_string( srv->last_login_info, last_login_info);
		buffer_copy_string( srv->cur_login_info, last_login_info);
		free(last_login_info);
	}
	#endif
	#else
	buffer_copy_string( srv->last_login_info, "admin>2012/08/08 18:28:28>100.100.100.100" );
	buffer_copy_string( srv->cur_login_info, "admin>2012/08/08 18:28:28>100.100.100.100" );
	#endif
	//////////////////////////////////////////////////////////////////////////////////////////
	
	if (HANDLER_GO_ON != plugins_call_set_defaults(srv)) {
		log_error_write(srv, __FILE__, __LINE__, "s", "Configuration of plugins failed. Going down.");
		
		plugins_free(srv);
		network_close(srv);
		server_free(srv);

		return -1;
	}
	
	/* dump unused config-keys */
	for (i = 0; i < srv->config_context->used; i++) {
		array *config = ((data_config *)srv->config_context->data[i])->value;
		size_t j;

		for (j = 0; config && j < config->used; j++) {
			data_unset *du = config->data[j];

			/* all var.* is known as user defined variable */
			if (strncmp(du->key->ptr, "var.", sizeof("var.") - 1) == 0) {
				continue;
			}

			if (NULL == array_get_element(srv->config_touched, du->key->ptr)) {
				log_error_write(srv, __FILE__, __LINE__, "sbs",
						"WARNING: unknown config-key:",
						du->key,
						"(ignored)");
			}
		}
	}

	if (srv->config_unsupported) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"Configuration contains unsupported keys. Going down.");
	}

	if (srv->config_deprecated) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"Configuration contains deprecated keys. Going down.");
	}

	if (srv->config_unsupported || srv->config_deprecated) {
		plugins_free(srv);
		network_close(srv);
		server_free(srv);

		return -1;
	}


#ifdef HAVE_FORK
	/* start watcher and workers */
	num_childs = srv->srvconf.max_worker;
	if (num_childs > 0) {
		int child = 0;
		while (!child && !srv_shutdown && !graceful_shutdown) {
			if (num_childs > 0) {
				switch (fork()) {
				case -1:
					return -1;
				case 0:
					child = 1;
					break;
				default:
					num_childs--;
					break;
				}
			} else {
				int status;

				if (-1 != wait(&status)) {
					/** 
					 * one of our workers went away 
					 */
					num_childs++;
				} else {
					switch (errno) {
					case EINTR:
						/**
						 * if we receive a SIGHUP we have to close our logs ourself as we don't 
						 * have the mainloop who can help us here
						 */
						if (handle_sig_hup) {
							handle_sig_hup = 0;

							log_error_cycle(srv);

							/**
							 * forward to all procs in the process-group
							 * 
							 * we also send it ourself
							 */
							if (!forwarded_sig_hup) {
								forwarded_sig_hup = 1;
								kill(0, SIGHUP);
							}
						}
						break;
					default:
						break;
					}
				}
			}
		}

		/**
		 * for the parent this is the exit-point 
		 */
		if (!child) {
			/** 
			 * kill all children too 
			 */
			if (graceful_shutdown) {
				kill(0, SIGINT);
			} else if (srv_shutdown) {
				kill(0, SIGTERM);
			}

			log_error_close(srv);
			network_close(srv);
			connections_free(srv);
			plugins_free(srv);
			server_free(srv);
			return 0;
		}
	}
#endif

	if (NULL == (srv->ev = fdevent_init(srv, srv->max_fds + 1, srv->event_handler))) {
		log_error_write(srv, __FILE__, __LINE__,
				"s", "fdevent_init failed");
		return -1;
	}

	/* libev backend overwrites our SIGCHLD handler and calls waitpid on SIGCHLD; we want our own SIGCHLD handling. */
#ifdef HAVE_SIGACTION
	sigaction(SIGCHLD, &act, NULL);
#elif defined(HAVE_SIGNAL)
	signal(SIGCHLD,  signal_handler);
#endif

	/*
	 * kqueue() is called here, select resets its internals,
	 * all server sockets get their handlers
	 *
	 * */
	if (0 != network_register_fdevents(srv)) {
		plugins_free(srv);
		network_close(srv);
		server_free(srv);

		return -1;
	}

	/* might fail if user is using fam (not gamin) and famd isn't running */
	if (NULL == (srv->stat_cache = stat_cache_init())) {
		log_error_write(srv, __FILE__, __LINE__, "s",
			"stat-cache could not be setup, dieing.");
		return -1;
	}

#ifdef HAVE_FAM_H
	/* setup FAM */
	if (srv->srvconf.stat_cache_engine == STAT_CACHE_ENGINE_FAM) {
		if (0 != FAMOpen2(&srv->stat_cache->fam, "lighttpd")) {
			log_error_write(srv, __FILE__, __LINE__, "s",
					 "could not open a fam connection, dieing.");
			return -1;
		}
#ifdef HAVE_FAMNOEXISTS
		FAMNoExists(&srv->stat_cache->fam);
#endif

		fdevent_register(srv->ev, FAMCONNECTION_GETFD(&srv->stat_cache->fam), stat_cache_handle_fdevent, NULL);
		fdevent_event_set(srv->ev, &(srv->stat_cache->fam_fcce_ndx), FAMCONNECTION_GETFD(&srv->stat_cache->fam), FDEVENT_IN);
	}
#endif


	/* get the current number of FDs */
	srv->cur_fds = open("/dev/null", O_RDONLY);
	close(srv->cur_fds);

	for (i = 0; i < srv->srv_sockets.used; i++) {
		server_socket *srv_socket = srv->srv_sockets.ptr[i];
		if (-1 == fdevent_fcntl_set(srv->ev, srv_socket->fd)) {
			log_error_write(srv, __FILE__, __LINE__, "ss", "fcntl failed:", strerror(errno));
			return -1;
		}
	}

	/* main-loop */
	while (!srv_shutdown) {
		int n;
		size_t ndx;
		time_t min_ts;

		if (handle_sig_hup) {
			handler_t r;

			/* reset notification */
			handle_sig_hup = 0;


			/* cycle logfiles */

			switch(r = plugins_call_handle_sighup(srv)) {
			case HANDLER_GO_ON:
				break;
			default:
				log_error_write(srv, __FILE__, __LINE__, "sd", "sighup-handler return with an error", r);
				break;
			}

			if (-1 == log_error_cycle(srv)) {
				log_error_write(srv, __FILE__, __LINE__, "s", "cycling errorlog failed, dying");

				return -1;
			} else {
#ifdef HAVE_SIGACTION
				log_error_write(srv, __FILE__, __LINE__, "sdsd", 
					"logfiles cycled UID =",
					last_sighup_info.si_uid,
					"PID =",
					last_sighup_info.si_pid);
#else
				log_error_write(srv, __FILE__, __LINE__, "s", 
					"logfiles cycled");
#endif
			}
		}

		if (handle_sig_alarm) {
			/* a new second */

#ifdef USE_ALARM
			/* reset notification */
			handle_sig_alarm = 0;
#endif

			/* get current time */
			min_ts = time(NULL);

			if (min_ts != srv->cur_ts) {
#ifdef DEBUG_CONNECTION_STATES
				int cs = 0;
#endif
				connections *conns = srv->conns;
				handler_t r;

				switch(r = plugins_call_handle_trigger(srv)) {
				case HANDLER_GO_ON:
					break;
				case HANDLER_ERROR:
					log_error_write(srv, __FILE__, __LINE__, "s", "one of the triggers failed");
					break;
				default:
					log_error_write(srv, __FILE__, __LINE__, "d", r);
					break;
				}

				/* trigger waitpid */
				srv->cur_ts = min_ts;

				/* cleanup stat-cache */
				stat_cache_trigger_cleanup(srv);
				/**
				 * check all connections for timeouts
				 *
				 */
				for (ndx = 0; ndx < conns->used; ndx++) {
					int changed = 0;
					connection *con;
					int t_diff;

					con = conns->ptr[ndx];

					if (con->state == CON_STATE_READ ||
					    con->state == CON_STATE_READ_POST) {
						if (con->request_count == 1 || con->state == CON_STATE_READ_POST) {
							if (srv->cur_ts - con->read_idle_ts > con->conf.max_read_idle) {
								/* time - out */
								if (con->conf.log_request_handling) {
									log_error_write(srv, __FILE__, __LINE__, "sd",
										"connection closed - read timeout:", con->fd);
								}

								connection_set_state(srv, con, CON_STATE_ERROR);
								changed = 1;
							}
						} else {
							if (srv->cur_ts - con->read_idle_ts > con->keep_alive_idle) {
								/* time - out */
								if (con->conf.log_request_handling) {
									log_error_write(srv, __FILE__, __LINE__, "sd",
										"connection closed - keep-alive timeout:", con->fd);
								}

								connection_set_state(srv, con, CON_STATE_ERROR);
								changed = 1;
							}
						}
					}

					if ((con->state == CON_STATE_WRITE) &&
					    (con->write_request_ts != 0)) {
#if 0
						if (srv->cur_ts - con->write_request_ts > 60) {
							log_error_write(srv, __FILE__, __LINE__, "sdd",
									"connection closed - pre-write-request-timeout:", con->fd, srv->cur_ts - con->write_request_ts);
						}
#endif

						if (srv->cur_ts - con->write_request_ts > con->conf.max_write_idle) {
							/* time - out */
							if (con->conf.log_timeouts) {
								log_error_write(srv, __FILE__, __LINE__, "sbsosds",
									"NOTE: a request for",
									con->request.uri,
									"timed out after writing",
									con->bytes_written,
									"bytes. We waited",
									(int)con->conf.max_write_idle,
									"seconds. If this a problem increase server.max-write-idle");
							}
							connection_set_state(srv, con, CON_STATE_ERROR);
							changed = 1;
						}
					}

					if (con->state == CON_STATE_CLOSE && (srv->cur_ts - con->close_timeout_ts > HTTP_LINGER_TIMEOUT)) {
						changed = 1;
					}

					/* we don't like div by zero */
					if (0 == (t_diff = srv->cur_ts - con->connection_start)) t_diff = 1;

					if (con->traffic_limit_reached &&
					    (con->conf.kbytes_per_second == 0 ||
					     ((con->bytes_written / t_diff) < con->conf.kbytes_per_second * 1024))) {
						/* enable connection again */
						con->traffic_limit_reached = 0;

						changed = 1;
					}

					if (changed) {
						connection_state_machine(srv, con);
					}
					con->bytes_written_cur_second = 0;
					*(con->conf.global_bytes_per_second_cnt_ptr) = 0;

#if DEBUG_CONNECTION_STATES
					if (cs == 0) {
						fprintf(stderr, "connection-state: ");
						cs = 1;
					}

					fprintf(stderr, "c[%d,%d]: %s ",
						con->fd,
						con->fcgi.fd,
						connection_get_state(con->state));
#endif
				}

#ifdef DEBUG_CONNECTION_STATES
				if (cs == 1) fprintf(stderr, "\n");
#endif
			}
		}

		if (srv->sockets_disabled) {
			/* our server sockets are disabled, why ? */

			if ((srv->cur_fds + srv->want_fds < srv->max_fds * 8 / 10) && /* we have enough unused fds */
			    (srv->conns->used <= srv->max_conns * 9 / 10) &&
			    (0 == graceful_shutdown)) {
				for (i = 0; i < srv->srv_sockets.used; i++) {
					server_socket *srv_socket = srv->srv_sockets.ptr[i];
					fdevent_event_set(srv->ev, &(srv_socket->fde_ndx), srv_socket->fd, FDEVENT_IN);
				}

				log_error_write(srv, __FILE__, __LINE__, "s", "[note] sockets enabled again");

				srv->sockets_disabled = 0;
			}
		} else {
			if ((srv->cur_fds + srv->want_fds > srv->max_fds * 9 / 10) || /* out of fds */
			    (srv->conns->used >= srv->max_conns) || /* out of connections */
			    (graceful_shutdown)) { /* graceful_shutdown */

				/* disable server-fds */

				for (i = 0; i < srv->srv_sockets.used; i++) {
					server_socket *srv_socket = srv->srv_sockets.ptr[i];
					fdevent_event_del(srv->ev, &(srv_socket->fde_ndx), srv_socket->fd);

					if (graceful_shutdown) {
						/* we don't want this socket anymore,
						 *
						 * closing it right away will make it possible for
						 * the next lighttpd to take over (graceful restart)
						 *  */

						fdevent_unregister(srv->ev, srv_socket->fd);
						close(srv_socket->fd);
						srv_socket->fd = -1;

						/* network_close() will cleanup after us */

						if (!buffer_string_is_empty(srv->srvconf.pid_file) &&
						    buffer_string_is_empty(srv->srvconf.changeroot)) {
							if (0 != unlink(srv->srvconf.pid_file->ptr)) {
								if (errno != EACCES && errno != EPERM) {
									log_error_write(srv, __FILE__, __LINE__, "sbds",
											"unlink failed for:",
											srv->srvconf.pid_file,
											errno,
											strerror(errno));
								}
							}
						}
					}
				}

				if (graceful_shutdown) {
					log_error_write(srv, __FILE__, __LINE__, "s", "[note] graceful shutdown started");
				} else if (srv->conns->used >= srv->max_conns) {
					log_error_write(srv, __FILE__, __LINE__, "s", "[note] sockets disabled, connection limit reached");
				} else {
					log_error_write(srv, __FILE__, __LINE__, "s", "[note] sockets disabled, out-of-fds");
				}

				srv->sockets_disabled = 1;
			}
		}

		if (graceful_shutdown && srv->conns->used == 0) {
			/* we are in graceful shutdown phase and all connections are closed
			 * we are ready to terminate without harming anyone */
			srv_shutdown = 1;
		}

		/* we still have some fds to share */
		if (srv->want_fds) {
			/* check the fdwaitqueue for waiting fds */
			int free_fds = srv->max_fds - srv->cur_fds - 16;
			connection *con;

			for (; free_fds > 0 && NULL != (con = fdwaitqueue_unshift(srv, srv->fdwaitqueue)); free_fds--) {
				connection_state_machine(srv, con);

				srv->want_fds--;
			}
		}

		if ((n = fdevent_poll(srv->ev, 1000)) > 0) {
			/* n is the number of events */
			int revents;
			int fd_ndx;
#if 0
			if (n > 0) {
				log_error_write(srv, __FILE__, __LINE__, "sd",
						"polls:", n);
			}
#endif
			fd_ndx = -1;
			do {
				fdevent_handler handler;
				void *context;
				handler_t r;

				fd_ndx  = fdevent_event_next_fdndx (srv->ev, fd_ndx);
				if (-1 == fd_ndx) break; /* not all fdevent handlers know how many fds got an event */

				revents = fdevent_event_get_revent (srv->ev, fd_ndx);
				fd      = fdevent_event_get_fd     (srv->ev, fd_ndx);
				handler = fdevent_get_handler(srv->ev, fd);
				context = fdevent_get_context(srv->ev, fd);

				/* connection_handle_fdevent needs a joblist_append */
#if 0
				log_error_write(srv, __FILE__, __LINE__, "sdd",
						"event for", fd, revents);
#endif
				switch (r = (*handler)(srv, context, revents)) {
				case HANDLER_FINISHED:
				case HANDLER_GO_ON:
				case HANDLER_WAIT_FOR_EVENT:
				case HANDLER_WAIT_FOR_FD:
					break;
				case HANDLER_ERROR:
					/* should never happen */
					SEGFAULT();
					break;
				default:
					log_error_write(srv, __FILE__, __LINE__, "d", r);
					break;
				}
			} while (--n > 0);
		} else if (n < 0 && errno != EINTR) {
			log_error_write(srv, __FILE__, __LINE__, "ss",
					"fdevent_poll failed:",
					strerror(errno));
		}

		for (ndx = 0; ndx < srv->joblist->used; ndx++) {
			connection *con = srv->joblist->ptr[ndx];
			handler_t r;

			connection_state_machine(srv, con);

			switch(r = plugins_call_handle_joblist(srv, con)) {
			case HANDLER_FINISHED:
			case HANDLER_GO_ON:
				break;
			default:
				log_error_write(srv, __FILE__, __LINE__, "d", r);
				break;
			}

			con->in_joblist = 0;
		}

		srv->joblist->used = 0;
	}

	if (!buffer_string_is_empty(srv->srvconf.pid_file) &&
	    buffer_string_is_empty(srv->srvconf.changeroot) &&
	    0 == graceful_shutdown) {
		if (0 != unlink(srv->srvconf.pid_file->ptr)) {
			if (errno != EACCES && errno != EPERM) {
				log_error_write(srv, __FILE__, __LINE__, "sbds",
						"unlink failed for:",
						srv->srvconf.pid_file,
						errno,
						strerror(errno));
			}
		}
	}

#ifdef HAVE_SIGACTION
	log_error_write(srv, __FILE__, __LINE__, "sdsd", 
			"server stopped by UID =",
			last_sigterm_info.si_uid,
			"PID =",
			last_sigterm_info.si_pid);
#else
	log_error_write(srv, __FILE__, __LINE__, "s", 
			"server stopped");
#endif

	/* clean-up */
	log_error_close(srv);
	network_close(srv);
	connections_free(srv);
	plugins_free(srv);
	server_free(srv);

	return 0;
}
