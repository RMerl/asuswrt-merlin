/*
 * Utility routines to assist with the running of sub-commands
 */

#include <net-snmp/net-snmp-config.h>

#if HAVE_IO_H
#include <io.h>
#endif
#include <stdio.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <sys/types.h>
#include <ctype.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <errno.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <ucd-snmp/errormib.h>

#include "execute.h"
#include "struct.h"

#define setPerrorstatus(x) snmp_log_perror(x)

#ifdef _MSC_VER
#define popen  _popen
#define pclose _pclose
#endif


int
run_shell_command( char *command, char *input,
                   char *output,  int *out_len)	/* Or realloc style ? */
{
#if HAVE_SYSTEM
    int         result;    /* and the return value of the command */

    if (!command)
        return -1;

    DEBUGMSGTL(("run_shell_command", "running %s\n", command));
    DEBUGMSGTL(("run:shell", "running '%s'\n", command));

    result = -1;

    /*
     * Set up the command and run it.
     */
    if (input) {
        FILE       *file;

        if (output) {
            const char *ifname;
            const char *ofname;    /* Filename for output redirection */
            char        shellline[STRMAX];   /* The full command to run */

            ifname = netsnmp_mktemp();
            if(NULL == ifname)
                return -1;
            file = fopen(ifname, "w");
            if(NULL == file) {
                snmp_log(LOG_ERR,"couldn't open temporary file %s\n", ifname);
                unlink(ifname);
                return -1;
            }
            fprintf(file, "%s", input);
            fclose( file );

            ofname = netsnmp_mktemp();
            if(NULL == ofname) {
                if(ifname)
                    unlink(ifname);
                return -1;
            }
            snprintf( shellline, sizeof(shellline), "(%s) < \"%s\" > \"%s\"",
                      command, ifname, ofname );
            result = system(shellline);
            /*
             * If output was requested, then retrieve & return it.
             * Tidy up, and return the result of the command.
             */
            if (out_len && *out_len != 0) {
                int         fd;        /* For processing any output */
                int         len = 0;
                fd = open(ofname, O_RDONLY);
                if(fd >= 0)
                    len  = read( fd, output, *out_len-1 );
                *out_len = len;
                if (len >= 0) output[len] = 0;
                else output[0] = 0;
                if (fd >= 0) close(fd);
            }
            unlink(ofname);
            unlink(ifname);
        } else {
            file = popen(command, "w");
            if (file) {
                fwrite(input, 1, strlen(input), file);
                result = pclose(file);
            }
        }
    } else {
        if (output) {
            FILE* file;

            file = popen(command, "r");
            if (file) {
                *out_len = fread(output, 1, *out_len - 1, file);
                if (*out_len >= 0)
                    output[*out_len] = 0;
                else
                    output[0] = 0;
                result = pclose(file);
            }
        } else
            result = system(command);
    }

    return result;
#else
    return -1;
#endif
}


/*
 * Split the given command up into separate tokens,
 * ready to be passed to 'execv'
 */
char **
tokenize_exec_command( char *command, int *argc )
{
    char ctmp[STRMAX];
    char *cp;
    char **argv;
    int  i;

    argv = (char **) calloc(100, sizeof(char *));
    cp = command;

    for ( i=0; cp; i++ ) {
        memset( ctmp, 0, STRMAX );
        cp = copy_nword( cp, ctmp, STRMAX );
        argv[i] = strdup( ctmp );
        if (i == 99)
            break;
    }
    if (cp) {
        argv[i++] = strdup( cp );
    }
    argv[i] = NULL;
    *argc = i;

    return argv;
}


int
run_exec_command( char *command, char *input,
                  char *output,  int  *out_len)	/* Or realloc style ? */
{
#if HAVE_EXECV
    int ipipe[2];
    int opipe[2];
    int i;
    int pid;
    int result;
    char **argv;
    int argc;

    DEBUGMSGTL(("run:exec", "running '%s'\n", command));
    pipe(ipipe);
    pipe(opipe);
    if ((pid = fork()) == 0) {
        /*
         * Child process
         */

        /*
         * Set stdin/out/err to use the pipe
         *   and close everything else
         */
        close(0);
        dup(  ipipe[0]);
	close(ipipe[1]);

        close(1);
        dup(  opipe[1]);
        close(opipe[0]);
        close(2);
        dup(1);
        for (i = getdtablesize()-1; i>2; i--)
            close(i);

        /*
         * Set up the argv array and execute it
         * This is being run in the child process,
         *   so will release resources when it terminates.
         */
        argv = tokenize_exec_command( command, &argc );
        execv( argv[0], argv );
        perror( argv[0] );
        exit(1);	/* End of child */

    } else if (pid > 0) {
        char            cache[NETSNMP_MAXCACHESIZE];
        char           *cache_ptr;
        ssize_t         count, cache_size, offset = 0;
        int             waited = 0, numfds;
        fd_set          readfds;
        struct timeval  timeout;

        /*
         * Parent process
         */

        /*
	 * Pass the input message (if any) to the child,
         * wait for the child to finish executing, and read
         *    any output into the output buffer (if provided)
         */
	close(ipipe[0]);
	close(opipe[1]);
	if (input) {
	   write(ipipe[1], input, strlen(input));
	   close(ipipe[1]);	/* or flush? */
        }
	else close(ipipe[1]);

        /*
         * child will block if it writes a lot of data and
         * fills up the pipe before exiting, so we read data
         * to keep the pipe empty.
         */
        if (output && ((NULL == out_len) || (0 == *out_len))) {
            DEBUGMSGTL(("run:exec",
                        "invalid params; no output will be returned\n"));
            output = NULL;
        }
        if (output) {
            cache_ptr = output;
            cache_size = *out_len - 1;
        } else {
            cache_ptr = cache;
            cache_size = sizeof(cache);
        }

        /*
         * xxx: some of this code was lifted from get_exec_output
         * in util_funcs.c. Probably should be moved to a common
         * routine for both to use.
         */
        DEBUGMSGTL(("verbose:run:exec","  waiting for child %d...\n", pid));
        numfds = opipe[0] + 1;
        i = NETSNMP_MAXREADCOUNT;
        for (; i; --i) {
            /*
             * set up data for select
             */
            FD_ZERO(&readfds);
            FD_SET(opipe[0],&readfds);
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            DEBUGMSGTL(("verbose:run:exec", "    calling select\n"));
            count = select(numfds, &readfds, NULL, NULL, &timeout);
            if (count == -1) {
                if (EAGAIN == errno)
                    continue;
                else {
                    DEBUGMSGTL(("verbose:run:exec", "      errno %d\n",
                                errno));
                    setPerrorstatus("read");
                    break;
                }
            } else if (0 == count) {
                DEBUGMSGTL(("verbose:run:exec", "      timeout\n"));
                continue;
            }

            if (! FD_ISSET(opipe[0], &readfds)) {
                DEBUGMSGTL(("verbose:run:exec", "    fd not ready!\n"));
                continue;
            }

            /*
             * read data from the pipe, optionally saving to output buffer
             */
            count = read(opipe[0], &cache_ptr[offset], cache_size);
            DEBUGMSGTL(("verbose:run:exec",
                        "    read %d bytes\n", (int)count));
            if (0 == count) {
                int rc;
                /*
                 * we shouldn't get no data, because select should
                 * wait til the fd is ready. before we go back around,
                 * check to see if the child exited.
                 */
                DEBUGMSGTL(("verbose:run:exec", "    no data!\n"));
                if ((rc = waitpid(pid, &result, WNOHANG)) <= 0) {
                    if (rc < 0) {
                        snmp_log_perror("waitpid");
                        break;
                    } else
                        DEBUGMSGTL(("verbose:run:exec",
                                    "      child not done!?!\n"));;
                } else {
                    DEBUGMSGTL(("verbose:run:exec", "      child done\n"));
                    waited = 1; /* don't wait again */
                    break;
                }
            }
            else if (count > 0) {
                /*
                 * got some data. fix up offset, if needed.
                 */
                if(output) {
                    offset += count;
                    cache_size -= count;
                    if (cache_size <= 0) {
                        DEBUGMSGTL(("verbose:run:exec",
                                    "      output full\n"));
                        break;
                    }
                    DEBUGMSGTL(("verbose:run:exec",
                                "    %d left in buffer\n", (int)cache_size));
                }
            }
            else if ((count == -1) && (EAGAIN != errno)) {
                /*
                 * if error, break
                 */
                DEBUGMSGTL(("verbose:run:exec", "      errno %d\n",
                            errno));
                setPerrorstatus("read");
                break;
            }
        }
        DEBUGMSGTL(("verbose:run:exec", "  done reading\n"));
        if (output)
            DEBUGMSGTL(("run:exec", "  got %d bytes\n", *out_len));
            
        /*
         * close pipe to signal that we aren't listenting any more.
         */
        close(opipe[0]);

        /*
         * if we didn't wait successfully above, wait now.
         * xxx-rks: seems like this is a waste of the agent's
         * time. maybe start a time to wait(WNOHANG) once a second,
         * and late the agent continue?
         */
        if ((!waited) && (waitpid(pid, &result, 0) < 0 )) {
            snmp_log_perror("waitpid");
            return -1;
        }

        /*
         * null terminate any output
         */
        if (output) {
	    output[offset] = 0;
	    *out_len = offset;
        }
        DEBUGMSGTL(("run:exec","  child %d finished. result=%d\n",
                    pid,result));

	return WEXITSTATUS(result);

    } else {
        /*
         * Parent process - fork failed
         */
        snmp_log_perror("fork");
	close(ipipe[0]);
	close(ipipe[1]);
	close(opipe[0]);
	close(opipe[1]);
	return -1;
    }
    
#else
    /*
     * If necessary, fall back to using 'system'
     */
    DEBUGMSGTL(("run:exec", "running shell command '%s'\n", command));
    return run_shell_command( command, input, output, out_len );
#endif
}
