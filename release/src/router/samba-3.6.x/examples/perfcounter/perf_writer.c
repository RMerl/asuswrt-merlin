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

#include "perf.h"

sig_atomic_t keep_running = TRUE;

/* allocates memory and gets numCPUs, total memory, and PerfFreq, number of disks... */
void get_constants(PERF_DATA_BLOCK *data)
{
    data->cpuInfo.numCPUs = sysconf(_SC_NPROCESSORS_ONLN) > 0 ? sysconf(_SC_NPROCESSORS_ONLN) : 1;
    data->PerfFreq = sysconf(_SC_CLK_TCK);
    init_mem_data(data);
    init_cpu_data(data);
    init_process_data(data);
    init_disk_data(data);

    return;
}

void output_num_instances(PerfCounter obj, int numInst, RuntimeSettings rt)
{
    char key[NAME_LEN];
    char sdata[NAME_LEN];

    make_key(key, NAME_LEN, obj.index, "inst");
    memset(sdata, 0, NAME_LEN);
    sprintf(sdata, "%d", numInst);
    add_key(rt.cnames, key, sdata, TDB_INSERT);

    return;
}

void output_perf_desc(PerfCounter counter, RuntimeSettings rt)
{
    char key[NAME_LEN];
    char sdata[NAME_LEN];

    /* First insert the counter name */
    make_key(key, NAME_LEN, counter.index, NULL);
    add_key(rt.cnames, key, counter.name, TDB_INSERT);
    /* Add the help string */
    make_key(key, NAME_LEN, counter.index + 1, NULL);
    add_key(rt.cnames, key, counter.help, TDB_INSERT);
    /* Add the relationships */
    make_key(key, NAME_LEN, counter.index, "rel");
    add_key(rt.cnames, key, counter.relationships, TDB_INSERT);
    /* Add type data if not PERF_OBJECT or PERF_INSTANCE */
    if(counter.record_type == PERF_COUNTER)
    {
	make_key(key, NAME_LEN, counter.index, "type");
	memset(sdata, 0, NAME_LEN);
	sprintf(sdata, "%d", counter.counter_type);
	add_key(rt.cnames, key, sdata, TDB_INSERT);
    }

    return;
}

void initialize(PERF_DATA_BLOCK *data, RuntimeSettings *rt, int argc, char **argv)
{
    memset(data, 0, sizeof(*data));
    memset(rt, 0, sizeof(*data));

    parse_flags(rt, argc, argv);
    setup_file_paths(rt);

    get_constants(data);

    if(rt->dflag == TRUE)
	daemonize(rt);

    output_mem_desc(data, *rt);
    output_cpu_desc(data, *rt);
    output_process_desc(data, *rt);
    output_disk_desc(data, *rt);

    return;
}

void refresh_perf_data_block(PERF_DATA_BLOCK *data, RuntimeSettings rt)
{
    data->PerfTime100nSec = 0;
    get_meminfo(data);
    get_cpuinfo(data);
    get_processinfo(data);
    get_diskinfo(data);
    return;
}

void output_perf_counter(PerfCounter counter, unsigned long long data,
			 RuntimeSettings rt, int tdb_flags)
{
    char key[NAME_LEN];
    char sdata[NAME_LEN];
    unsigned int size_mask;

    make_key(key, NAME_LEN, counter.index, NULL);
    memset(sdata, 0, NAME_LEN);

    size_mask = counter.counter_type & PERF_SIZE_VARIABLE_LEN;

    if(size_mask == PERF_SIZE_DWORD)
	sprintf(sdata, "%d", (unsigned int)data);
    else if(size_mask == PERF_SIZE_LARGE)
	sprintf(sdata, "%Lu", data);

    add_key(rt.cdata, key, sdata, tdb_flags);

    return;
}

void output_perf_instance(int parentObjInd,
			  int instanceInd,
			  void *instData,
			  size_t dsize,
			  char *name,
			  RuntimeSettings rt,
			  int tdb_flags)
{
    char key[NAME_LEN];
    char sdata[NAME_LEN];

    memset(key, 0, NAME_LEN);
    sprintf(key, "%di%d", parentObjInd, instanceInd);
    add_key_raw(rt.cdata, key, instData, dsize, tdb_flags);

    /* encode name */
    memset(key, 0, NAME_LEN);
    sprintf(key, "%di%dname", parentObjInd, instanceInd);
    add_key(rt.cnames, key, name, tdb_flags);

    return;
}

void output_global_data(PERF_DATA_BLOCK *data, RuntimeSettings rt, int tdb_flags)
{
    int i;
    char key[NAME_LEN];
    char sdata[NAME_LEN];
    
     /* Initialize BaseIndex */
    make_key(key, NAME_LEN, 1, NULL);
    memset(sdata, 0, NAME_LEN);
    sprintf(sdata, "%d", data->num_counters);
    add_key(rt.cnames, key, sdata, tdb_flags);
    /* Initialize PerfTime, PerfFreq and PerfTime100nSec */
    memset(sdata, 0, NAME_LEN);
    make_key(key, NAME_LEN, 0, "PerfTime");
    sprintf(sdata, "%Lu", data->PerfTime);
    add_key(rt.cdata, key, sdata, tdb_flags);
    make_key(key, NAME_LEN, 0, "PerfTime100nSec");
    memset(sdata, 0, NAME_LEN);
    sprintf(sdata, "%Lu", data->PerfTime100nSec);
    add_key(rt.cdata, key, sdata, tdb_flags);
    memset(sdata, 0, NAME_LEN);
    make_key(key, NAME_LEN, 0, "PerfFreq");
    sprintf(sdata, "%Lu", data->PerfFreq);
    add_key(rt.cnames, key, sdata, tdb_flags);

    return;
}

void output_perf_data_block(PERF_DATA_BLOCK *data, RuntimeSettings rt, int tdb_flags)
{
    output_global_data(data, rt, tdb_flags);
    output_meminfo(data, rt, tdb_flags);
    output_cpuinfo(data, rt, tdb_flags);
    output_processinfo(data, rt, tdb_flags);
    output_diskinfo(data, rt, tdb_flags);
    return;
}

void update_counters(PERF_DATA_BLOCK *data, RuntimeSettings rt)
{
    refresh_perf_data_block(data, rt);
    output_perf_data_block(data, rt, TDB_REPLACE);

    return;
}

int main(int argc, char **argv)
{
    PERF_DATA_BLOCK data;
    RuntimeSettings rt;

    initialize(&data, &rt, argc, argv);

    while(keep_running)
    {
	update_counters(&data, rt);
	sleep(1);
    }
    
    return 0;
}
