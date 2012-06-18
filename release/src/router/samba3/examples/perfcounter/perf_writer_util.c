/* 
 *  Unix SMB/CIFS implementation.
 *  Performance Counter Daemon
 *
 *  Copyright (C) Marcin Krzysztof Porwit    2005
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "perf.h"

extern sig_atomic_t keep_running;

void fatal(char *msg)
{
  perror(msg);
  exit(1);
}

void add_key_raw(TDB_CONTEXT *db, char *keystring, void *databuf, size_t datasize, int flags)
{
  TDB_DATA key, data;

  key.dptr = keystring;
  key.dsize = strlen(keystring);
  data.dptr = databuf;
  data.dsize = datasize;
  fprintf(stderr, "doing insert of [%x] with key [%s] into [%s]\n", 
	  data.dptr, 
	  keystring, 
	  db->name);

  tdb_store(db, key, data, flags);
}

void add_key(TDB_CONTEXT *db, char *keystring, char *datastring, int flags)
{
  TDB_DATA key, data;

  key.dptr = keystring;
  key.dsize = strlen(keystring);
  data.dptr = datastring;
  data.dsize = strlen(datastring);
  /*  fprintf(stderr, "doing insert of [%s] with key [%s] into [%s]\n", 
	  data.dptr, 
	  keystring, 
	  db->name);*/

  tdb_store(db, key, data, flags);
}

void make_key(char *buf, int buflen, int key_part1, char *key_part2)
{
    memset(buf, 0, buflen);
    if(key_part2 != NULL)
	sprintf(buf, "%d%s", key_part1, key_part2);
    else
	sprintf(buf, "%d", key_part1);

    return;
}

void usage(char *progname)
{
    fprintf(stderr, "Usage: %s [-d] [-f <file_path>].\n", progname);
    fprintf(stderr, "\t-d: run as a daemon.\n");
    fprintf(stderr, "\t-f <file_path>: path where the TDB files reside.\n");
    fprintf(stderr, "\t\tDEFAULT is /var/lib/samba/perfmon\n");
    exit(1);
}

void parse_flags(RuntimeSettings *rt, int argc, char **argv)
{
    int flag;
    
    while((flag = getopt(argc, argv, "df:")) != -1)
    {
	switch(flag)
	{
	    case 'd':
	    {
		rt->dflag = TRUE;
		break;
	    }
	    case 'f':
	    {
		memcpy(rt->dbDir, optarg, strlen(optarg));
		break;
	    }
	    default:
	    {
		usage(argv[0]);
	    }
	}
    }

    return;
}

void setup_file_paths(RuntimeSettings *rt)
{
    int status;

    if(strlen(rt->dbDir) == 0)
    {
	/* No file path was passed in, use default */
	sprintf(rt->dbDir, "/var/lib/samba/perfmon");
    }

    sprintf(rt->nameFile, "%s/names.tdb", rt->dbDir);
    sprintf(rt->counterFile, "%s/data.tdb", rt->dbDir);

    mkdir(rt->dbDir, 0755);
    rt->cnames = tdb_open(rt->nameFile, 0, TDB_CLEAR_IF_FIRST, O_RDWR | O_CREAT, 0644);
    rt->cdata = tdb_open(rt->counterFile, 0, TDB_CLEAR_IF_FIRST, O_RDWR | O_CREAT, 0644);

    if(rt->cnames == NULL || rt->cdata == NULL)
    {
	perror("setup_file_paths");
	exit(1);
    }

    return;
}

void sigterm_handler()
{
    keep_running = FALSE;
    return;
}

void daemonize(RuntimeSettings *rt)
{
    pid_t pid;
    int i;
    int fd;

    /* Check if we're already a daemon */
    if(getppid() == 1)
	return;
    pid = fork();
    if(pid < 0)
	/* can't fork */
	exit(1);
    else if(pid > 0)
    {
	/* we're the parent */
	tdb_close(rt->cnames);
	tdb_close(rt->cdata);
	exit(0);
    }

    /* get a new session */
    if(setsid() == -1)
	exit(2);

    /* Change CWD */
    chdir("/");

    /* close file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    /* And reopen them as safe defaults */
    fd = open("/dev/null", O_RDONLY);
    if(fd != 0)
    {
	dup2(fd, 0);
	close(fd);
    }
    fd = open("/dev/null", O_WRONLY);
    if(fd != 1)
    {
	dup2(fd, 1);
	close(fd);
    }
    fd = open("/dev/null", O_WRONLY);
    if(fd != 2)
    {
	dup2(fd, 2);
	close(fd);
    }

    /* handle signals */
    signal(SIGINT, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    signal(SIGTERM, sigterm_handler);

    return;
}

int get_counter_id(PERF_DATA_BLOCK *data)
{
    data->counter_id += 2;
    data->num_counters++;

    return data->counter_id;
}

void init_perf_counter(PerfCounter *counter,
		       PerfCounter *parent,
		       unsigned int index,
		       char *name,
		       char *help,
		       int counter_type,
		       int record_type)
{
    counter->index = index;
    memcpy(counter->name, name, strlen(name));
    memcpy(counter->help, help, strlen(help));
    counter->counter_type = counter_type;
    counter->record_type = record_type;

    switch(record_type)
    {
	case PERF_OBJECT:
	    sprintf(counter->relationships, "p");
	    break;
	case PERF_COUNTER:
	    sprintf(counter->relationships, "c[%d]", parent->index);
	    break;
	case PERF_INSTANCE:
	    sprintf(counter->relationships, "i[%d]", parent->index);
	    break;
	default:
	    perror("init_perf_counter: unknown record type");
	    exit(1);
    }

    return;
}
