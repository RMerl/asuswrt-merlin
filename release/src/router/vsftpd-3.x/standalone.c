/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 * standalone.c
 *
 * Code to listen on the network and launch children servants.
 */

#include "standalone.h"

#include "parseconf.h"
#include "tunables.h"
#include "sysutil.h"
#include "sysdeputil.h"
#include "utility.h"
#include "defs.h"
#include "hash.h"
#include "str.h"
#include "ipaddrparse.h"

static unsigned int s_children;
static struct hash* s_p_ip_count_hash;
static struct hash* s_p_pid_ip_hash;
static unsigned int s_ipaddr_size;

static void handle_sigchld(void*  duff);
static void handle_sighup(void*  duff);
static void prepare_child(int sockfd);
static unsigned int handle_ip_count(void* p_raw_addr);
static void drop_ip_count(void* p_raw_addr);

static unsigned int hash_ip(unsigned int buckets, void* p_key);
static unsigned int hash_pid(unsigned int buckets, void* p_key);

struct vsf_client_launch
vsf_standalone_main(void)
{
  struct vsf_sysutil_sockaddr* p_accept_addr = 0;
  int listen_sock = -1;
  int retval;
  s_ipaddr_size = vsf_sysutil_get_ipaddr_size();
  if (tunable_listen && tunable_listen_ipv6)
  {
    die("run two copies of vsftpd for IPv4 and IPv6");
  }
  if (tunable_background)
  {
    int forkret = vsf_sysutil_fork();
    if (forkret > 0)
    {
      /* Parent, just exit */
      vsf_sysutil_exit(0);
    }
    /* Son, close standard FDs to avoid SSH hang-on-exit */
    vsf_sysutil_reopen_standard_fds();
    vsf_sysutil_make_session_leader();
  }
  if (tunable_listen)
  {
    listen_sock = vsf_sysutil_get_ipv4_sock();
  }
  else
  {
    listen_sock = vsf_sysutil_get_ipv6_sock();
  }
  vsf_sysutil_activate_reuseaddr(listen_sock);

  s_p_ip_count_hash = hash_alloc(256, s_ipaddr_size,
                                 sizeof(unsigned int), hash_ip);
  s_p_pid_ip_hash = hash_alloc(256, sizeof(int),
                               s_ipaddr_size, hash_pid);
  if (tunable_setproctitle_enable)
  {
    vsf_sysutil_setproctitle("LISTENER");
  }
  vsf_sysutil_install_sighandler(kVSFSysUtilSigCHLD, handle_sigchld, 0, 1);
  vsf_sysutil_install_sighandler(kVSFSysUtilSigHUP, handle_sighup, 0, 1);
  if (tunable_listen)
  {
    struct vsf_sysutil_sockaddr* p_sockaddr = 0;
    vsf_sysutil_sockaddr_alloc_ipv4(&p_sockaddr);
    vsf_sysutil_sockaddr_set_port(p_sockaddr,
                                  (unsigned short) tunable_listen_port);
    if (!tunable_listen_address)
    {
      vsf_sysutil_sockaddr_set_any(p_sockaddr);
    }
    else
    {
      if (!vsf_sysutil_inet_aton(tunable_listen_address, p_sockaddr))
      {
        die2("bad listen_address: ", tunable_listen_address);
      }
    }
    retval = vsf_sysutil_bind(listen_sock, p_sockaddr);
    vsf_sysutil_free(p_sockaddr);
    if (vsf_sysutil_retval_is_error(retval))
    {
      die("could not bind listening IPv4 socket");
    }
  }
  else
  {
    struct vsf_sysutil_sockaddr* p_sockaddr = 0;
    vsf_sysutil_sockaddr_alloc_ipv6(&p_sockaddr);
    vsf_sysutil_sockaddr_set_port(p_sockaddr,
                                  (unsigned short) tunable_listen_port);
    if (!tunable_listen_address6)
    {
      vsf_sysutil_sockaddr_set_any(p_sockaddr);
    }
    else
    {
      struct mystr addr_str = INIT_MYSTR;
      const unsigned char* p_raw_addr;
      str_alloc_text(&addr_str, tunable_listen_address6);
      p_raw_addr = vsf_sysutil_parse_ipv6(&addr_str);
      str_free(&addr_str);
      if (!p_raw_addr)
      {
        die2("bad listen_address6: ", tunable_listen_address6);
      }
      vsf_sysutil_sockaddr_set_ipv6addr(p_sockaddr, p_raw_addr);
    }
    retval = vsf_sysutil_bind(listen_sock, p_sockaddr);
    vsf_sysutil_free(p_sockaddr);
    if (vsf_sysutil_retval_is_error(retval))
    {
      die("could not bind listening IPv6 socket");
    }
  }
  retval = vsf_sysutil_listen(listen_sock, VSFTP_LISTEN_BACKLOG);
  if (vsf_sysutil_retval_is_error(retval))
  {
    die("could not listen");
  }
  vsf_sysutil_sockaddr_alloc(&p_accept_addr);
  while (1)
  {
    struct vsf_client_launch child_info;
    void* p_raw_addr;
    int new_child;
    int new_client_sock;
    new_client_sock = vsf_sysutil_accept_timeout(
        listen_sock, p_accept_addr, 0);
    if (vsf_sysutil_retval_is_error(new_client_sock))
    {
      continue;
    }
    ++s_children;
    child_info.num_children = s_children;
    child_info.num_this_ip = 0;
    p_raw_addr = vsf_sysutil_sockaddr_get_raw_addr(p_accept_addr);
    child_info.num_this_ip = handle_ip_count(p_raw_addr);
    if (tunable_isolate)
    {
      if (tunable_http_enable && tunable_isolate_network)
      {
        new_child = vsf_sysutil_fork_isolate_all_failok();
      }
      else
      {
        new_child = vsf_sysutil_fork_isolate_failok();
      }
    }
    else
    {
      new_child = vsf_sysutil_fork_failok();
    }
    if (new_child != 0)
    {
      /* Parent context */
      vsf_sysutil_close(new_client_sock);
      if (new_child > 0)
      {
        hash_add_entry(s_p_pid_ip_hash, (void*)&new_child, p_raw_addr);
      }
      else
      {
        /* fork() failed, clear up! */
        --s_children;
        drop_ip_count(p_raw_addr);
      }
      /* Fall through to while() loop and accept() again */
    }
    else
    {
      /* Child context */
      vsf_set_die_if_parent_dies();
      vsf_sysutil_close(listen_sock);
      prepare_child(new_client_sock);
      /* By returning here we "launch" the child process with the same
       * contract as xinetd would provide.
       */
      return child_info;
    }
  }
}

static void
prepare_child(int new_client_sock)
{
  /* We must satisfy the contract: command socket on fd 0, 1, 2 */
  vsf_sysutil_dupfd2(new_client_sock, 0);
  vsf_sysutil_dupfd2(new_client_sock, 1);
  vsf_sysutil_dupfd2(new_client_sock, 2);
  if (new_client_sock > 2)
  {
    vsf_sysutil_close(new_client_sock);
  }
}

static void
drop_ip_count(void* p_raw_addr)
{
  unsigned int count;
  unsigned int* p_count =
    (unsigned int*)hash_lookup_entry(s_p_ip_count_hash, p_raw_addr);
  if (!p_count)
  {
    bug("IP address missing from hash");
  }
  count = *p_count;
  if (!count)
  {
    bug("zero count for IP address");
  }
  count--;
  *p_count = count;
  if (!count)
  {
    hash_free_entry(s_p_ip_count_hash, p_raw_addr);
  }
}

static void
handle_sigchld(void* duff)
{
  unsigned int reap_one = 1;
  (void) duff;
  while (reap_one)
  {
    reap_one = (unsigned int)vsf_sysutil_wait_reap_one();
    if (reap_one)
    {
      struct vsf_sysutil_ipaddr* p_ip;
      /* Account total number of instances */
      --s_children;
      /* Account per-IP limit */
      p_ip = (struct vsf_sysutil_ipaddr*)
        hash_lookup_entry(s_p_pid_ip_hash, (void*)&reap_one);
      drop_ip_count(p_ip);      
      hash_free_entry(s_p_pid_ip_hash, (void*)&reap_one);
    }
  }
}

static void
handle_sighup(void* duff)
{
  (void) duff;
  /* We don't crash the out the listener if an invalid config was added */
  tunables_load_defaults();
  vsf_parseconf_load_file(0, 0);
}

static unsigned int
hash_ip(unsigned int buckets, void* p_key)
{
  const unsigned char* p_raw_ip = (const unsigned char*)p_key;
  unsigned int val = 0;
  int shift = 24;
  unsigned int i;
  for (i = 0; i < s_ipaddr_size; ++i)
  {
    val = val ^ (unsigned int) (p_raw_ip[i] << shift);
    shift -= 8;
    if (shift < 0)
    {
      shift = 24;
    }
  }
  return val % buckets;
}

static unsigned int
hash_pid(unsigned int buckets, void* p_key)
{
  unsigned int* p_pid = (unsigned int*)p_key;
  return (*p_pid) % buckets;
}

static unsigned int
handle_ip_count(void* p_ipaddr)
{
  unsigned int* p_count =
    (unsigned int*)hash_lookup_entry(s_p_ip_count_hash, p_ipaddr);
  unsigned int count;
  if (!p_count)
  {
    count = 1;
    hash_add_entry(s_p_ip_count_hash, p_ipaddr, (void*)&count);
  }
  else
  {
    count = *p_count;
    count++;
    *p_count = count;
  }
  return count;
}

