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

void init_cpudata_desc(PERF_DATA_BLOCK *data)
{
    init_perf_counter(&(data->cpuInfo.cpuObjDesc),
		      &(data->cpuInfo.cpuObjDesc),
		      get_counter_id(data),
		      "Processor",
		      "The Processor object consists of counters that describe the behavior of the CPU.",
		      0,
		      PERF_OBJECT);
    init_perf_counter(&(data->cpuInfo.userCPU),
		      &(data->cpuInfo.cpuObjDesc),
		      get_counter_id(data),
		      "\% User CPU Utilization",
		      "\% User CPU Utilization is the percentage of the CPU used by  processes executing user code.",
		      PERF_SIZE_LARGE | PERF_TYPE_COUNTER | PERF_COUNTER_RATE | PERF_TIMER_100NS | PERF_DELTA_COUNTER | PERF_DISPLAY_PERCENT,
		      PERF_COUNTER);
    init_perf_counter(&(data->cpuInfo.systemCPU),
		      &(data->cpuInfo.cpuObjDesc),
		      get_counter_id(data),
		      "\% System CPU Utilization",
		      "\% System CPU Utilization is the percentage of the CPU used by processes doing system calls.",
		      PERF_SIZE_LARGE | PERF_TYPE_COUNTER | PERF_COUNTER_RATE | PERF_TIMER_100NS | PERF_DELTA_COUNTER | PERF_DISPLAY_PERCENT,
		      PERF_COUNTER);
    init_perf_counter(&(data->cpuInfo.niceCPU),
		      &(data->cpuInfo.cpuObjDesc),
		      get_counter_id(data),
		      "\% Nice CPU Utilization",
		      "\% Nice CPU Utilization is the percentage of the CPU used by processes running in nice mode.",
		      PERF_SIZE_LARGE | PERF_TYPE_COUNTER | PERF_COUNTER_RATE | PERF_TIMER_100NS | PERF_DELTA_COUNTER | PERF_DISPLAY_NOSHOW,
		      PERF_COUNTER);
    init_perf_counter(&(data->cpuInfo.idleCPU),
		      &(data->cpuInfo.cpuObjDesc),
		      get_counter_id(data),
		      "\% Idle CPU",
		      "\% Idle CPU is the percentage of the CPU not doing any work.",
		      PERF_SIZE_LARGE | PERF_TYPE_COUNTER | PERF_COUNTER_RATE | PERF_TIMER_100NS | PERF_DELTA_COUNTER | PERF_DISPLAY_NOSHOW,
		      PERF_COUNTER);

    return;
}

void get_cpuinfo(PERF_DATA_BLOCK *data)
{
    int num, i;
    unsigned int cpuid;
    char buf[PROC_BUF];
    static FILE *fp = NULL;

    if(!fp)
    {
	if(!(fp = fopen("/proc/stat", "r")))
	{
	    perror("get_cpuinfo: fopen");
	    exit(1);
	}
    }

    rewind(fp);
    fflush(fp);

    /* Read in the first line and discard it -- that has the CPU summary */
    if(!fgets(buf, sizeof(buf), fp))
    {
	perror("get_cpuinfo: fgets");
	exit(1);
    }
    for(i = 0; i < data->cpuInfo.numCPUs; i++)
    {
	if(!fgets(buf, sizeof(buf), fp))
	{
	    perror("get_cpuinfo: fgets");
	    exit(1);
	}
	num = sscanf(buf, "cpu%u %Lu %Lu %Lu %Lu",
		     &cpuid,
		     &data->cpuInfo.data[i].user,
		     &data->cpuInfo.data[i].nice,
		     &data->cpuInfo.data[i].system,
		     &data->cpuInfo.data[i].idle);
	if(i != cpuid)
	{
	    perror("get_cpuinfo: /proc/stat inconsistent?");
	    exit(1);
	}
	/*
	  Alternate way of doing things:
	  struct tms buffer;
	  data->PerfTime100nSec = times(&buffer);
	*/
	data->PerfTime100nSec += data->cpuInfo.data[i].user +
	    data->cpuInfo.data[i].nice +
	    data->cpuInfo.data[i].system +
	    data->cpuInfo.data[i].idle;
    }
    data->PerfTime100nSec /= data->cpuInfo.numCPUs;
    return;
}

void init_cpu_data(PERF_DATA_BLOCK *data)
{
    data->cpuInfo.data = calloc(data->cpuInfo.numCPUs, sizeof(*data->cpuInfo.data));
    if(!data->cpuInfo.data)
    {
	perror("init_cpu_data: out of memory");
	exit(1);
    }

    init_cpudata_desc(data);

    get_cpuinfo(data);

    return;
}   

void output_cpu_desc(PERF_DATA_BLOCK *data, RuntimeSettings rt)
{
    output_perf_desc(data->cpuInfo.cpuObjDesc, rt);
    output_perf_desc(data->cpuInfo.userCPU, rt);
    output_perf_desc(data->cpuInfo.niceCPU, rt);
    output_perf_desc(data->cpuInfo.systemCPU, rt);
    output_perf_desc(data->cpuInfo.idleCPU, rt);
    if(data->cpuInfo.numCPUs > 1)
	    output_num_instances(data->cpuInfo.cpuObjDesc, data->cpuInfo.numCPUs + 1, rt);

    return;
}

void output_cpuinfo(PERF_DATA_BLOCK *data, RuntimeSettings rt, int tdb_flags)
{
    int i;
    char buf[NAME_LEN];

    output_perf_counter(data->cpuInfo.userCPU,
			data->cpuInfo.data[0].user,
			rt, tdb_flags);
    output_perf_counter(data->cpuInfo.systemCPU,
			data->cpuInfo.data[0].system,
			rt, tdb_flags);
    output_perf_counter(data->cpuInfo.niceCPU,
			data->cpuInfo.data[0].nice,
			rt, tdb_flags);
    output_perf_counter(data->cpuInfo.idleCPU,
			data->cpuInfo.data[0].idle,
			rt, tdb_flags);
    if(data->cpuInfo.numCPUs > 1)
      {
	      for(i = 0; i < data->cpuInfo.numCPUs; i++)
	      {
		      memset(buf, 0, NAME_LEN);
		      sprintf(buf, "cpu%d", i);
		      output_perf_instance(data->cpuInfo.cpuObjDesc.index,
					   i,
					   (void *)&(data->cpuInfo.data[i]),
					   sizeof(data->cpuInfo.data[i]),
					   buf, rt, tdb_flags);
	      }
	      
	      memset(buf, 0, NAME_LEN);
	      sprintf(buf, "_Total");
	      output_perf_instance(data->cpuInfo.cpuObjDesc.index,
				   i,
				   (void *)&(data->cpuInfo.data[i]),
				   sizeof(data->cpuInfo.data[i]),
				   buf, rt, tdb_flags);
      }
    return;
}
