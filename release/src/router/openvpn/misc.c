/*
 *  OpenVPN -- An application to securely tunnel IP networks
 *             over a single TCP/UDP port, with support for SSL/TLS-based
 *             session authentication and key exchange,
 *             packet encryption, packet authentication, and
 *             packet compression.
 *
 *  Copyright (C) 2002-2010 OpenVPN Technologies, Inc. <sales@openvpn.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program (see the file COPYING included with this
 *  distribution); if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "syshead.h"

#include "buffer.h"
#include "misc.h"
#include "base64.h"
#include "tun.h"
#include "error.h"
#include "otime.h"
#include "plugin.h"
#include "options.h"
#include "manage.h"
#include "crypto.h"
#include "route.h"
#include "win32.h"

#include "memdbg.h"

#ifdef CONFIG_FEATURE_IPROUTE
const char *iproute_path = IPROUTE_PATH; /* GLOBAL */
#endif

/* contains an SSEC_x value defined in misc.h */
int script_security = SSEC_BUILT_IN; /* GLOBAL */

/* contains SM_x value defined in misc.h */
int script_method = SM_EXECVE; /* GLOBAL */

/* Redefine the top level directory of the filesystem
   to restrict access to files for security */
void
do_chroot (const char *path)
{
  if (path)
    {
#ifdef HAVE_CHROOT
      const char *top = "/";
      if (chroot (path))
	msg (M_ERR, "chroot to '%s' failed", path);
      if (openvpn_chdir (top))
	msg (M_ERR, "cd to '%s' failed", top);
      msg (M_INFO, "chroot to '%s' and cd to '%s' succeeded", path, top);
#else
      msg (M_FATAL, "Sorry but I can't chroot to '%s' because this operating system doesn't appear to support the chroot() system call", path);
#endif
    }
}

/* Get/Set UID of process */

bool
get_user (const char *username, struct user_state *state)
{
  bool ret = false;
  CLEAR (*state);
  if (username)
    {
#if defined(HAVE_GETPWNAM) && defined(HAVE_SETUID)
      state->pw = getpwnam (username);
      if (!state->pw)
	msg (M_ERR, "failed to find UID for user %s", username);
      state->username = username;
      ret = true;
#else
      msg (M_FATAL, "cannot get UID for user %s -- platform lacks getpwname() or setuid() system calls", username);
#endif
    }
  return ret;
}

void
set_user (const struct user_state *state)
{
#if defined(HAVE_GETPWNAM) && defined(HAVE_SETUID)
  if (state->username && state->pw)
    {
      if (setuid (state->pw->pw_uid))
	msg (M_ERR, "setuid('%s') failed", state->username);
      msg (M_INFO, "UID set to %s", state->username);
    }
#endif
}

/* Get/Set GID of process */

bool
get_group (const char *groupname, struct group_state *state)
{
  bool ret = false;
  CLEAR (*state);
  if (groupname)
    {
#if defined(HAVE_GETGRNAM) && defined(HAVE_SETGID)
      state->gr = getgrnam (groupname);
      if (!state->gr)
	msg (M_ERR, "failed to find GID for group %s", groupname);
      state->groupname = groupname;
      ret = true;
#else
      msg (M_FATAL, "cannot get GID for group %s -- platform lacks getgrnam() or setgid() system calls", groupname);
#endif
    }
  return ret;
}

void
set_group (const struct group_state *state)
{
#if defined(HAVE_GETGRNAM) && defined(HAVE_SETGID)
  if (state->groupname && state->gr)
    {
      if (setgid (state->gr->gr_gid))
	msg (M_ERR, "setgid('%s') failed", state->groupname);
      msg (M_INFO, "GID set to %s", state->groupname);
#ifdef HAVE_SETGROUPS
      {
        gid_t gr_list[1];
	gr_list[0] = state->gr->gr_gid;
	if (setgroups (1, gr_list))
	  msg (M_ERR, "setgroups('%s') failed", state->groupname);
      }
#endif
    }
#endif
}

/* Change process priority */
void
set_nice (int niceval)
{
  if (niceval)
    {
#ifdef HAVE_NICE
      errno = 0;
      if (nice (niceval) < 0 && errno != 0)
	msg (M_WARN | M_ERRNO, "WARNING: nice %d failed: %s", niceval, strerror(errno));
      else
	msg (M_INFO, "nice %d succeeded", niceval);
#else
      msg (M_WARN, "WARNING: nice %d failed (function not implemented)", niceval);
#endif
    }
}

/*
 * Pass tunnel endpoint and MTU parms to a user-supplied script.
 * Used to execute the up/down script/plugins.
 */
void
run_up_down (const char *command,
	     const struct plugin_list *plugins,
	     int plugin_type,
	     const char *arg,
	     int tun_mtu,
	     int link_mtu,
	     const char *ifconfig_local,
	     const char* ifconfig_remote,
	     const char *context,
	     const char *signal_text,
	     const char *script_type,
	     struct env_set *es)
{
  struct gc_arena gc = gc_new ();

  if (signal_text)
    setenv_str (es, "signal", signal_text);
  setenv_str (es, "script_context", context);
  setenv_int (es, "tun_mtu", tun_mtu);
  setenv_int (es, "link_mtu", link_mtu);
  setenv_str (es, "dev", arg);

  if (!ifconfig_local)
    ifconfig_local = "";
  if (!ifconfig_remote)
    ifconfig_remote = "";
  if (!context)
    context = "";

  if (plugin_defined (plugins, plugin_type))
    {
      struct argv argv = argv_new ();
      ASSERT (arg);
      argv_printf (&argv,
		   "%s %d %d %s %s %s",
		   arg,
		   tun_mtu, link_mtu,
		   ifconfig_local, ifconfig_remote,
		   context);

      if (plugin_call (plugins, plugin_type, &argv, NULL, es) != OPENVPN_PLUGIN_FUNC_SUCCESS)
	msg (M_FATAL, "ERROR: up/down plugin call failed");

      argv_reset (&argv);
    }

  if (command)
    {
      struct argv argv = argv_new ();
      ASSERT (arg);
      setenv_str (es, "script_type", script_type);
      argv_printf (&argv,
		  "%sc %s %d %d %s %s %s",
		  command,
		  arg,
		  tun_mtu, link_mtu,
		  ifconfig_local, ifconfig_remote,
		  context);
      argv_msg (M_INFO, &argv);
      openvpn_run_script (&argv, es, S_FATAL, "--up/--down");
      argv_reset (&argv);
    }

  gc_free (&gc);
}

/* Get the file we will later write our process ID to */
void
get_pid_file (const char* filename, struct pid_state *state)
{
  CLEAR (*state);
  if (filename)
    {
      state->fp = fopen (filename, "w");
      if (!state->fp)
	msg (M_ERR, "Open error on pid file %s", filename);
      state->filename = filename;
    }
}

/* Write our PID to a file */
void
write_pid (const struct pid_state *state)
{
  if (state->filename && state->fp)
    {
      unsigned int pid = openvpn_getpid (); 
      fprintf(state->fp, "%u\n", pid);
      if (fclose (state->fp))
	msg (M_ERR, "Close error on pid file %s", state->filename);
    }
}

/* Get current PID */
unsigned int
openvpn_getpid ()
{
#ifdef WIN32
  return (unsigned int) GetCurrentProcessId ();
#else
#ifdef HAVE_GETPID
  return (unsigned int) getpid ();
#else
  return 0;
#endif
#endif
}

/* Disable paging */
void
do_mlockall(bool print_msg)
{
#ifdef HAVE_MLOCKALL
  if (mlockall (MCL_CURRENT | MCL_FUTURE))
    msg (M_WARN | M_ERRNO, "WARNING: mlockall call failed");
  else if (print_msg)
    msg (M_INFO, "mlockall call succeeded");
#else
  msg (M_WARN, "WARNING: mlockall call failed (function not implemented)");
#endif
}

#ifndef HAVE_DAEMON

int
daemon(int nochdir, int noclose)
{
#if defined(HAVE_FORK) && defined(HAVE_SETSID)
  switch (fork())
    {
    case -1:
      return (-1);
    case 0:
      break;
    default:
      openvpn_exit (OPENVPN_EXIT_STATUS_GOOD); /* exit point */
    }

  if (setsid() == -1)
    return (-1);

  if (!nochdir)
    openvpn_chdir ("/");

  if (!noclose)
    set_std_files_to_null (false);
#else
  msg (M_FATAL, "Sorry but I can't become a daemon because this operating system doesn't appear to support either the daemon() or fork() system calls");
#endif
  return (0);
}

#endif

/*
 * Set standard file descriptors to /dev/null
 */
void
set_std_files_to_null (bool stdin_only)
{
#if defined(HAVE_DUP) && defined(HAVE_DUP2)
  int fd;
  if ((fd = open ("/dev/null", O_RDWR, 0)) != -1)
    {
      dup2 (fd, 0);
      if (!stdin_only)
	{
	  dup2 (fd, 1);
	  dup2 (fd, 2);
	}
      if (fd > 2)
	close (fd);
    }
#endif
}

/*
 * Wrapper for chdir library function
 */
int
openvpn_chdir (const char* dir)
{
#ifdef HAVE_CHDIR
  return chdir (dir);
#else
  return -1;
#endif
}

/*
 *  dup inetd/xinetd socket descriptor and save
 */

int inetd_socket_descriptor = SOCKET_UNDEFINED; /* GLOBAL */

void
save_inetd_socket_descriptor (void)
{
  inetd_socket_descriptor = INETD_SOCKET_DESCRIPTOR;
#if defined(HAVE_DUP) && defined(HAVE_DUP2)
  /* use handle passed by inetd/xinetd */
  if ((inetd_socket_descriptor = dup (INETD_SOCKET_DESCRIPTOR)) < 0)
    msg (M_ERR, "INETD_SOCKET_DESCRIPTOR dup(%d) failed", INETD_SOCKET_DESCRIPTOR);
  set_std_files_to_null (true);
#endif
}

/*
 * Warn if a given file is group/others accessible.
 */
void
warn_if_group_others_accessible (const char* filename)
{
#ifndef WIN32
#ifdef HAVE_STAT
#if ENABLE_INLINE_FILES
  if (strcmp (filename, INLINE_FILE_TAG))
#endif
    {
      struct stat st;
      if (stat (filename, &st))
	{
	  msg (M_WARN | M_ERRNO, "WARNING: cannot stat file '%s'", filename);
	}
      else
	{
	  if (st.st_mode & (S_IRWXG|S_IRWXO))
	    msg (M_WARN, "WARNING: file '%s' is group or others accessible", filename);
	}
    }
#endif
#endif
}

/*
 * convert system() return into a success/failure value
 */
bool
system_ok (int stat)
{
#ifdef WIN32
  return stat == 0;
#else
  return stat != -1 && WIFEXITED (stat) && WEXITSTATUS (stat) == 0;
#endif
}

/*
 * did system() call execute the given command?
 */
bool
system_executed (int stat)
{
#ifdef WIN32
  return stat != -1;
#else
  return stat != -1 && WEXITSTATUS (stat) != 127;
#endif
}

/*
 * Print an error message based on the status code returned by system().
 */
const char *
system_error_message (int stat, struct gc_arena *gc)
{
  struct buffer out = alloc_buf_gc (256, gc);
#ifdef WIN32
  if (stat == -1)
    buf_printf (&out, "external program did not execute -- ");
  buf_printf (&out, "returned error code %d", stat);
#else
  if (stat == -1)
    buf_printf (&out, "external program fork failed");
  else if (!WIFEXITED (stat))
    buf_printf (&out, "external program did not exit normally");
  else
    {
      const int cmd_ret = WEXITSTATUS (stat);
      if (!cmd_ret)
	buf_printf (&out, "external program exited normally");
      else if (cmd_ret == 127)
	buf_printf (&out, "could not execute external program");
      else
	buf_printf (&out, "external program exited with error status: %d", cmd_ret);
    }
#endif
  return (const char *)out.data;
}

/*
 * Wrapper around openvpn_execve
 */
bool
openvpn_execve_check (const struct argv *a, const struct env_set *es, const unsigned int flags, const char *error_message)
{
  struct gc_arena gc = gc_new ();
  const int stat = openvpn_execve (a, es, flags);
  int ret = false;

  if (system_ok (stat))
    ret = true;
  else
    {
      if (error_message)
	msg (((flags & S_FATAL) ? M_FATAL : M_WARN), "%s: %s",
	     error_message,
	     system_error_message (stat, &gc));
    }
  gc_free (&gc);
  return ret;
}

bool
openvpn_execve_allowed (const unsigned int flags)
{
  if (flags & S_SCRIPT)
    return script_security >= SSEC_SCRIPTS;
  else
    return script_security >= SSEC_BUILT_IN;
}


#ifndef WIN32
/*
 * Run execve() inside a fork().  Designed to replicate the semantics of system() but
 * in a safer way that doesn't require the invocation of a shell or the risks
 * assocated with formatting and parsing a command line.
 */
int
openvpn_execve (const struct argv *a, const struct env_set *es, const unsigned int flags)
{
  struct gc_arena gc = gc_new ();
  int ret = -1;
  static bool warn_shown = false;

  if (a && a->argv[0])
    {
#if defined(ENABLE_EXECVE)
      if (openvpn_execve_allowed (flags))
	{
	  if (script_method == SM_EXECVE)
	    {
	      const char *cmd = a->argv[0];
	      char *const *argv = a->argv;
	      char *const *envp = (char *const *)make_env_array (es, true, &gc);
	      pid_t pid;

	      pid = fork ();
	      if (pid == (pid_t)0) /* child side */
		{
		  execve (cmd, argv, envp);
		  exit (127);
		}
	      else if (pid < (pid_t)0) /* fork failed */
		;
	      else /* parent side */
		{
		  if (waitpid (pid, &ret, 0) != pid)
		    ret = -1;
		}
	    }
	  else if (script_method == SM_SYSTEM)
	    {
	      ret = openvpn_system (argv_system_str (a), es, flags);
	    }
	  else
	    {
	      ASSERT (0);
	    }
	}
      else if (!warn_shown && (script_security < SSEC_SCRIPTS))
	{
	  msg (M_WARN, SCRIPT_SECURITY_WARNING);
          warn_shown = true;
	}
#else
      msg (M_WARN, "openvpn_execve: execve function not available");
#endif
    }
  else
    {
      msg (M_WARN, "openvpn_execve: called with empty argv");
    }

  gc_free (&gc);
  return ret;
}
#endif

/*
 * Wrapper around the system() call.
 */
int
openvpn_system (const char *command, const struct env_set *es, unsigned int flags)
{
#ifdef HAVE_SYSTEM
  int ret;

  perf_push (PERF_SCRIPT);

  /*
   * add env_set to environment.
   */
  if (flags & S_SCRIPT)
    env_set_add_to_environment (es);


  /* debugging */
  dmsg (D_SCRIPT, "SYSTEM[%u] '%s'", flags, command);
  if (flags & S_SCRIPT)
    env_set_print (D_SCRIPT, es);

  /*
   * execute the command
   */
  ret = system (command);

  /* debugging */
  dmsg (D_SCRIPT, "SYSTEM return=%u", ret);

  /*
   * remove env_set from environment
   */
  if (flags & S_SCRIPT)
    env_set_remove_from_environment (es);

  perf_pop ();
  return ret;

#else
  msg (M_FATAL, "Sorry but I can't execute the shell command '%s' because this operating system doesn't appear to support the system() call", command);
  return -1; /* NOTREACHED */
#endif
}

/*
 * Initialize random number seed.  random() is only used
 * when "weak" random numbers are acceptable.
 * OpenSSL routines are always used when cryptographically
 * strong random numbers are required.
 */

void
init_random_seed(void)
{
#ifdef HAVE_GETTIMEOFDAY
  struct timeval tv;

  if (!gettimeofday (&tv, NULL))
    {
      const unsigned int seed = (unsigned int) tv.tv_sec ^ tv.tv_usec;
      srandom (seed);
    }
#else /* HAVE_GETTIMEOFDAY */
  const time_t current = time (NULL);
  srandom ((unsigned int)current);
#endif /* HAVE_GETTIMEOFDAY */
}

/* thread-safe strerror */

const char *
strerror_ts (int errnum, struct gc_arena *gc)
{
#ifdef HAVE_STRERROR
  struct buffer out = alloc_buf_gc (256, gc);

  buf_printf (&out, "%s", openvpn_strerror (errnum, gc));
  return BSTR (&out);
#else
  return "[error string unavailable]";
#endif
}

/*
 * Set environmental variable (int or string).
 *
 * On Posix, we use putenv for portability,
 * and put up with its painful semantics
 * that require all the support code below.
 */

/* General-purpose environmental variable set functions */

static char *
construct_name_value (const char *name, const char *value, struct gc_arena *gc)
{
  struct buffer out;

  ASSERT (name);
  if (!value)
    value = "";
  out = alloc_buf_gc (strlen (name) + strlen (value) + 2, gc);
  buf_printf (&out, "%s=%s", name, value);
  return BSTR (&out);
}

bool
deconstruct_name_value (const char *str, const char **name, const char **value, struct gc_arena *gc)
{
  char *cp;

  ASSERT (str);
  ASSERT (name && value);

  *name = cp = string_alloc (str, gc);
  *value = NULL;

  while ((*cp))
    {
      if (*cp == '=' && !*value)
	{
	  *cp = 0;
	  *value = cp + 1;
	}
      ++cp;
    }
  return *name && *value;
}

static bool
env_string_equal (const char *s1, const char *s2)
{
  int c1, c2;
  ASSERT (s1);
  ASSERT (s2);

  while (true)
    {
      c1 = *s1++;
      c2 = *s2++;
      if (c1 == '=')
	c1 = 0;
      if (c2 == '=')
	c2 = 0;
      if (!c1 && !c2)
	return true;
      if (c1 != c2)
	break;
    }
  return false;
}

static bool
remove_env_item (const char *str, const bool do_free, struct env_item **list)
{
  struct env_item *current, *prev;

  ASSERT (str);
  ASSERT (list);

  for (current = *list, prev = NULL; current != NULL; current = current->next)
    {
      if (env_string_equal (current->string, str))
	{
	  if (prev)
	    prev->next = current->next;
	  else
	    *list = current->next;
	  if (do_free)
	    {
	      memset (current->string, 0, strlen (current->string));
	      free (current->string);
	      free (current);
	    }
	  return true;
	}
      prev = current;
    }
  return false;
}

static void
add_env_item (char *str, const bool do_alloc, struct env_item **list, struct gc_arena *gc)
{
  struct env_item *item;

  ASSERT (str);
  ASSERT (list);

  ALLOC_OBJ_GC (item, struct env_item, gc);
  item->string = do_alloc ? string_alloc (str, gc): str;
  item->next = *list;
  *list = item;
}

/* struct env_set functions */

static bool
env_set_del_nolock (struct env_set *es, const char *str)
{
  return remove_env_item (str, es->gc == NULL, &es->list);
}

static void
env_set_add_nolock (struct env_set *es, const char *str)
{
  remove_env_item (str, es->gc == NULL, &es->list);  
  add_env_item ((char *)str, true, &es->list, es->gc);
}

struct env_set *
env_set_create (struct gc_arena *gc)
{
  struct env_set *es;
  ALLOC_OBJ_CLEAR_GC (es, struct env_set, gc);
  es->list = NULL;
  es->gc = gc;
  return es;
}

void
env_set_destroy (struct env_set *es)
{
  if (es && es->gc == NULL)
    {
      struct env_item *e = es->list;
      while (e)
	{
	  struct env_item *next = e->next;
	  free (e->string);
	  free (e);
	  e = next;
	}
      free (es);
    }
}

bool
env_set_del (struct env_set *es, const char *str)
{
  bool ret;
  ASSERT (es);
  ASSERT (str);
  ret = env_set_del_nolock (es, str);
  return ret;
}

void
env_set_add (struct env_set *es, const char *str)
{
  ASSERT (es);
  ASSERT (str);
  env_set_add_nolock (es, str);
}

void
env_set_print (int msglevel, const struct env_set *es)
{
  if (check_debug_level (msglevel))
    {
      const struct env_item *e;
      int i;

      if (es)
	{
	  e = es->list;
	  i = 0;

	  while (e)
	    {
	      if (env_safe_to_print (e->string))
		msg (msglevel, "ENV [%d] '%s'", i, e->string);
	      ++i;
	      e = e->next;
	    }
	}
    }
}

void
env_set_inherit (struct env_set *es, const struct env_set *src)
{
  const struct env_item *e;

  ASSERT (es);

  if (src)
    {
      e = src->list;
      while (e)
	{
	  env_set_add_nolock (es, e->string);
	  e = e->next;
	}
    }
}

void
env_set_add_to_environment (const struct env_set *es)
{
  if (es)
    {
      struct gc_arena gc = gc_new ();
      const struct env_item *e;

      e = es->list;

      while (e)
	{
	  const char *name;
	  const char *value;

	  if (deconstruct_name_value (e->string, &name, &value, &gc))
	    setenv_str (NULL, name, value);

	  e = e->next;
	}
      gc_free (&gc);
    }
}

void
env_set_remove_from_environment (const struct env_set *es)
{
  if (es)
    {
      struct gc_arena gc = gc_new ();
      const struct env_item *e;

      e = es->list;

      while (e)
	{
	  const char *name;
	  const char *value;

	  if (deconstruct_name_value (e->string, &name, &value, &gc))
	    setenv_del (NULL, name);

	  e = e->next;
	}
      gc_free (&gc);
    }
}

#ifdef HAVE_PUTENV

/* companion functions to putenv */

static struct env_item *global_env = NULL; /* GLOBAL */

static void
manage_env (char *str)
{
  remove_env_item (str, true, &global_env);
  add_env_item (str, false, &global_env, NULL);
}

#endif

/* add/modify/delete environmental strings */

void
setenv_counter (struct env_set *es, const char *name, counter_type value)
{
  char buf[64];
  openvpn_snprintf (buf, sizeof(buf), counter_format, value);
  setenv_str (es, name, buf);
}

void
setenv_int (struct env_set *es, const char *name, int value)
{
  char buf[64];
  openvpn_snprintf (buf, sizeof(buf), "%d", value);
  setenv_str (es, name, buf);
}

void
setenv_unsigned (struct env_set *es, const char *name, unsigned int value)
{
  char buf[64];
  openvpn_snprintf (buf, sizeof(buf), "%u", value);
  setenv_str (es, name, buf);
}

void
setenv_str (struct env_set *es, const char *name, const char *value)
{
  setenv_str_ex (es, name, value, CC_NAME, 0, 0, CC_PRINT, 0, 0);
}

void
setenv_str_safe (struct env_set *es, const char *name, const char *value)
{
  uint8_t b[64];
  struct buffer buf;
  buf_set_write (&buf, b, sizeof (b));
  if (buf_printf (&buf, "OPENVPN_%s", name))
    setenv_str (es, BSTR(&buf), value);
  else
    msg (M_WARN, "setenv_str_safe: name overflow");
}

void
setenv_del (struct env_set *es, const char *name)
{
  ASSERT (name);
  setenv_str (es, name, NULL);
}

void
setenv_str_ex (struct env_set *es,
	       const char *name,
	       const char *value,
	       const unsigned int name_include,
	       const unsigned int name_exclude,
	       const char name_replace,
	       const unsigned int value_include,
	       const unsigned int value_exclude,
	       const char value_replace)
{
  struct gc_arena gc = gc_new ();
  const char *name_tmp;
  const char *val_tmp = NULL;

  ASSERT (name && strlen (name) > 1);

  name_tmp = string_mod_const (name, name_include, name_exclude, name_replace, &gc);

  if (value)
    val_tmp = string_mod_const (value, value_include, value_exclude, value_replace, &gc);

  if (es)
    {
      if (val_tmp)
	{
	  const char *str = construct_name_value (name_tmp, val_tmp, &gc);
	  env_set_add (es, str);
	  /*msg (M_INFO, "SETENV_ES '%s'", str);*/
	}
      else
	env_set_del (es, name_tmp);
    }
  else
    {
#if defined(WIN32)
      {
	/*msg (M_INFO, "SetEnvironmentVariable '%s' '%s'", name_tmp, val_tmp ? val_tmp : "NULL");*/
	if (!SetEnvironmentVariable (name_tmp, val_tmp))
	  msg (M_WARN | M_ERRNO, "SetEnvironmentVariable failed, name='%s', value='%s'",
	       name_tmp,
	       val_tmp ? val_tmp : "NULL");
      }
#elif defined(HAVE_PUTENV)
      {
	char *str = construct_name_value (name_tmp, val_tmp, NULL);
	int status;

	status = putenv (str);
	/*msg (M_INFO, "PUTENV '%s'", str);*/
	if (!status)
	  manage_env (str);
	if (status)
	  msg (M_WARN | M_ERRNO, "putenv('%s') failed", str);
      }
#endif
    }

  gc_free (&gc);
}

/*
 * Setenv functions that append an integer index to the name
 */
static const char *
setenv_format_indexed_name (const char *name, const int i, struct gc_arena *gc)
{
  struct buffer out = alloc_buf_gc (strlen (name) + 16, gc);
  if (i >= 0)
    buf_printf (&out, "%s_%d", name, i);
  else
    buf_printf (&out, "%s", name);
  return BSTR (&out);
}

void
setenv_int_i (struct env_set *es, const char *name, const int value, const int i)
{
  struct gc_arena gc = gc_new ();
  const char *name_str = setenv_format_indexed_name (name, i, &gc);
  setenv_int (es, name_str, value);
  gc_free (&gc);
}

void
setenv_str_i (struct env_set *es, const char *name, const char *value, const int i)
{
  struct gc_arena gc = gc_new ();
  const char *name_str = setenv_format_indexed_name (name, i, &gc);
  setenv_str (es, name_str, value);
  gc_free (&gc);
}

/*
 * taken from busybox networking/ifupdown.c
 */
unsigned int
count_bits(unsigned int a)
{
  unsigned int result;
  result = (a & 0x55) + ((a >> 1) & 0x55);
  result = (result & 0x33) + ((result >> 2) & 0x33);
  return((result & 0x0F) + ((result >> 4) & 0x0F));
}

int
count_netmask_bits(const char *dotted_quad)
{
  unsigned int result, a, b, c, d;
  /* Found a netmask...  Check if it is dotted quad */
  if (sscanf(dotted_quad, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
    return -1;
  result = count_bits(a);
  result += count_bits(b);
  result += count_bits(c);
  result += count_bits(d);
  return ((int)result);
}

/*
 * Go to sleep for n milliseconds.
 */
void
sleep_milliseconds (unsigned int n)
{
#ifdef WIN32
  Sleep (n);
#else
  struct timeval tv;
  tv.tv_sec = n / 1000;
  tv.tv_usec = (n % 1000) * 1000;
  select (0, NULL, NULL, NULL, &tv);
#endif
}

/*
 * Go to sleep indefinitely.
 */
void
sleep_until_signal (void)
{
#ifdef WIN32
  ASSERT (0);
#else
  select (0, NULL, NULL, NULL, NULL);
#endif
}

/* return true if filename can be opened for read */
bool
test_file (const char *filename)
{
  bool ret = false;
  if (filename)
    {
      FILE *fp = fopen (filename, "r");
      if (fp)
	{
	  fclose (fp);
	  ret = true;
	}
    }

  dmsg (D_TEST_FILE, "TEST FILE '%s' [%d]",
       filename ? filename : "UNDEF",
       ret);

  return ret;
}

#ifdef USE_CRYPTO

/* create a temporary filename in directory */
const char *
create_temp_file (const char *directory, const char *prefix, struct gc_arena *gc)
{
  static unsigned int counter;
  struct buffer fname = alloc_buf_gc (256, gc);
  int fd;
  const char *retfname = NULL;
  unsigned int attempts = 0;

  do
    {
      uint8_t rndbytes[16];
      const char *rndstr;

      ++attempts;
      ++counter;

      prng_bytes (rndbytes, sizeof rndbytes);
      rndstr = format_hex_ex (rndbytes, sizeof rndbytes, 40, 0, NULL, gc);
      buf_printf (&fname, PACKAGE "_%s_%s.tmp", prefix, rndstr);

      retfname = gen_path (directory, BSTR (&fname), gc);
      if (!retfname)
        {
          msg (M_FATAL, "Failed to create temporary filename and path");
          return NULL;
        }

      /* Atomically create the file.  Errors out if the file already
         exists.  */
      fd = open (retfname, O_CREAT | O_EXCL | O_WRONLY, S_IRUSR | S_IWUSR);
      if (fd != -1)
        {
          close (fd);
          return retfname;
        }
      else if (fd == -1 && errno != EEXIST)
        {
          /* Something else went wrong, no need to retry.  */
          struct gc_arena gcerr = gc_new ();
          msg (M_FATAL, "Could not create temporary file '%s': %s",
               retfname, strerror_ts (errno, &gcerr));
          gc_free (&gcerr);
          return NULL;
        }
    }
  while (attempts < 6);

  msg (M_FATAL, "Failed to create temporary file after %i attempts", attempts);
  return NULL;
}

/*
 * Add a random string to first DNS label of hostname to prevent DNS caching.
 * For example, foo.bar.gov would be modified to <random-chars>.foo.bar.gov.
 * Of course, this requires explicit support in the DNS server.
 */
const char *
hostname_randomize(const char *hostname, struct gc_arena *gc)
{
# define n_rnd_bytes 6

  char *hst = string_alloc(hostname, gc);
  char *dot = strchr(hst, '.');

  if (dot)
    {
      uint8_t rnd_bytes[n_rnd_bytes];
      const char *rnd_str;
      struct buffer hname = alloc_buf_gc (strlen(hostname)+sizeof(rnd_bytes)*2+4, gc);

      *dot++ = '\0';
      prng_bytes (rnd_bytes, sizeof (rnd_bytes));
      rnd_str = format_hex_ex (rnd_bytes, sizeof (rnd_bytes), 40, 0, NULL, gc);
      buf_printf(&hname, "%s-0x%s.%s", hst, rnd_str, dot);
      return BSTR(&hname);
    }
  else
    return hostname;
# undef n_rnd_bytes
}

#else

const char *
hostname_randomize(const char *hostname, struct gc_arena *gc)
{
  msg (M_WARN, "WARNING: hostname randomization disabled when crypto support is not compiled");
  return hostname;
}

#endif

/*
 * Put a directory and filename together.
 */
const char *
gen_path (const char *directory, const char *filename, struct gc_arena *gc)
{
  const char *safe_filename = string_mod_const (filename, CC_ALNUM|CC_UNDERBAR|CC_DASH|CC_DOT|CC_AT, 0, '_', gc);

  if (safe_filename
      && strcmp (safe_filename, ".")
      && strcmp (safe_filename, "..")
#ifdef WIN32
      && win_safe_filename (safe_filename)
#endif
      )
    {
      const size_t outsize = strlen(safe_filename) + (directory ? strlen (directory) : 0) + 16;
      struct buffer out = alloc_buf_gc (outsize, gc);
      char dirsep[2];

      dirsep[0] = OS_SPECIFIC_DIRSEP;
      dirsep[1] = '\0';

      if (directory)
	buf_printf (&out, "%s%s", directory, dirsep);
      buf_printf (&out, "%s", safe_filename);

      return BSTR (&out);
    }
  else
    return NULL;
}

/* delete a file, return true if succeeded */
bool
delete_file (const char *filename)
{
#if defined(WIN32)
  return (DeleteFile (filename) != 0);
#elif defined(HAVE_UNLINK)
  return (unlink (filename) == 0);
#else
  return false;
#endif
}

bool
absolute_pathname (const char *pathname)
{
  if (pathname)
    {
      const int c = pathname[0];
#ifdef WIN32
      return c == '\\' || (isalpha(c) && pathname[1] == ':' && pathname[2] == '\\');
#else
      return c == '/';
#endif
    }
  else
    return false;
}

#ifdef HAVE_GETPASS

static FILE *
open_tty (const bool write)
{
  FILE *ret;
  ret = fopen ("/dev/tty", write ? "w" : "r");
  if (!ret)
    ret = write ? stderr : stdin;
  return ret;
}

static void
close_tty (FILE *fp)
{
  if (fp != stderr && fp != stdin)
    fclose (fp);
}

#endif

/*
 * Get input from console
 */
bool
get_console_input (const char *prompt, const bool echo, char *input, const int capacity)
{
  bool ret = false;
  ASSERT (prompt);
  ASSERT (input);
  ASSERT (capacity > 0);
  input[0] = '\0';

#if defined(WIN32)
  return get_console_input_win32 (prompt, echo, input, capacity);
#elif defined(HAVE_GETPASS)
  if (echo)
    {
      FILE *fp;

      fp = open_tty (true);
      fprintf (fp, "%s", prompt);
      fflush (fp);
      close_tty (fp);

      fp = open_tty (false);
      if (fgets (input, capacity, fp) != NULL)
	{
	  chomp (input);
	  ret = true;
	}
      close_tty (fp);
    }
  else
    {
      char *gp = getpass (prompt);
      if (gp)
	{
	  strncpynt (input, gp, capacity);
	  memset (gp, 0, strlen (gp));
	  ret = true;
	}
    }
#else
  msg (M_FATAL, "Sorry, but I can't get console input on this OS");
#endif
  return ret;
}

/*
 * Get and store a username/password
 */

bool
get_user_pass_cr (struct user_pass *up,
		  const char *auth_file,
		  const char *prefix,
		  const unsigned int flags,
		  const char *auth_challenge)
{
  struct gc_arena gc = gc_new ();

  if (!up->defined)
    {
      const bool from_stdin = (!auth_file || !strcmp (auth_file, "stdin"));

      if (flags & GET_USER_PASS_PREVIOUS_CREDS_FAILED)
	msg (M_WARN, "Note: previous '%s' credentials failed", prefix);

#ifdef ENABLE_MANAGEMENT
      /*
       * Get username/password from management interface?
       */
      if (management
	  && ((auth_file && streq (auth_file, "management")) || (from_stdin && (flags & GET_USER_PASS_MANAGEMENT)))
	  && management_query_user_pass_enabled (management))
	{
	  if (flags & GET_USER_PASS_PREVIOUS_CREDS_FAILED)
	    management_auth_failure (management, prefix, "previous auth credentials failed");

	  if (!management_query_user_pass (management, up, prefix, flags))
	    {
	      if ((flags & GET_USER_PASS_NOFATAL) != 0)
		return false;
	      else
		msg (M_FATAL, "ERROR: could not read %s username/password/ok/string from management interface", prefix);
	    }
	}
      else
#endif
      /*
       * Get NEED_OK confirmation from the console
       */
      if (flags & GET_USER_PASS_NEED_OK)
	{
	  struct buffer user_prompt = alloc_buf_gc (128, &gc);

	  buf_printf (&user_prompt, "NEED-OK|%s|%s:", prefix, up->username);
	  
	  if (!get_console_input (BSTR (&user_prompt), true, up->password, USER_PASS_LEN))
	    msg (M_FATAL, "ERROR: could not read %s ok-confirmation from stdin", prefix);
	  
	  if (!strlen (up->password))
	    strcpy (up->password, "ok");
	}
	  
      /*
       * Get username/password from standard input?
       */
      else if (from_stdin)
	{
#ifdef ENABLE_CLIENT_CR
	  if (auth_challenge)
	    {
	      struct auth_challenge_info *ac = get_auth_challenge (auth_challenge, &gc);
	      if (ac)
		{
		  char *response = (char *) gc_malloc (USER_PASS_LEN, false, &gc);
		  struct buffer packed_resp;

		  buf_set_write (&packed_resp, (uint8_t*)up->password, USER_PASS_LEN);
		  msg (M_INFO, "CHALLENGE: %s", ac->challenge_text);
		  if (!get_console_input ("Response:", BOOL_CAST(ac->flags&CR_ECHO), response, USER_PASS_LEN))
		    msg (M_FATAL, "ERROR: could not read challenge response from stdin");
		  strncpynt (up->username, ac->user, USER_PASS_LEN);
		  buf_printf (&packed_resp, "CRV1::%s::%s", ac->state_id, response);
		}
	      else
		{
		  msg (M_FATAL, "ERROR: received malformed challenge request from server");
		}
	    }
	  else
#endif
	    {
	      struct buffer user_prompt = alloc_buf_gc (128, &gc);
	      struct buffer pass_prompt = alloc_buf_gc (128, &gc);

	      buf_printf (&user_prompt, "Enter %s Username:", prefix);
	      buf_printf (&pass_prompt, "Enter %s Password:", prefix);

	      if (!(flags & GET_USER_PASS_PASSWORD_ONLY))
		{
		  if (!get_console_input (BSTR (&user_prompt), true, up->username, USER_PASS_LEN))
		    msg (M_FATAL, "ERROR: could not read %s username from stdin", prefix);
		  if (strlen (up->username) == 0)
		    msg (M_FATAL, "ERROR: %s username is empty", prefix);
		}

	      if (!get_console_input (BSTR (&pass_prompt), false, up->password, USER_PASS_LEN))
		msg (M_FATAL, "ERROR: could not not read %s password from stdin", prefix);
	    }
	}
      else
	{
	  /*
	   * Get username/password from a file.
	   */
	  FILE *fp;
      
#ifndef ENABLE_PASSWORD_SAVE
	  /*
	   * Unless ENABLE_PASSWORD_SAVE is defined, don't allow sensitive passwords
	   * to be read from a file.
	   */
	  if (flags & GET_USER_PASS_SENSITIVE)
	    msg (M_FATAL, "Sorry, '%s' password cannot be read from a file", prefix);
#endif

	  warn_if_group_others_accessible (auth_file);

	  fp = fopen (auth_file, "r");
	  if (!fp)
	    msg (M_ERR, "Error opening '%s' auth file: %s", prefix, auth_file);

	  if (flags & GET_USER_PASS_PASSWORD_ONLY)
	    {
	      if (fgets (up->password, USER_PASS_LEN, fp) == NULL)
		msg (M_FATAL, "Error reading password from %s authfile: %s",
		     prefix,
		     auth_file);
	    }
	  else
	    {
	      if (fgets (up->username, USER_PASS_LEN, fp) == NULL
		  || fgets (up->password, USER_PASS_LEN, fp) == NULL)
		msg (M_FATAL, "Error reading username and password (must be on two consecutive lines) from %s authfile: %s",
		     prefix,
		     auth_file);
	    }
      
	  fclose (fp);
      
	  chomp (up->username);
	  chomp (up->password);
      
	  if (!(flags & GET_USER_PASS_PASSWORD_ONLY) && strlen (up->username) == 0)
	    msg (M_FATAL, "ERROR: username from %s authfile '%s' is empty", prefix, auth_file);
	}

      string_mod (up->username, CC_PRINT, CC_CRLF, 0);
      string_mod (up->password, CC_PRINT, CC_CRLF, 0);

      up->defined = true;
    }

#if 0
  msg (M_INFO, "GET_USER_PASS %s u='%s' p='%s'", prefix, up->username, up->password);
#endif

  gc_free (&gc);

  return true;
}

#ifdef ENABLE_CLIENT_CR

/*
 * Parse a challenge message returned along with AUTH_FAILED.
 * The message is formatted as such:
 *
 *  CRV1:<flags>:<state_id>:<username_base64>:<challenge_text>
 *
 * flags: a series of optional, comma-separated flags:
 *  E : echo the response when the user types it
 *  R : a response is required
 *
 * state_id: an opaque string that should be returned to the server
 *  along with the response.
 *
 * username_base64 : the username formatted as base64
 *
 * challenge_text : the challenge text to be shown to the user
 *
 * Example challenge:
 *
 *   CRV1:R,E:Om01u7Fh4LrGBS7uh0SWmzwabUiGiW6l:Y3Ix:Please enter token PIN
 *
 * After showing the challenge_text and getting a response from the user
 * (if R flag is specified), the client should submit the following
 * auth creds back to the OpenVPN server:
 *
 * Username: [username decoded from username_base64]
 * Password: CRV1::<state_id>::<response_text>
 *
 * Where state_id is taken from the challenge request and response_text
 * is what the user entered in response to the challenge_text.
 * If the R flag is not present, response_text may be the empty
 * string.
 *
 * Example response (suppose the user enters "8675309" for the token PIN):
 *
 *   Username: cr1 ("Y3Ix" base64 decoded)
 *   Password: CRV1::Om01u7Fh4LrGBS7uh0SWmzwabUiGiW6l::8675309
 */
struct auth_challenge_info *
get_auth_challenge (const char *auth_challenge, struct gc_arena *gc)
{
  if (auth_challenge)
    {
      struct auth_challenge_info *ac;
      const int len = strlen (auth_challenge);
      char *work = (char *) gc_malloc (len+1, false, gc);
      char *cp;

      struct buffer b;
      buf_set_read (&b, (const uint8_t *)auth_challenge, len);

      ALLOC_OBJ_CLEAR_GC (ac, struct auth_challenge_info, gc);

      /* parse prefix */
      if (!buf_parse(&b, ':', work, len))
	return NULL;
      if (strcmp(work, "CRV1"))
	return NULL;

      /* parse flags */
      if (!buf_parse(&b, ':', work, len))
	return NULL;
      for (cp = work; *cp != '\0'; ++cp)
	{
	  const char c = *cp;
	  if (c == 'E')
	    ac->flags |= CR_ECHO;
	  else if (c == 'R')
	    ac->flags |= CR_RESPONSE;
	}
      
      /* parse state ID */
      if (!buf_parse(&b, ':', work, len))
	return NULL;
      ac->state_id = string_alloc(work, gc);

      /* parse user name */
      if (!buf_parse(&b, ':', work, len))
	return NULL;
      ac->user = (char *) gc_malloc (strlen(work)+1, true, gc);
      base64_decode(work, (void*)ac->user);

      /* parse challenge text */
      ac->challenge_text = string_alloc(BSTR(&b), gc);

      return ac;
    }
  else
    return NULL;
}

#endif

#if AUTO_USERID

static const char *
get_platform_prefix (void)
{
#if defined(TARGET_LINUX)
  return "L";
#elif defined(TARGET_SOLARIS)
  return "S";
#elif defined(TARGET_OPENBSD)
  return "O";
#elif defined(TARGET_DARWIN)
  return "M";
#elif defined(TARGET_NETBSD)
  return "N";
#elif defined(TARGET_FREEBSD)
  return "F";
#elif defined(WIN32)
  return "W";
#else
  return "X";
#endif
}

void
get_user_pass_auto_userid (struct user_pass *up, const char *tag)
{
  struct gc_arena gc = gc_new ();
  MD5_CTX ctx;
  struct buffer buf;
  uint8_t macaddr[6];
  static uint8_t digest [MD5_DIGEST_LENGTH];
  static const uint8_t hashprefix[] = "AUTO_USERID_DIGEST";

  CLEAR (*up);
  buf_set_write (&buf, (uint8_t*)up->username, USER_PASS_LEN);
  buf_printf (&buf, "%s", get_platform_prefix ());
  if (get_default_gateway_mac_addr (macaddr))
    {
      dmsg (D_AUTO_USERID, "GUPAU: macaddr=%s", format_hex_ex (macaddr, sizeof (macaddr), 0, 1, ":", &gc));
      MD5_Init (&ctx);
      MD5_Update (&ctx, hashprefix, sizeof (hashprefix) - 1);
      MD5_Update (&ctx, macaddr, sizeof (macaddr));
      MD5_Final (digest, &ctx);
      buf_printf (&buf, "%s", format_hex_ex (digest, sizeof (digest), 0, 256, " ", &gc));
    }
  else
    {
      buf_printf (&buf, "UNKNOWN");
    }
  if (tag && strcmp (tag, "stdin"))
    buf_printf (&buf, "-%s", tag);
  up->defined = true;
  gc_free (&gc);

  dmsg (D_AUTO_USERID, "GUPAU: AUTO_USERID: '%s'", up->username);
}

#endif

void
purge_user_pass (struct user_pass *up, const bool force)
{
  const bool nocache = up->nocache;
  static bool warn_shown = false;
  if (nocache || force)
    {
      CLEAR (*up);
      up->nocache = nocache;
    }
  else if (!warn_shown)
    {
      msg (M_WARN, "WARNING: this configuration may cache passwords in memory -- use the auth-nocache option to prevent this");
      warn_shown = true;
    }
}

/*
 * Process string received by untrusted peer before
 * printing to console or log file.
 *
 * Assumes that string has been null terminated.
 */
const char *
safe_print (const char *str, struct gc_arena *gc)
{
  return string_mod_const (str, CC_PRINT, CC_CRLF, '.', gc);
}

static bool
is_password_env_var (const char *str)
{
  return (strncmp (str, "password", 8) == 0);
}

bool
env_allowed (const char *str)
{
  return (script_security >= SSEC_PW_ENV || !is_password_env_var (str));
}

bool
env_safe_to_print (const char *str)
{
#ifndef UNSAFE_DEBUG
  if (is_password_env_var (str))
    return false;
#endif
  return true;
}

/* Make arrays of strings */

const char **
make_env_array (const struct env_set *es,
		const bool check_allowed,
		struct gc_arena *gc)
{
  char **ret = NULL;
  struct env_item *e = NULL;
  int i = 0, n = 0;

  /* figure length of es */
  if (es)
    {
      for (e = es->list; e != NULL; e = e->next)
	++n;
    }

  /* alloc return array */
  ALLOC_ARRAY_CLEAR_GC (ret, char *, n+1, gc);

  /* fill return array */
  if (es)
    {
      i = 0;
      for (e = es->list; e != NULL; e = e->next)
	{
	  if (!check_allowed || env_allowed (e->string))
	    {
	      ASSERT (i < n);
	      ret[i++] = e->string;
	    }
	}
    }

  ret[i] = NULL;
  return (const char **)ret;
}

const char **
make_arg_array (const char *first, const char *parms, struct gc_arena *gc)
{
  char **ret = NULL;
  int base = 0;
  const int max_parms = MAX_PARMS + 2;
  int n = 0;

  /* alloc return array */
  ALLOC_ARRAY_CLEAR_GC (ret, char *, max_parms, gc);

  /* process first parameter, if provided */
  if (first)
    {
      ret[base++] = string_alloc (first, gc);
    }

  if (parms)
    {
      n = parse_line (parms, &ret[base], max_parms - base - 1, "make_arg_array", 0, M_WARN, gc);
      ASSERT (n >= 0 && n + base + 1 <= max_parms);
    }
  ret[base + n] = NULL;

  return (const char **)ret;
}

#if ENABLE_INLINE_FILES
static const char **
make_inline_array (const char *str, struct gc_arena *gc)
{
  char line[OPTION_LINE_SIZE];
  struct buffer buf;
  int len = 0;
  char **ret = NULL;
  int i = 0;

  buf_set_read (&buf, (const uint8_t *) str, strlen (str));
  while (buf_parse (&buf, '\n', line, sizeof (line)))
    ++len;

  /* alloc return array */
  ALLOC_ARRAY_CLEAR_GC (ret, char *, len + 1, gc);

  buf_set_read (&buf, (const uint8_t *) str, strlen(str));
  while (buf_parse (&buf, '\n', line, sizeof (line)))
    {
      chomp (line);
      ASSERT (i < len);
      ret[i] = string_alloc (skip_leading_whitespace (line), gc);
      ++i;
    }  
  ASSERT (i <= len);
  ret[i] = NULL;
  return (const char **)ret;
}
#endif

static const char **
make_arg_copy (char **p, struct gc_arena *gc)
{
  char **ret = NULL;
  const int len = string_array_len ((const char **)p);
  const int max_parms = len + 1;
  int i;

  /* alloc return array */
  ALLOC_ARRAY_CLEAR_GC (ret, char *, max_parms, gc);

  for (i = 0; i < len; ++i)
    ret[i] = p[i];

  return (const char **)ret;
}

const char **
make_extended_arg_array (char **p, struct gc_arena *gc)
{
  const int argc = string_array_len ((const char **)p);
#if ENABLE_INLINE_FILES
  if (!strcmp (p[0], INLINE_FILE_TAG) && argc == 2)
    return make_inline_array (p[1], gc);
  else
#endif
  if (argc == 0)
    return make_arg_array (NULL, NULL, gc);
  else if (argc == 1)
    return make_arg_array (p[0], NULL, gc);
  else if (argc == 2)
    return make_arg_array (p[0], p[1], gc);
  else
    return make_arg_copy (p, gc);
}

void
openvpn_sleep (const int n)
{
#ifdef ENABLE_MANAGEMENT
  if (management)
    {
      management_event_loop_n_seconds (management, n);
      return;
    }
#endif
  sleep (n);
}

/*
 * Return the next largest power of 2
 * or u if u is a power of 2.
 */
size_t
adjust_power_of_2 (size_t u)
{
  size_t ret = 1;

  while (ret < u)
    {
      ret <<= 1;
      ASSERT (ret > 0);
    }

  return ret;
}

/*
 * A printf-like function (that only recognizes a subset of standard printf
 * format operators) that prints arguments to an argv list instead
 * of a standard string.  This is used to build up argv arrays for passing
 * to execve.
 */

void
argv_init (struct argv *a)
{
  a->capacity = 0;
  a->argc = 0;
  a->argv = NULL;
  a->system_str = NULL;
}

struct argv
argv_new (void)
{
  struct argv ret;
  argv_init (&ret);
  return ret;
}

void
argv_reset (struct argv *a)
{
  size_t i;
  for (i = 0; i < a->argc; ++i)
    free (a->argv[i]);
  free (a->argv);
  free (a->system_str);
  argv_init (a);
}

static void
argv_extend (struct argv *a, const size_t newcap)
{
  if (newcap > a->capacity)
    {
      char **newargv;
      size_t i;
      ALLOC_ARRAY_CLEAR (newargv, char *, newcap);
      for (i = 0; i < a->argc; ++i)
	newargv[i] = a->argv[i];
      free (a->argv);
      a->argv = newargv;
      a->capacity = newcap;
    }
}

static void
argv_grow (struct argv *a, const size_t add)
{
  const size_t newargc = a->argc + add + 1;
  ASSERT (newargc > a->argc);
  argv_extend (a, adjust_power_of_2 (newargc));
}

static void
argv_append (struct argv *a, char *str) /* str must have been malloced or be NULL */
{
  argv_grow (a, 1);
  a->argv[a->argc++] = str;
}

static void
argv_system_str_append (struct argv *a, const char *str, const bool enquote)
{
  if (str)
    {
      char *newstr;

      /* compute length of new system_str */
      size_t l = strlen (str) + 1; /* space for new string plus trailing '\0' */
      if (a->system_str)
	l += strlen (a->system_str) + 1; /* space for existing string + space (" ") separator */
      if (enquote)
	l += 2; /* space for two quotes */

      /* build new system_str */
      newstr = (char *) malloc (l);
      newstr[0] = '\0';
      check_malloc_return (newstr);
      if (a->system_str)
	{
	  strcpy (newstr, a->system_str);
	  strcat (newstr, " ");
	}
      if (enquote)
	strcat (newstr, "\"");
      strcat (newstr, str);
      if (enquote)
	strcat (newstr, "\"");
      free (a->system_str);
      a->system_str = newstr;
    }
}

static char *
argv_extract_cmd_name (const char *path)
{
  if (path)
    {
      const char *bn = openvpn_basename (path);
      if (bn)
	{
	  char *ret = string_alloc (bn, NULL);
	  char *dot = strrchr (ret, '.');
	  if (dot)
	    *dot = '\0';
	  if (ret[0] != '\0')
	    return ret;
	}
    }
  return NULL;
}

const char *
argv_system_str (const struct argv *a)
{
  return a->system_str;
}

struct argv
argv_clone (const struct argv *a, const size_t headroom)
{
  struct argv r;
  size_t i;

  argv_init (&r);
  for (i = 0; i < headroom; ++i)
    argv_append (&r, NULL);
  if (a)
    {
      for (i = 0; i < a->argc; ++i)
	argv_append (&r, string_alloc (a->argv[i], NULL));
      r.system_str = string_alloc (a->system_str, NULL);
    }
  return r;
}

struct argv
argv_insert_head (const struct argv *a, const char *head)
{
  struct argv r;
  char *s;

  r = argv_clone (a, 1);
  r.argv[0] = string_alloc (head, NULL);
  s = r.system_str;
  r.system_str = string_alloc (head, NULL);
  if (s)
    {
      argv_system_str_append (&r, s, false);
      free (s);
    }
  return r;
}

char *
argv_term (const char **f)
{
  const char *p = *f;
  const char *term = NULL;
  size_t termlen = 0;

  if (*p == '\0')
    return NULL;

  while (true)
    {
      const int c = *p;
      if (c == '\0')
	break;
      if (term)
	{
	  if (!isspace (c))
	    ++termlen;
	  else
	    break;
	}
      else
	{
	  if (!isspace (c))
	    {
	      term = p;
	      termlen = 1;
	    }
	}
      ++p;
    }
  *f = p;

  if (term)
    {
      char *ret;
      ASSERT (termlen > 0);
      ret = malloc (termlen + 1);
      check_malloc_return (ret);
      memcpy (ret, term, termlen);
      ret[termlen] = '\0';
      return ret;
    }
  else
    return NULL;
}

const char *
argv_str (const struct argv *a, struct gc_arena *gc, const unsigned int flags)
{
  if (a->argv)
    return print_argv ((const char **)a->argv, gc, flags);
  else
    return "";
}

void
argv_msg (const int msglev, const struct argv *a)
{
  struct gc_arena gc = gc_new ();
  msg (msglev, "%s", argv_str (a, &gc, 0));
  gc_free (&gc);
}

void
argv_msg_prefix (const int msglev, const struct argv *a, const char *prefix)
{
  struct gc_arena gc = gc_new ();
  msg (msglev, "%s: %s", prefix, argv_str (a, &gc, 0));
  gc_free (&gc);
}

void
argv_printf (struct argv *a, const char *format, ...)
{
  va_list arglist;
  va_start (arglist, format);
  argv_printf_arglist (a, format, 0, arglist);
  va_end (arglist);
 }

void
argv_printf_cat (struct argv *a, const char *format, ...)
{
  va_list arglist;
  va_start (arglist, format);
  argv_printf_arglist (a, format, APA_CAT, arglist);
  va_end (arglist);
}

void
argv_printf_arglist (struct argv *a, const char *format, const unsigned int flags, va_list arglist)
{
  struct gc_arena gc = gc_new ();
  char *term;
  const char *f = format;

  if (!(flags & APA_CAT))
    argv_reset (a);
  argv_extend (a, 1); /* ensure trailing NULL */

  while ((term = argv_term (&f)) != NULL) 
    {
      if (term[0] == '%')
	{
	  if (!strcmp (term, "%s"))
	    {
	      char *s = va_arg (arglist, char *);
	      if (!s)
		s = "";
	      argv_append (a, string_alloc (s, NULL));
	      argv_system_str_append (a, s, true);
	    }
	  else if (!strcmp (term, "%sc"))
	    {
	      char *s = va_arg (arglist, char *);
	      if (s)
		{
		  int nparms;
		  char *parms[MAX_PARMS+1];
		  int i;

		  nparms = parse_line (s, parms, MAX_PARMS, "SCRIPT-ARGV", 0, D_ARGV_PARSE_CMD, &gc);
		  if (nparms)
		    {
		      for (i = 0; i < nparms; ++i)
			argv_append (a, string_alloc (parms[i], NULL));
		    }
		  else
		    argv_append (a, string_alloc (s, NULL));

		  argv_system_str_append (a, s, false);
		}
	      else
		{
		  argv_append (a, string_alloc ("", NULL));
		  argv_system_str_append (a, "echo", false);
		}
	    }
	  else if (!strcmp (term, "%d"))
	    {
	      char numstr[64];
	      openvpn_snprintf (numstr, sizeof (numstr), "%d", va_arg (arglist, int));
	      argv_append (a, string_alloc (numstr, NULL));
	      argv_system_str_append (a, numstr, false);
	    }
	  else if (!strcmp (term, "%u"))
	    {
	      char numstr[64];
	      openvpn_snprintf (numstr, sizeof (numstr), "%u", va_arg (arglist, unsigned int));
	      argv_append (a, string_alloc (numstr, NULL));
	      argv_system_str_append (a, numstr, false);
	    }
	  else if (!strcmp (term, "%s/%d"))
	    {
	      char numstr[64];
	      char *s = va_arg (arglist, char *);

	      if (!s)
		s = "";

	      openvpn_snprintf (numstr, sizeof (numstr), "%d", va_arg (arglist, int));

	      {
		const size_t len = strlen(s) + strlen(numstr) + 2;
		char *combined = (char *) malloc (len);
		check_malloc_return (combined);

		strcpy (combined, s);
		strcat (combined, "/");
		strcat (combined, numstr);
		argv_append (a, combined);
		argv_system_str_append (a, combined, false);
	      }
	    }
	  else if (!strcmp (term, "%s%sc"))
	    {
	      char *s1 = va_arg (arglist, char *);
	      char *s2 = va_arg (arglist, char *);
	      char *combined;
	      char *cmd_name;

	      if (!s1) s1 = "";
	      if (!s2) s2 = "";
	      combined = (char *) malloc (strlen(s1) + strlen(s2) + 1);
	      check_malloc_return (combined);
	      strcpy (combined, s1);
	      strcat (combined, s2);
	      argv_append (a, combined);

	      cmd_name = argv_extract_cmd_name (combined);
	      if (cmd_name)
		{
		  argv_system_str_append (a, cmd_name, false);
		  free (cmd_name);
		}
	    }
	  else
	    ASSERT (0);
	  free (term);
	}
      else
	{
	  argv_append (a, term);
	  argv_system_str_append (a, term, false);
	}
    }
  gc_free (&gc);
}

#ifdef ARGV_TEST
void
argv_test (void)
{
  struct gc_arena gc = gc_new ();
  const char *s;

  struct argv a;

  argv_init (&a);
  argv_printf (&a, "%sc foo bar %s", "c:\\\\src\\\\test\\\\jyargs.exe", "foo bar");
  argv_msg_prefix (M_INFO, &a, "ARGV");
  msg (M_INFO, "ARGV-S: %s", argv_system_str(&a));
  //openvpn_execve_check (&a, NULL, 0, "command failed");

  argv_printf (&a, "%sc %s %s", "c:\\\\src\\\\test files\\\\batargs.bat", "foo", "bar");  
  argv_msg_prefix (M_INFO, &a, "ARGV");
  msg (M_INFO, "ARGV-S: %s", argv_system_str(&a));
  //openvpn_execve_check (&a, NULL, 0, "command failed");

  argv_printf (&a, "%s%sc foo bar %s %s/%d %d %u", "/foo", "/bar.exe", "one two", "1.2.3.4", 24, -69, 96);
  argv_msg_prefix (M_INFO, &a, "ARGV");
  msg (M_INFO, "ARGV-S: %s", argv_system_str(&a));
  //openvpn_execve_check (&a, NULL, 0, "command failed");

  argv_printf (&a, "this is a %s test of int %d unsigned %u", "FOO", -69, 42);
  s = argv_str (&a, &gc, PA_BRACKET);
  printf ("PF: %s\n", s);
  printf ("PF-S: %s\n", argv_system_str(&a));

  {
    struct argv b = argv_insert_head (&a, "MARK");
    s = argv_str (&b, &gc, PA_BRACKET);
    printf ("PF: %s\n", s);
    printf ("PF-S: %s\n", argv_system_str(&b));
    argv_reset (&b);
  }

  argv_printf (&a, "%sc foo bar %d", "\"multi term\" command      following \\\"spaces", 99);
  s = argv_str (&a, &gc, PA_BRACKET);
  printf ("PF: %s\n", s);
  printf ("PF-S: %s\n", argv_system_str(&a));
  argv_reset (&a);

  s = argv_str (&a, &gc, PA_BRACKET);
  printf ("PF: %s\n", s);
  printf ("PF-S: %s\n", argv_system_str(&a));
  argv_reset (&a);

  argv_printf (&a, "foo bar %d", 99);
  argv_printf_cat (&a, "bar %d foo %sc", 42, "nonesuch");
  argv_printf_cat (&a, "cool %s %d u %s/%d end", "frood", 4, "hello", 7);
  s = argv_str (&a, &gc, PA_BRACKET);
  printf ("PF: %s\n", s);
  printf ("PF-S: %s\n", argv_system_str(&a));
  argv_reset (&a);

#if 0
  {
    char line[512];
    while (fgets (line, sizeof(line), stdin) != NULL)
      {
	char *term;
	const char *f = line;
	int i = 0;

	while ((term = argv_term (&f)) != NULL) 
	  {
	    printf ("[%d] '%s'\n", i, term);
	    ++i;
	    free (term);
	  }
      }
  }
#endif

  argv_reset (&a);
  gc_free (&gc);
}
#endif

const char *
openvpn_basename (const char *path)
{
  const char *ret;
  const int dirsep = OS_SPECIFIC_DIRSEP;

  if (path)
    {
      ret = strrchr (path, dirsep);
      if (ret && *ret)
	++ret;
      else
	ret = path;
      if (*ret)
	return ret;
    }
  return NULL;
}
