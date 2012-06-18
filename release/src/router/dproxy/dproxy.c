/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>

#include "dproxy.h"
#include "dns_decode.h"
#include "cache.h"
#include "conf.h"
#include "dns_list.h"
#include "dns_construct.h"
#include "dns_io.h"

/*****************************************************************************/
/*****************************************************************************/
int dns_main_quit;
int dns_sock;
int srv_sock;
fd_set rfds;
dns_request_t *dns_request_list;
struct in_addr ns_addr[MAX_NS_COUNT];
int ns_count;
int ns_shift;
/*****************************************************************************/
int is_connected()
{
  FILE *fp;

  if(!config.ppp_detect)return 1;

  fp = fopen( config.ppp_device_file, "r" );
  if(!fp)return 0;
  fclose(fp);
  return 1;
}
/*****************************************************************************/
void load_resolv_entries(char *resolv_file, int new_shift)
{
  FILE *fp;
  struct in_addr in;
  char line[81], dns_ser_ip[81];

  ns_count = 0;
  ns_shift = new_shift;
  fp = fopen(config.resolv_file, "r");
  if (!fp)
	return;
  while (
    ns_count < MAX_NS_COUNT &&
    fgets(line, 80, fp) != NULL &&
    sscanf(line, "nameserver %s", dns_ser_ip) == 1 &&
    inet_aton(dns_ser_ip, &in) && in.s_addr != INADDR_ANY)
  {
    ns_addr[ns_count++].s_addr = in.s_addr;
  }
  fclose(fp);
}
/*****************************************************************************/
int dns_prepare_socket(int port)
{
  struct sockaddr_in sa;
  int fd = -1;
  int flags;

  /* Clear it out */
  memset((void *)&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = INADDR_ANY;
  sa.sin_port = htons(port);

  fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (fd < 0)
	return fd;

  fcntl(fd, F_SETFD, FD_CLOEXEC);

  flags = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*) &flags, sizeof(flags)) < 0 ){
	printf("Could not set reuse option\n");
	debug_perror("setsockopt");
	goto err;
  }

  flags = fcntl(fd, F_GETFL);
  if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
	printf("Could not set socket nonblock\n");
	debug_perror("fcntl");
	goto err;
  }

  if (bind(fd, (struct sockaddr *) &sa, sizeof(struct sockaddr)) < 0) {
	printf("Could not bind socket to port %d\n", port);
	debug_perror("bind");
	goto err;
  }

  return fd;

err:
  close(fd);
  return -1;
}
/*****************************************************************************/
int dns_init()
{
  srv_sock = dns_prepare_socket(0);

  /* Error */
  if (srv_sock < 0) {
	 printf("Could not create server socket\n");
	 debug_perror("Could not create server socket");
	 exit(1);
  }

  dns_sock = dns_prepare_socket(PORT);

  /* Error */
  if( dns_sock < 0 ){
	 printf("Could not create socket\n");
	 debug_perror("Could not create socket");
	 exit(1);
  }

  dns_main_quit = 0;

  FD_ZERO( &rfds );
  FD_SET( dns_sock, &rfds );
  FD_SET( srv_sock, &rfds );

  dns_request_list = NULL;

  load_resolv_entries(config.resolv_file, 0);
  cache_purge( config.purge_time );

  return 1;
}
/*****************************************************************************/
void forward_dns_query(dns_request_t *node, dns_request_t *m)
{
  struct in_addr in;
  int try, ns_index = 0;

  for (try = 0; try <= ns_count; try++) {
    if (/* ns_count > 0 && */ try < ns_count) {
      ns_index = (ns_shift + try) % ns_count;
      in.s_addr = ns_addr[ns_index].s_addr;
    } else
      inet_aton(config.name_server, &in);

    if (in.s_addr == INADDR_ANY)
      continue;

    debug("forward_dns_query: query DNS server -- %s\n", inet_ntoa(in));
    if (dns_write_packet(srv_sock, in, PORT, m) > 0) {
      ns_shift = ns_index;
      return;
    }
  }

  debug("forward_dns_query: no DNS servers available\n");
}
/*****************************************************************************/
void dns_handle_new_query(dns_request_t *m)
{
  //struct in_addr in;
  int retval = 0;	/* modified by CMC from retval=-1 2002/12/6 */

  if( m->message.question[0].type == A || m->message.question[0].type == AAA){
    /* added by CMC to deny name 2002/11/19 */
    if ( deny_lookup_name( m->cname ) ) {
      debug("%s --> blocked.\n", m->cname);
      dns_construct_error_reply(m);
      dns_write_packet( dns_sock, m->src_addr, m->src_port, m );
      return;
    }
    /* standard query */
    retval = cache_lookup_name( m->cname, m->ip );
  }else if( m->message.question[0].type == PTR ){
    /* reverse lookup */
    retval = cache_lookup_ip( m->ip, m->cname );
  }

  debug(".......... %s ---- %s\n", m->cname, m->ip );

  switch( retval )
    {
    case 0:
      if( is_connected() ){
	debug("Adding to list-> id: %d\n", m->message.header.id);
	dns_request_list = dns_list_add( dns_request_list, m );
	/* relay the query untouched */
	forward_dns_query( dns_request_list, m );  /* modified by CMC 8/3/2001 */
      }else{
	debug("Not connected **\n");
	dns_construct_error_reply(m);
	dns_write_packet( dns_sock, m->src_addr, m->src_port, m );
      }
      break;
    case 1:
      dns_construct_reply( m );
      dns_write_packet( dns_sock, m->src_addr, m->src_port, m );
      debug("Cache hit\n");
      break;
    default:
      debug("Unknown query type: %d\n", m->message.question[0].type );
      debug("CMC: Here is un-reachable code! (2002/12/6)\n");
    }

}
/*****************************************************************************/
void dns_handle_response(dns_request_t *m)
{
  dns_request_t *ptr = NULL;

  ptr = dns_list_find_by_id(dns_request_list, m);
  if (ptr != NULL) {
    debug("Found query in list\n");
    /* message may be a response */
    if(m->message.header.flags.f.question == 1) {
      dns_write_packet(dns_sock, ptr->src_addr, ptr->src_port, m);
      debug("Replying with answer from %s\n", inet_ntoa(m->src_addr));
      if (m->message.header.flags.f.rcode == 0 &&
          (ptr->message.question[0].type == A || ptr->message.question[0].type == PTR)) {
	debug("Cache append: %s ----> %s\n", m->cname, m->ip);
	cache_name_append(m->cname, m->ip);
      }
      dns_request_list = dns_list_remove( dns_request_list, ptr );
    }
  }
}
/*****************************************************************************/
void dns_handle_query(dns_request_t *m)
{
  dns_request_t *ptr = NULL;

  ptr = dns_list_find_by_id(dns_request_list, m);
  if (ptr != NULL) {
    ns_shift++;
    ptr->duplicate_queries++;
    debug("Duplicate query(%d)\n", ptr->duplicate_queries);
    forward_dns_query(ptr, m);
  } else
    dns_handle_new_query(m);
}
/*****************************************************************************/
int dns_main_loop()
{
  struct timeval tv;
  fd_set active_rfds;
  int retval;
  dns_request_t m;
  dns_request_t *ptr, *next;
  //int purge_time = config.purge_time / 60;
  int purge_time = CACHE_CHECK_TIME / DNS_TICK_TIME;	//(30sec) modified by CMC 8/4/2001
  int max_fd;

  while( !dns_main_quit ){

    /* set the one second time out */
    tv.tv_sec = DNS_TICK_TIME;	  //modified by CMC 8/3/2001
    tv.tv_usec = 0;

    /* now copy the main rfds in the active one as it gets modified by select*/
    active_rfds = rfds;

    max_fd = (dns_sock > srv_sock) ? dns_sock : srv_sock;
    retval = select(max_fd + 1, &active_rfds, NULL, NULL, &tv);
    if (retval < 0) {
      /* EINTR? A signal was caught, don't panic */
      continue;
    }

    if (retval){
      /* data is now available */
      if (FD_ISSET(srv_sock, &active_rfds) &&
          dns_read_packet(srv_sock, &m) != -1)
        dns_handle_response(&m);
      if (FD_ISSET(dns_sock, &active_rfds) &&
          dns_read_packet(dns_sock, &m) != -1)
        dns_handle_query(&m);
    }else{
      /* select time out */
      ptr = dns_request_list;
      while( ptr ){
	next = ptr->next;
	ptr->time_pending++;
	if( ptr->time_pending > DNS_TIMEOUT/DNS_TICK_TIME ){
	  /* CMC: ptr->time_pending= DNS_TIMEOUT ~ DNS_TIMEOUT+DNS_TICK_TIME */
	  debug("Request timed out\n");
	  /* send error back */
	  dns_construct_error_reply(ptr);
	  dns_write_packet( dns_sock, ptr->src_addr, ptr->src_port, ptr );
	  dns_request_list = dns_list_remove( dns_request_list, ptr );
	}
	ptr = next;
      } /* while(ptr) */

      /* purge cache */
      purge_time--;
      if( purge_time <= 0 ){			//modified by CMC 8/4/2001
	load_resolv_entries(config.resolv_file, ns_shift);
	cache_purge( config.purge_time );
	//purge_time = config.purge_time / 60;
	purge_time = CACHE_CHECK_TIME / DNS_TICK_TIME; 	//(30sec) modified by CMC 8/3/2001
      }

    } /* if (retval) */
  }
  return 0;
}


#if DNS_DEBUG	//added by CMC 8/4/2001
/*****************************************************************************/
void debug_perror( char * msg ) {
	debug( "%s : %s\n" , msg , strerror(errno) );
}
/*****************************************************************************/
void debug(char *fmt, ...)
{
#define MAX_MESG_LEN 1024

  va_list args;
  char text[ MAX_MESG_LEN ];

  sprintf( text, "[ %d ]: ", getpid());
  va_start (args, fmt);
  vsnprintf( &text[strlen(text)], MAX_MESG_LEN - strlen(text), fmt, args);
  va_end (args);

  if( config.debug_file[0] ){
    FILE *fp;
    fp = fopen( config.debug_file, "w");
    if(!fp){
      syslog( LOG_ERR, "could not open log file %m" );
      return;
    }
    fprintf( fp, "%s", text);
    fclose(fp);
  }

  /** if not in daemon-mode stderr was not closed, use it. */
  if( ! config.daemon_mode ) {
    fprintf( stderr, "%s", text);
  }
}
#else
void debug_perror( char * msg ) {}
void debug(char *fmt, ...) {}
#endif
/*****************************************************************************
 * print usage informations to stderr.
 *
 *****************************************************************************/
void usage(char * program , char * message ) {
  fprintf(stderr,"%s\n" , message );
  fprintf(stderr,"usage : %s [-c <config-file>] [-d] [-h] [-P]\n", program );
  fprintf(stderr,"\t-c <config-file>\tread configuration from <config-file>\n");
#if DNS_DEBUG	//added by CMC 8/6/2001
  fprintf(stderr,"\t-d \t\trun in debug (=non-daemon) mode.\n");
#else
  fprintf(stderr,"\t-d \t\trun in debug (=non-daemon) mode.\n");
  fprintf(stderr,"\t-d \t\tCMC: Please re-compile dproxy with DNS_DEBUG=1\n");
#endif
  fprintf(stderr,"\t-P \t\tprint configuration on stdout and exit.\n");
  fprintf(stderr,"\t-h \t\tthis message.\n");
}
/*****************************************************************************
 * get commandline options.
 *
 * @return 0 on success, < 0 on error.
 *****************************************************************************/
int get_options( int argc, char ** argv )
{
  char c = 0;
  int not_daemon = 0, cc;
  int want_printout = 0;
  char * progname = argv[0];

  conf_defaults();

  while( (cc = getopt( argc, argv, "c:dhP")) != EOF ) {
    c = (char)cc;	//added by CMC 8/3/2001
    switch(c) {
	 case 'c':
  		conf_load(optarg);
		break;
	 case 'd':
		not_daemon = 1;
		break;
	 case 'h':
		usage(progname,"");
		return -1;
	 case 'P':
		want_printout = 1;
		break;
	 default:
		usage(progname,"");
		return -1;
    }
  }

  /** unset daemon-mode if -d was given. */
  if( not_daemon ) {
	 config.daemon_mode = 0;
  }

  if( want_printout ) {
	 conf_print();
	 exit(0);
  }
  return 0;
}
/*****************************************************************************/
void sig_hup (int signo)
{
  signal(SIGHUP, sig_hup); /* set this for the next sighup */
  conf_load (config.config_file);
  load_resolv_entries(config.resolv_file, 0);
}
/*****************************************************************************/
int main(int argc, char **argv)
{

  /* get commandline options, load config if needed. */
  if(get_options( argc, argv ) < 0 ) {
	  exit(1);
  }

  sigset_t sigs_to_catch;
  sigemptyset(&sigs_to_catch);
  sigaddset(&sigs_to_catch, SIGHUP);
  sigprocmask(SIG_UNBLOCK, &sigs_to_catch, NULL);

  signal(SIGHUP, sig_hup);

  dns_init();

  if (config.daemon_mode) {
    /* Standard fork and background code */
    switch (fork()) {
	 case -1:	/* Oh shit, something went wrong */
		debug_perror("fork");
		exit(-1);
	 case 0:	/* Child: close off stdout, stdin and stderr */
		close(0);
		close(1);
		close(2);
		break;
	 default:	/* Parent: Just exit */
		exit(0);
    }
  }

  dns_main_loop();

  return 0;
}

