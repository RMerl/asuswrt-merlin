/* 
 *  Unix SMB/CIFS implementation.
 *  Performance Counter Daemon
 *
 *  Copyright (C) Marcin Krzysztof Porwit    2005
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __PERF_H__
#define __PERF_H__

#define _PUBLIC_

#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#endif

#if !defined(HAVE_BOOL)
#ifdef HAVE__Bool
#define bool _Bool
#else
typedef int bool;
#endif
#endif


#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <limits.h>
#include <tdb.h>
#include "librpc/gen_ndr/perfcount.h"
#include <sys/statfs.h>
#include <sys/times.h>
#include <sys/sysinfo.h>

#define NUM_COUNTERS 10

#define NAME_LEN 256
#define HELP_LEN 1024

#define PERF_OBJECT 0
#define PERF_INSTANCE 1
#define PERF_COUNTER 2

#define FALSE 0
#define TRUE !FALSE

#define PROC_BUF 256
#define LARGE_BUF 16384

typedef struct perf_counter
{
    int index;
    char name[NAME_LEN];
    char help[HELP_LEN];
    char relationships[NAME_LEN];
    unsigned int counter_type;
    int record_type;
} PerfCounter;

typedef struct mem_data
{
    unsigned int availPhysKb;
    unsigned int availSwapKb;
    unsigned int totalPhysKb;
    unsigned int totalSwapKb;
} MemData;

typedef struct mem_info
{
    PerfCounter memObjDesc;
    PerfCounter availPhysKb;
    PerfCounter availSwapKb;
    PerfCounter totalPhysKb;
    PerfCounter totalSwapKb;
    MemData *data;
} MemInfo;

typedef struct cpu_data
{
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
} CPUData;

typedef struct cpu_info
{
    unsigned int numCPUs;
    PerfCounter cpuObjDesc;
    PerfCounter userCPU;
    PerfCounter niceCPU;
    PerfCounter systemCPU;
    PerfCounter idleCPU;
    CPUData *data;
} CPUInfo;

typedef struct disk_meta_data
{
	char name[NAME_LEN];
	char mountpoint[NAME_LEN];
} DiskMetaData;

typedef struct disk_data
{
	unsigned long long freeMegs;
	unsigned int writesPerSec;
	unsigned int readsPerSec;
} DiskData;

typedef struct disk_info
{
	unsigned int numDisks;
	DiskMetaData *mdata;
	PerfCounter diskObjDesc;
	PerfCounter freeMegs;
	PerfCounter writesPerSec;
	PerfCounter readsPerSec;
	DiskData *data;
} DiskInfo;

typedef struct process_data
{
	unsigned int runningProcessCount;
} ProcessData;

typedef struct process_info
{
	PerfCounter processObjDesc;
	PerfCounter runningProcessCount;
	ProcessData *data;
} ProcessInfo;

typedef struct perf_data_block
{
	unsigned int counter_id;
	unsigned int num_counters;
	unsigned int NumObjectTypes;
	unsigned long long PerfTime;
	unsigned long long PerfFreq;
	unsigned long long PerfTime100nSec;
	MemInfo memInfo;
	CPUInfo cpuInfo;
	ProcessInfo processInfo;
	DiskInfo diskInfo;
} PERF_DATA_BLOCK;

typedef struct runtime_settings
{
    /* Runtime flags */
    int dflag;
    /* DB path names */
    char dbDir[PATH_MAX];
    char nameFile[PATH_MAX];
    char counterFile[PATH_MAX];
    /* TDB context */
    TDB_CONTEXT *cnames;
    TDB_CONTEXT *cdata;
} RuntimeSettings;

/* perf_writer_ng_util.c function prototypes */
void fatal(char *msg);
void add_key(TDB_CONTEXT *db, char *keystring, char *datastring, int flags);
void add_key_raw(TDB_CONTEXT *db, char *keystring, void *datastring, size_t datasize, int flags);
void make_key(char *buf, int buflen, int key_part1, char *key_part2);
void parse_flags(RuntimeSettings *rt, int argc, char **argv);
void setup_file_paths(RuntimeSettings *rt);
void daemonize(RuntimeSettings *rt);

/* perf_writer_ng_mem.c function prototypes */
void get_meminfo(PERF_DATA_BLOCK *data);
void init_memdata_desc(PERF_DATA_BLOCK *data);
void init_memdata(PERF_DATA_BLOCK *data);
void output_mem_desc(PERF_DATA_BLOCK *data, RuntimeSettings rt);
void output_meminfo(PERF_DATA_BLOCK *data, RuntimeSettings rt, int tdb_flags);
void init_perf_counter(PerfCounter *counter, PerfCounter *parent, unsigned int index, char *name, char *help, int counter_type, int record_type);

/* perf_writer_ng_cpu.c function prototypes */
unsigned long long get_cpufreq();
void init_cpudata_desc(PERF_DATA_BLOCK *data);
void get_cpuinfo(PERF_DATA_BLOCK *data);
void init_cpu_data(PERF_DATA_BLOCK *data);
void output_cpu_desc(PERF_DATA_BLOCK *data, RuntimeSettings rt);
void output_cpuinfo(PERF_DATA_BLOCK *data, RuntimeSettings rt, int tdb_flags);

#endif /* __PERF_H__ */
