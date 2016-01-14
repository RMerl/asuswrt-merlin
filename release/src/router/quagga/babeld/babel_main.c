/*  
 *  This file is free software: you may copy, redistribute and/or modify it  
 *  under the terms of the GNU General Public License as published by the  
 *  Free Software Foundation, either version 2 of the License, or (at your  
 *  option) any later version.  
 *  
 *  This file is distributed in the hope that it will be useful, but  
 *  WITHOUT ANY WARRANTY; without even the implied warranty of  
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU  
 *  General Public License for more details.  
 *  
 *  You should have received a copy of the GNU General Public License  
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  
 *  
 * This file incorporates work covered by the following copyright and  
 * permission notice:  
 *  

Copyright 2011 by Matthieu Boutier and Juliusz Chroboczek

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/* include zebra library */
#include <zebra.h>
#include "getopt.h"
#include "if.h"
#include "log.h"
#include "thread.h"
#include "privs.h"
#include "sigevent.h"
#include "version.h"
#include "command.h"
#include "vty.h"
#include "memory.h"

#include "babel_main.h"
#include "babeld.h"
#include "util.h"
#include "kernel.h"
#include "babel_interface.h"
#include "neighbour.h"
#include "route.h"
#include "xroute.h"
#include "message.h"
#include "resend.h"
#include "babel_zebra.h"


static void babel_init (int argc, char **argv);
static char *babel_get_progname(char *argv_0);
static void babel_fail(void);
static void babel_init_random(void);
static void babel_replace_by_null(int fd);
static void babel_init_signals(void);
static void babel_exit_properly(void);
static void babel_save_state_file(void);


struct thread_master *master;     /* quagga's threads handler */
struct timeval babel_now;         /* current time             */

unsigned char myid[8];            /* unique id (mac address of an interface) */
int debug = 0;

int resend_delay = -1;
static const char *pidfile = PATH_BABELD_PID;

const unsigned char zeroes[16] = {0};
const unsigned char ones[16] =
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static const char *state_file = DAEMON_VTY_DIR "/babel-state";

unsigned char protocol_group[16]; /* babel's link-local multicast address */
int protocol_port;                /* babel's port */
int protocol_socket = -1;         /* socket: communicate with others babeld */

static char babel_config_default[] = SYSCONFDIR BABEL_DEFAULT_CONFIG;
static char *babel_config_file = NULL;
static char *babel_vty_addr = NULL;
static int babel_vty_port = BABEL_VTY_PORT;

/* Babeld options. */
struct option longopts[] =
{
    { "daemon",      no_argument,       NULL, 'd'},
    { "config_file", required_argument, NULL, 'f'},
    { "pid_file",    required_argument, NULL, 'i'},
    { "socket",      required_argument, NULL, 'z'},
    { "help",        no_argument,       NULL, 'h'},
    { "vty_addr",    required_argument, NULL, 'A'},
    { "vty_port",    required_argument, NULL, 'P'},
    { "user",        required_argument, NULL, 'u'},
    { "group",       required_argument, NULL, 'g'},
    { "version",     no_argument,       NULL, 'v'},
    { 0 }
};

/* babeld privileges */
static zebra_capabilities_t _caps_p [] =
{
    ZCAP_NET_RAW,
    ZCAP_BIND
};
static struct zebra_privs_t babeld_privs =
{
#if defined(QUAGGA_USER)
    .user = QUAGGA_USER,
#endif
#if defined QUAGGA_GROUP
    .group = QUAGGA_GROUP,
#endif
#ifdef VTY_GROUP
    .vty_group = VTY_GROUP,
#endif
    .caps_p = _caps_p,
    .cap_num_p = 2,
    .cap_num_i = 0
};


int
main(int argc, char **argv)
{
    struct thread thread;
    /* and print banner too */
    babel_init(argc, argv);
    while (thread_fetch (master, &thread)) {
        thread_call (&thread);
    }
    return 0;
}

static void
babel_usage (char *progname, int status)
{
  if (status != 0)
    fprintf (stderr, "Try `%s --help' for more information.\n", progname);
  else
    {
      printf ("Usage : %s [OPTION...]\n\
Daemon which manages Babel routing protocol.\n\n\
-d, --daemon       Runs in daemon mode\n\
-f, --config_file  Set configuration file name\n\
-i, --pid_file     Set process identifier file name\n\
-z, --socket       Set path of zebra socket\n\
-A, --vty_addr     Set vty's bind address\n\
-P, --vty_port     Set vty's port number\n\
-u, --user         User to run as\n\
-g, --group        Group to run as\n\
-v, --version      Print program version\n\
-h, --help         Display this help and exit\n\
\n\
Report bugs to %s\n", progname, ZEBRA_BUG_ADDRESS);
    }
  exit (status);
}

/* make initialisations witch don't need infos about kernel(interfaces, etc.) */
static void
babel_init(int argc, char **argv)
{
    int rc, opt;
    int do_daemonise = 0;
    char *progname = NULL;

    /* Set umask before anything for security */
    umask (0027);
    progname = babel_get_progname(argv[0]);

    /* set default log (lib/log.h) */
    zlog_default = openzlog(progname, ZLOG_BABEL,
                            LOG_CONS|LOG_NDELAY|LOG_PID, LOG_DAEMON);
    /* set log destination as stdout until the config file is read */
    zlog_set_level(NULL, ZLOG_DEST_STDOUT, LOG_WARNING);

    babel_init_random();

    /* set the Babel's default link-local multicast address and Babel's port */
    parse_address("ff02:0:0:0:0:0:1:6", protocol_group, NULL);
    protocol_port = 6696;

    /* get options */
    while(1) {
        opt = getopt_long(argc, argv, "df:i:z:hA:P:u:g:v", longopts, 0);
        if(opt < 0)
            break;

        switch(opt) {
            case 0:
                break;
            case 'd':
                do_daemonise = -1;
                break;
            case 'f':
                babel_config_file = optarg;
                break;
            case 'i':
                pidfile = optarg;
                break;
            case 'z':
                zclient_serv_path_set (optarg);
                break;
            case 'A':
                babel_vty_addr = optarg;
                break;
            case 'P':
                babel_vty_port = atoi (optarg);
                if (babel_vty_port <= 0 || babel_vty_port > 0xffff)
                    babel_vty_port = BABEL_VTY_PORT;
                break;
            case 'u':
                babeld_privs.user = optarg;
                break;
            case 'g':
                babeld_privs.group = optarg;
                break;
            case 'v':
                print_version (progname);
                exit (0);
                break;
            case 'h':
                babel_usage (progname, 0);
                break;
            default:
                babel_usage (progname, 1);
                break;
        }
    }

    /* create the threads handler */
    master = thread_master_create ();

    /* Library inits. */
    zprivs_init (&babeld_privs);
    babel_init_signals();
    cmd_init (1);
    vty_init (master);
    memory_init ();

    resend_delay = BABEL_DEFAULT_RESEND_DELAY;

    babel_replace_by_null(STDIN_FILENO);

    if (do_daemonise && daemonise() < 0) {
        zlog_err("daemonise: %s", safe_strerror(errno));
        exit (1);
    }

    /* write pid file */
    if (pid_output(pidfile) < 0) {
        zlog_err("error while writing pidfile");
        exit (1);
    };

    /* init some quagga's dependencies, and babeld's commands */
    babeld_quagga_init();
    /* init zebra client's structure and it's commands */
    /* this replace kernel_setup && kernel_setup_socket */
    babelz_zebra_init ();

    /* Get zebra configuration file. */
    zlog_set_level (NULL, ZLOG_DEST_STDOUT, ZLOG_DISABLED);
    vty_read_config (babel_config_file, babel_config_default);

    /* Create VTY socket */
    vty_serv_sock (babel_vty_addr, babel_vty_port, BABEL_VTYSH_PATH);

    /* init buffer */
    rc = resize_receive_buffer(1500);
    if(rc < 0)
        babel_fail();

    schedule_neighbours_check(5000, 1);

    zlog_notice ("BABELd %s starting: vty@%d", BABEL_VERSION, babel_vty_port);
}

/* return the progname (without path, example: "./x/progname" --> "progname") */
static char *
babel_get_progname(char *argv_0) {
    char *p = strrchr (argv_0, '/');
    return (p ? ++p : argv_0);
}

static void
babel_fail(void)
{
    exit(1);
}

/* initialize random value, and set 'babel_now' by the way. */
static void
babel_init_random(void)
{
    gettime(&babel_now);
    int rc;
    unsigned int seed;

    rc = read_random_bytes(&seed, sizeof(seed));
    if(rc < 0) {
        zlog_err("read(random): %s", safe_strerror(errno));
        seed = 42;
    }

    seed ^= (babel_now.tv_sec ^ babel_now.tv_usec);
    srandom(seed);
}

/*
 close fd, and replace it by "/dev/null"
 exit if error
 */
static void
babel_replace_by_null(int fd)
{
    int fd_null;
    int rc;

    fd_null = open("/dev/null", O_RDONLY);
    if(fd_null < 0) {
        zlog_err("open(null): %s", safe_strerror(errno));
        exit(1);
    }

    rc = dup2(fd_null, fd);
    if(rc < 0) {
        zlog_err("dup2(null, 0): %s", safe_strerror(errno));
        exit(1);
    }

    close(fd_null);
}

/*
 Load the state file: check last babeld's running state, usefull in case of
 "/etc/init.d/babeld restart"
 */
void
babel_load_state_file(void)
{
    int fd;
    int rc;

    fd = open(state_file, O_RDONLY);
    if(fd < 0 && errno != ENOENT)
        zlog_err("open(babel-state: %s)", safe_strerror(errno));
    rc = unlink(state_file);
    if(fd >= 0 && rc < 0) {
        zlog_err("unlink(babel-state): %s", safe_strerror(errno));
        /* If we couldn't unlink it, it's probably stale. */
        close(fd);
        fd = -1;
    }
    if(fd >= 0) {
        char buf[100];
        char buf2[100];
        int s;
        long t;
        rc = read(fd, buf, 99);
        if(rc < 0) {
            zlog_err("read(babel-state): %s", safe_strerror(errno));
        } else {
            buf[rc] = '\0';
            rc = sscanf(buf, "%99s %d %ld\n", buf2, &s, &t);
            if(rc == 3 && s >= 0 && s <= 0xFFFF) {
                unsigned char sid[8];
                rc = parse_eui64(buf2, sid);
                if(rc < 0) {
                    zlog_err("Couldn't parse babel-state.");
                } else {
                    struct timeval realnow;
                    debugf(BABEL_DEBUG_COMMON,
                           "Got %s %d %ld from babel-state.",
                           format_eui64(sid), s, t);
                    gettimeofday(&realnow, NULL);
                    if(memcmp(sid, myid, 8) == 0)
                        myseqno = seqno_plus(s, 1);
                    else
                        zlog_err("ID mismatch in babel-state. id=%s; old=%s",
                                 format_eui64(myid),
                                 format_eui64(sid));
                }
            } else {
                zlog_err("Couldn't parse babel-state.");
            }
        }
        close(fd);
        fd = -1;
    }
}

static void
babel_sigexit(void)
{
    zlog_notice("Terminating on signal");

    babel_exit_properly();
}

static void
babel_sigusr1 (void)
{
    zlog_rotate (NULL);
}

static void
babel_init_signals(void)
{
    static struct quagga_signal_t babel_signals[] =
    {
        {
            .signal = SIGUSR1,
            .handler = &babel_sigusr1,
        },
        {
            .signal = SIGINT,
            .handler = &babel_sigexit,
        },
        {
            .signal = SIGTERM,
            .handler = &babel_sigexit,
        },
    };

    signal_init (master, array_size(babel_signals), babel_signals);
}

static void
babel_exit_properly(void)
{
    debugf(BABEL_DEBUG_COMMON, "Exiting...");
    usleep(roughly(10000));
    gettime(&babel_now);

    /* Uninstall and flush all routes. */
    debugf(BABEL_DEBUG_COMMON, "Uninstall routes.");
    flush_all_routes();
    babel_interface_close_all();
    babel_zebra_close_connexion();
    babel_save_state_file();
    debugf(BABEL_DEBUG_COMMON, "Remove pid file.");
    if(pidfile)
        unlink(pidfile);
    debugf(BABEL_DEBUG_COMMON, "Done.");

    exit(0);
}

static void
babel_save_state_file(void)
{
    int fd;
    int rc;

    debugf(BABEL_DEBUG_COMMON, "Save state file.");
    fd = open(state_file, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if(fd < 0) {
        zlog_err("creat(babel-state): %s", safe_strerror(errno));
        unlink(state_file);
    } else {
        struct timeval realnow;
        char buf[100];
        gettimeofday(&realnow, NULL);
        rc = snprintf(buf, 100, "%s %d %ld\n",
                      format_eui64(myid), (int)myseqno,
                      (long)realnow.tv_sec);
        if(rc < 0 || rc >= 100) {
            zlog_err("write(babel-state): overflow.");
            unlink(state_file);
        } else {
            rc = write(fd, buf, rc);
            if(rc < 0) {
                zlog_err("write(babel-state): %s", safe_strerror(errno));
                unlink(state_file);
            }
            fsync(fd);
        }
        close(fd);
    }
}

void
show_babel_main_configuration (struct vty *vty)
{
    vty_out(vty,
            "pid file                = %s%s"
            "state file              = %s%s"
            "configuration file      = %s%s"
            "protocol informations:%s"
            "  multicast address     = %s%s"
            "  port                  = %d%s"
            "vty address             = %s%s"
            "vty port                = %d%s"
            "id                      = %s%s"
            "allow_duplicates        = %s%s"
            "kernel_metric           = %d%s",
            pidfile, VTY_NEWLINE,
            state_file, VTY_NEWLINE,
            babel_config_file ? babel_config_file : babel_config_default,
            VTY_NEWLINE,
            VTY_NEWLINE,
            format_address(protocol_group), VTY_NEWLINE,
            protocol_port, VTY_NEWLINE,
            babel_vty_addr ? babel_vty_addr : "None",
            VTY_NEWLINE,
            babel_vty_port, VTY_NEWLINE,
            format_eui64(myid), VTY_NEWLINE,
            format_bool(allow_duplicates), VTY_NEWLINE,
            kernel_metric, VTY_NEWLINE);
}
