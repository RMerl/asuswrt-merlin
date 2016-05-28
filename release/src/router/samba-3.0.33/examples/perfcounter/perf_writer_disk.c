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

void init_diskdata_desc(PERF_DATA_BLOCK *data)
{
	init_perf_counter(&(data->diskInfo.diskObjDesc),
			  &(data->diskInfo.diskObjDesc),
			  get_counter_id(data),
			  "Logical Disk",
			  "The Logical Disk object consists of counters that show information about disks.",
			  0,
			  PERF_OBJECT);
	init_perf_counter(&(data->diskInfo.freeMegs),
			  &(data->diskInfo.diskObjDesc),
			  get_counter_id(data),
			  "Megabytes Free",
			  "The amount of available disk space, in megabytes.",
			  PERF_SIZE_LARGE | PERF_TYPE_NUMBER | PERF_NUMBER_DECIMAL | PERF_DISPLAY_NO_SUFFIX,
			  PERF_COUNTER);
	init_perf_counter(&(data->diskInfo.writesPerSec),
			  &(data->diskInfo.diskObjDesc),
			  get_counter_id(data),
			  "Writes/sec",
			  "The number of writes per second to that disk.",
			  PERF_SIZE_DWORD | PERF_TYPE_COUNTER | PERF_COUNTER_RATE | PERF_DELTA_COUNTER | PERF_DISPLAY_PER_SEC,
			  PERF_COUNTER);
	init_perf_counter(&(data->diskInfo.readsPerSec),
			  &(data->diskInfo.diskObjDesc),
			  get_counter_id(data),
			  "Reads/sec",
			  "The number of reads of that disk per second.",
			  PERF_SIZE_DWORD | PERF_TYPE_COUNTER | PERF_COUNTER_RATE | PERF_DELTA_COUNTER | PERF_DISPLAY_PER_SEC,
			  PERF_COUNTER);

	return;
}
void init_num_disks(PERF_DATA_BLOCK *data)
{
	FILE *mtab;
	char buf[PROC_BUF];
	char *start, *stop;
	int i = 0, num;

	if(!(mtab = fopen("/etc/mtab", "r")))
	{
		perror("init_disk_names: fopen");
		exit(1);
	}

	rewind(mtab);
	fflush(mtab);

	while(fgets(buf, sizeof(buf), mtab))
	{
		if(start = strstr(buf, "/dev/"))
		{
			if(start = strstr(start, "da"))
			{
				i++;
			}
		}
	}

	data->diskInfo.numDisks = i;
	fclose(mtab);

	return;
}
	
void init_disk_names(PERF_DATA_BLOCK *data)
{
	FILE *mtab;
	char buf[PROC_BUF];
	char *start, *stop;
	int i = 0, num;

	if(!(mtab = fopen("/etc/mtab", "r")))
	{
		perror("init_disk_names: fopen");
		exit(1);
	}

	rewind(mtab);
	fflush(mtab);

	while(fgets(buf, sizeof(buf), mtab))
	{
		if(start = strstr(buf, "/dev/"))
		{
			if(start = strstr(start, "da"))
			{
				start -=1;
				stop = strstr(start, " ");
				memcpy(data->diskInfo.mdata[i].name, start, stop - start);
				start = stop +1;
				stop = strstr(start, " ");
				memcpy(data->diskInfo.mdata[i].mountpoint, start, stop - start);
				i++;
			}
		}
	}

	fclose(mtab);

	return;
}

void get_diskinfo(PERF_DATA_BLOCK *data)
{
	int i;
	DiskData *p;
	struct statfs statfsbuf;
	int status, num;
	char buf[LARGE_BUF], *start;
	FILE *diskstats;
	long reads, writes, discard;

	diskstats = fopen("/proc/diskstats", "r");
	rewind(diskstats);
	fflush(diskstats);
	status = fread(buf, sizeof(char), LARGE_BUF, diskstats);
	fclose(diskstats);

	for(i = 0; i < data->diskInfo.numDisks; i++)
	{
		p = &(data->diskInfo.data[i]);
		status = statfs(data->diskInfo.mdata[i].mountpoint, &statfsbuf);
		p->freeMegs = (statfsbuf.f_bfree*statfsbuf.f_bsize)/1048576;
		start = strstr(buf, data->diskInfo.mdata[i].name);
		start += strlen(data->diskInfo.mdata[i].name) + 1;
		num = sscanf(start, "%u %u %u %u",
			     &reads,
			     &discard, 
			     &writes, 
			     &discard);
		p->writesPerSec = writes;
		p->readsPerSec = reads;
		fprintf(stderr, "%s:\t%u\t%u\n",
			data->diskInfo.mdata[i].mountpoint,
			reads, writes);
	}
	return;
}
void init_disk_data(PERF_DATA_BLOCK *data)
{
	init_diskdata_desc(data);
	
	init_num_disks(data);

	data->diskInfo.mdata = calloc(data->diskInfo.numDisks, sizeof(DiskMetaData));
	if(!data->diskInfo.mdata)
	{
		fatal("init_disk_data: out of memory");
	}

	init_disk_names(data);

	data->diskInfo.data = calloc(data->diskInfo.numDisks, sizeof(DiskData));
	if(!data->diskInfo.data)
	{
		fatal("init_disk_data: out of memory");
	}

	get_diskinfo(data);

	return;
}

void output_disk_desc(PERF_DATA_BLOCK *data, RuntimeSettings rt)
{
	output_perf_desc(data->diskInfo.diskObjDesc, rt);
	output_perf_desc(data->diskInfo.freeMegs, rt);
	output_perf_desc(data->diskInfo.writesPerSec, rt);
	output_perf_desc(data->diskInfo.readsPerSec, rt);
	output_num_instances(data->diskInfo.diskObjDesc, data->diskInfo.numDisks, rt);

	return;
}

void output_diskinfo(PERF_DATA_BLOCK *data, RuntimeSettings rt, int tdb_flags)
{
	int i;
	
	output_perf_counter(data->diskInfo.freeMegs,
			    data->diskInfo.data[0].freeMegs,
			    rt, tdb_flags);
	output_perf_counter(data->diskInfo.writesPerSec,
			    (unsigned long long)data->diskInfo.data[0].writesPerSec,
			    rt, tdb_flags);
	output_perf_counter(data->diskInfo.readsPerSec,
			    (unsigned long long)data->diskInfo.data[0].readsPerSec,
			    rt, tdb_flags);

	for(i = 0; i < data->diskInfo.numDisks; i++)
	{
		output_perf_instance(data->diskInfo.diskObjDesc.index,
				     i,
				     (void *)&(data->diskInfo.data[i]),
				     sizeof(DiskData),
				     data->diskInfo.mdata[i].mountpoint,
				     rt, tdb_flags);
	}

	return;
}
