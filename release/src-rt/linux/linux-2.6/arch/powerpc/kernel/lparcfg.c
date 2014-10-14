/*
 * PowerPC64 LPAR Configuration Information Driver
 *
 * Dave Engebretsen engebret@us.ibm.com
 *    Copyright (c) 2003 Dave Engebretsen
 * Will Schmidt willschm@us.ibm.com
 *    SPLPAR updates, Copyright (c) 2003 Will Schmidt IBM Corporation.
 *    seq_file updates, Copyright (c) 2004 Will Schmidt IBM Corporation.
 * Nathan Lynch nathanl@austin.ibm.com
 *    Added lparcfg_write, Copyright (C) 2004 Nathan Lynch IBM Corporation.
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 * This driver creates a proc file at /proc/ppc64/lparcfg which contains
 * keyword - value pairs that specify the configuration of the partition.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/iseries/hv_lp_config.h>
#include <asm/lppaca.h>
#include <asm/hvcall.h>
#include <asm/firmware.h>
#include <asm/rtas.h>
#include <asm/system.h>
#include <asm/time.h>
#include <asm/prom.h>
#include <asm/vdso_datapage.h>
#include <asm/vio.h>
#include <asm/mmu.h>

#define MODULE_VERS "1.9"
#define MODULE_NAME "lparcfg"

/* #define LPARCFG_DEBUG */

static struct proc_dir_entry *proc_ppc64_lparcfg;

/*
 * Track sum of all purrs across all processors. This is used to further
 * calculate usage values by different applications
 */
static unsigned long get_purr(void)
{
	unsigned long sum_purr = 0;
	int cpu;

	for_each_possible_cpu(cpu) {
		if (firmware_has_feature(FW_FEATURE_ISERIES))
			sum_purr += lppaca_of(cpu).emulated_time_base;
		else {
			struct cpu_usage *cu;

			cu = &per_cpu(cpu_usage_array, cpu);
			sum_purr += cu->current_tb;
		}
	}
	return sum_purr;
}

#ifdef CONFIG_PPC_ISERIES

/*
 * Methods used to fetch LPAR data when running on an iSeries platform.
 */
static int iseries_lparcfg_data(struct seq_file *m, void *v)
{
	unsigned long pool_id;
	int shared, entitled_capacity, max_entitled_capacity;
	int processors, max_processors;
	unsigned long purr = get_purr();

	shared = (int)(local_paca->lppaca_ptr->shared_proc);

	seq_printf(m, "system_active_processors=%d\n",
		   (int)HvLpConfig_getSystemPhysicalProcessors());

	seq_printf(m, "system_potential_processors=%d\n",
		   (int)HvLpConfig_getSystemPhysicalProcessors());

	processors = (int)HvLpConfig_getPhysicalProcessors();
	seq_printf(m, "partition_active_processors=%d\n", processors);

	max_processors = (int)HvLpConfig_getMaxPhysicalProcessors();
	seq_printf(m, "partition_potential_processors=%d\n", max_processors);

	if (shared) {
		entitled_capacity = HvLpConfig_getSharedProcUnits();
		max_entitled_capacity = HvLpConfig_getMaxSharedProcUnits();
	} else {
		entitled_capacity = processors * 100;
		max_entitled_capacity = max_processors * 100;
	}
	seq_printf(m, "partition_entitled_capacity=%d\n", entitled_capacity);

	seq_printf(m, "partition_max_entitled_capacity=%d\n",
		   max_entitled_capacity);

	if (shared) {
		pool_id = HvLpConfig_getSharedPoolIndex();
		seq_printf(m, "pool=%d\n", (int)pool_id);
		seq_printf(m, "pool_capacity=%d\n",
			   (int)(HvLpConfig_getNumProcsInSharedPool(pool_id) *
				 100));
		seq_printf(m, "purr=%ld\n", purr);
	}

	seq_printf(m, "shared_processor_mode=%d\n", shared);

	return 0;
}

#else				/* CONFIG_PPC_ISERIES */

static int iseries_lparcfg_data(struct seq_file *m, void *v)
{
	return 0;
}

#endif				/* CONFIG_PPC_ISERIES */

#ifdef CONFIG_PPC_PSERIES
/*
 * Methods used to fetch LPAR data when running on a pSeries platform.
 */
/**
 * h_get_mpp
 * H_GET_MPP hcall returns info in 7 parms
 */
int h_get_mpp(struct hvcall_mpp_data *mpp_data)
{
	int rc;
	unsigned long retbuf[PLPAR_HCALL9_BUFSIZE];

	rc = plpar_hcall9(H_GET_MPP, retbuf);

	mpp_data->entitled_mem = retbuf[0];
	mpp_data->mapped_mem = retbuf[1];

	mpp_data->group_num = (retbuf[2] >> 2 * 8) & 0xffff;
	mpp_data->pool_num = retbuf[2] & 0xffff;

	mpp_data->mem_weight = (retbuf[3] >> 7 * 8) & 0xff;
	mpp_data->unallocated_mem_weight = (retbuf[3] >> 6 * 8) & 0xff;
	mpp_data->unallocated_entitlement = retbuf[3] & 0xffffffffffff;

	mpp_data->pool_size = retbuf[4];
	mpp_data->loan_request = retbuf[5];
	mpp_data->backing_mem = retbuf[6];

	return rc;
}
EXPORT_SYMBOL(h_get_mpp);

struct hvcall_ppp_data {
	u64	entitlement;
	u64	unallocated_entitlement;
	u16	group_num;
	u16	pool_num;
	u8	capped;
	u8	weight;
	u8	unallocated_weight;
	u16	active_procs_in_pool;
	u16	active_system_procs;
	u16	phys_platform_procs;
	u32	max_proc_cap_avail;
	u32	entitled_proc_cap_avail;
};

/*
 * H_GET_PPP hcall returns info in 4 parms.
 *  entitled_capacity,unallocated_capacity,
 *  aggregation, resource_capability).
 *
 *  R4 = Entitled Processor Capacity Percentage.
 *  R5 = Unallocated Processor Capacity Percentage.
 *  R6 (AABBCCDDEEFFGGHH).
 *      XXXX - reserved (0)
 *          XXXX - reserved (0)
 *              XXXX - Group Number
 *                  XXXX - Pool Number.
 *  R7 (IIJJKKLLMMNNOOPP).
 *      XX - reserved. (0)
 *        XX - bit 0-6 reserved (0).   bit 7 is Capped indicator.
 *          XX - variable processor Capacity Weight
 *            XX - Unallocated Variable Processor Capacity Weight.
 *              XXXX - Active processors in Physical Processor Pool.
 *                  XXXX  - Processors active on platform.
 *  R8 (QQQQRRRRRRSSSSSS). if ibm,partition-performance-parameters-level >= 1
 *	XXXX - Physical platform procs allocated to virtualization.
 *	    XXXXXX - Max procs capacity % available to the partitions pool.
 *	          XXXXXX - Entitled procs capacity % available to the
 *			   partitions pool.
 */
static unsigned int h_get_ppp(struct hvcall_ppp_data *ppp_data)
{
	unsigned long rc;
	unsigned long retbuf[PLPAR_HCALL9_BUFSIZE];

	rc = plpar_hcall9(H_GET_PPP, retbuf);

	ppp_data->entitlement = retbuf[0];
	ppp_data->unallocated_entitlement = retbuf[1];

	ppp_data->group_num = (retbuf[2] >> 2 * 8) & 0xffff;
	ppp_data->pool_num = retbuf[2] & 0xffff;

	ppp_data->capped = (retbuf[3] >> 6 * 8) & 0x01;
	ppp_data->weight = (retbuf[3] >> 5 * 8) & 0xff;
	ppp_data->unallocated_weight = (retbuf[3] >> 4 * 8) & 0xff;
	ppp_data->active_procs_in_pool = (retbuf[3] >> 2 * 8) & 0xffff;
	ppp_data->active_system_procs = retbuf[3] & 0xffff;

	ppp_data->phys_platform_procs = retbuf[4] >> 6 * 8;
	ppp_data->max_proc_cap_avail = (retbuf[4] >> 3 * 8) & 0xffffff;
	ppp_data->entitled_proc_cap_avail = retbuf[4] & 0xffffff;

	return rc;
}

static unsigned h_pic(unsigned long *pool_idle_time,
		      unsigned long *num_procs)
{
	unsigned long rc;
	unsigned long retbuf[PLPAR_HCALL_BUFSIZE];

	rc = plpar_hcall(H_PIC, retbuf);

	*pool_idle_time = retbuf[0];
	*num_procs = retbuf[1];

	return rc;
}

/*
 * parse_ppp_data
 * Parse out the data returned from h_get_ppp and h_pic
 */
static void parse_ppp_data(struct seq_file *m)
{
	struct hvcall_ppp_data ppp_data;
	struct device_node *root;
	const int *perf_level;
	int rc;

	rc = h_get_ppp(&ppp_data);
	if (rc)
		return;

	seq_printf(m, "partition_entitled_capacity=%lld\n",
	           ppp_data.entitlement);
	seq_printf(m, "group=%d\n", ppp_data.group_num);
	seq_printf(m, "system_active_processors=%d\n",
	           ppp_data.active_system_procs);

	/* pool related entries are appropriate for shared configs */
	if (lppaca_of(0).shared_proc) {
		unsigned long pool_idle_time, pool_procs;

		seq_printf(m, "pool=%d\n", ppp_data.pool_num);

		/* report pool_capacity in percentage */
		seq_printf(m, "pool_capacity=%d\n",
			   ppp_data.active_procs_in_pool * 100);

		h_pic(&pool_idle_time, &pool_procs);
		seq_printf(m, "pool_idle_time=%ld\n", pool_idle_time);
		seq_printf(m, "pool_num_procs=%ld\n", pool_procs);
	}

	seq_printf(m, "unallocated_capacity_weight=%d\n",
		   ppp_data.unallocated_weight);
	seq_printf(m, "capacity_weight=%d\n", ppp_data.weight);
	seq_printf(m, "capped=%d\n", ppp_data.capped);
	seq_printf(m, "unallocated_capacity=%lld\n",
		   ppp_data.unallocated_entitlement);

	/* The last bits of information returned from h_get_ppp are only
	 * valid if the ibm,partition-performance-parameters-level
	 * property is >= 1.
	 */
	root = of_find_node_by_path("/");
	if (root) {
		perf_level = of_get_property(root,
				"ibm,partition-performance-parameters-level",
					     NULL);
		if (perf_level && (*perf_level >= 1)) {
			seq_printf(m,
			    "physical_procs_allocated_to_virtualization=%d\n",
				   ppp_data.phys_platform_procs);
			seq_printf(m, "max_proc_capacity_available=%d\n",
				   ppp_data.max_proc_cap_avail);
			seq_printf(m, "entitled_proc_capacity_available=%d\n",
				   ppp_data.entitled_proc_cap_avail);
		}

		of_node_put(root);
	}
}

/**
 * parse_mpp_data
 * Parse out data returned from h_get_mpp
 */
static void parse_mpp_data(struct seq_file *m)
{
	struct hvcall_mpp_data mpp_data;
	int rc;

	rc = h_get_mpp(&mpp_data);
	if (rc)
		return;

	seq_printf(m, "entitled_memory=%ld\n", mpp_data.entitled_mem);

	if (mpp_data.mapped_mem != -1)
		seq_printf(m, "mapped_entitled_memory=%ld\n",
		           mpp_data.mapped_mem);

	seq_printf(m, "entitled_memory_group_number=%d\n", mpp_data.group_num);
	seq_printf(m, "entitled_memory_pool_number=%d\n", mpp_data.pool_num);

	seq_printf(m, "entitled_memory_weight=%d\n", mpp_data.mem_weight);
	seq_printf(m, "unallocated_entitled_memory_weight=%d\n",
	           mpp_data.unallocated_mem_weight);
	seq_printf(m, "unallocated_io_mapping_entitlement=%ld\n",
	           mpp_data.unallocated_entitlement);

	if (mpp_data.pool_size != -1)
		seq_printf(m, "entitled_memory_pool_size=%ld bytes\n",
		           mpp_data.pool_size);

	seq_printf(m, "entitled_memory_loan_request=%ld\n",
	           mpp_data.loan_request);

	seq_printf(m, "backing_memory=%ld bytes\n", mpp_data.backing_mem);
}

#define SPLPAR_CHARACTERISTICS_TOKEN 20
#define SPLPAR_MAXLENGTH 1026*(sizeof(char))

/*
 * parse_system_parameter_string()
 * Retrieve the potential_processors, max_entitled_capacity and friends
 * through the get-system-parameter rtas call.  Replace keyword strings as
 * necessary.
 */
static void parse_system_parameter_string(struct seq_file *m)
{
	int call_status;

	unsigned char *local_buffer = kmalloc(SPLPAR_MAXLENGTH, GFP_KERNEL);
	if (!local_buffer) {
		printk(KERN_ERR "%s %s kmalloc failure at line %d\n",
		       __FILE__, __func__, __LINE__);
		return;
	}

	spin_lock(&rtas_data_buf_lock);
	memset(rtas_data_buf, 0, SPLPAR_MAXLENGTH);
	call_status = rtas_call(rtas_token("ibm,get-system-parameter"), 3, 1,
				NULL,
				SPLPAR_CHARACTERISTICS_TOKEN,
				__pa(rtas_data_buf),
				RTAS_DATA_BUF_SIZE);
	memcpy(local_buffer, rtas_data_buf, SPLPAR_MAXLENGTH);
	spin_unlock(&rtas_data_buf_lock);

	if (call_status != 0) {
		printk(KERN_INFO
		       "%s %s Error calling get-system-parameter (0x%x)\n",
		       __FILE__, __func__, call_status);
	} else {
		int splpar_strlen;
		int idx, w_idx;
		char *workbuffer = kzalloc(SPLPAR_MAXLENGTH, GFP_KERNEL);
		if (!workbuffer) {
			printk(KERN_ERR "%s %s kmalloc failure at line %d\n",
			       __FILE__, __func__, __LINE__);
			kfree(local_buffer);
			return;
		}
#ifdef LPARCFG_DEBUG
		printk(KERN_INFO "success calling get-system-parameter\n");
#endif
		splpar_strlen = local_buffer[0] * 256 + local_buffer[1];
		local_buffer += 2;	/* step over strlen value */

		w_idx = 0;
		idx = 0;
		while ((*local_buffer) && (idx < splpar_strlen)) {
			workbuffer[w_idx++] = local_buffer[idx++];
			if ((local_buffer[idx] == ',')
			    || (local_buffer[idx] == '\0')) {
				workbuffer[w_idx] = '\0';
				if (w_idx) {
					/* avoid the empty string */
					seq_printf(m, "%s\n", workbuffer);
				}
				memset(workbuffer, 0, SPLPAR_MAXLENGTH);
				idx++;	/* skip the comma */
				w_idx = 0;
			} else if (local_buffer[idx] == '=') {
				/* code here to replace workbuffer contents
				   with different keyword strings */
				if (0 == strcmp(workbuffer, "MaxEntCap")) {
					strcpy(workbuffer,
					       "partition_max_entitled_capacity");
					w_idx = strlen(workbuffer);
				}
				if (0 == strcmp(workbuffer, "MaxPlatProcs")) {
					strcpy(workbuffer,
					       "system_potential_processors");
					w_idx = strlen(workbuffer);
				}
			}
		}
		kfree(workbuffer);
		local_buffer -= 2;	/* back up over strlen value */
	}
	kfree(local_buffer);
}

/* Return the number of processors in the system.
 * This function reads through the device tree and counts
 * the virtual processors, this does not include threads.
 */
static int lparcfg_count_active_processors(void)
{
	struct device_node *cpus_dn = NULL;
	int count = 0;

	while ((cpus_dn = of_find_node_by_type(cpus_dn, "cpu"))) {
#ifdef LPARCFG_DEBUG
		printk(KERN_ERR "cpus_dn %p\n", cpus_dn);
#endif
		count++;
	}
	return count;
}

static void pseries_cmo_data(struct seq_file *m)
{
	int cpu;
	unsigned long cmo_faults = 0;
	unsigned long cmo_fault_time = 0;

	seq_printf(m, "cmo_enabled=%d\n", firmware_has_feature(FW_FEATURE_CMO));

	if (!firmware_has_feature(FW_FEATURE_CMO))
		return;

	for_each_possible_cpu(cpu) {
		cmo_faults += lppaca_of(cpu).cmo_faults;
		cmo_fault_time += lppaca_of(cpu).cmo_fault_time;
	}

	seq_printf(m, "cmo_faults=%lu\n", cmo_faults);
	seq_printf(m, "cmo_fault_time_usec=%lu\n",
		   cmo_fault_time / tb_ticks_per_usec);
	seq_printf(m, "cmo_primary_psp=%d\n", cmo_get_primary_psp());
	seq_printf(m, "cmo_secondary_psp=%d\n", cmo_get_secondary_psp());
	seq_printf(m, "cmo_page_size=%lu\n", cmo_get_page_size());
}

static void splpar_dispatch_data(struct seq_file *m)
{
	int cpu;
	unsigned long dispatches = 0;
	unsigned long dispatch_dispersions = 0;

	for_each_possible_cpu(cpu) {
		dispatches += lppaca_of(cpu).yield_count;
		dispatch_dispersions += lppaca_of(cpu).dispersion_count;
	}

	seq_printf(m, "dispatches=%lu\n", dispatches);
	seq_printf(m, "dispatch_dispersions=%lu\n", dispatch_dispersions);
}

static void parse_em_data(struct seq_file *m)
{
	unsigned long retbuf[PLPAR_HCALL_BUFSIZE];

	if (plpar_hcall(H_GET_EM_PARMS, retbuf) == H_SUCCESS)
		seq_printf(m, "power_mode_data=%016lx\n", retbuf[0]);
}

static int pseries_lparcfg_data(struct seq_file *m, void *v)
{
	int partition_potential_processors;
	int partition_active_processors;
	struct device_node *rtas_node;
	const int *lrdrp = NULL;

	rtas_node = of_find_node_by_path("/rtas");
	if (rtas_node)
		lrdrp = of_get_property(rtas_node, "ibm,lrdr-capacity", NULL);

	if (lrdrp == NULL) {
		partition_potential_processors = vdso_data->processorCount;
	} else {
		partition_potential_processors = *(lrdrp + 4);
	}
	of_node_put(rtas_node);

	partition_active_processors = lparcfg_count_active_processors();

	if (firmware_has_feature(FW_FEATURE_SPLPAR)) {
		/* this call handles the ibm,get-system-parameter contents */
		parse_system_parameter_string(m);
		parse_ppp_data(m);
		parse_mpp_data(m);
		pseries_cmo_data(m);
		splpar_dispatch_data(m);

		seq_printf(m, "purr=%ld\n", get_purr());
	} else {		/* non SPLPAR case */

		seq_printf(m, "system_active_processors=%d\n",
			   partition_potential_processors);

		seq_printf(m, "system_potential_processors=%d\n",
			   partition_potential_processors);

		seq_printf(m, "partition_max_entitled_capacity=%d\n",
			   partition_potential_processors * 100);

		seq_printf(m, "partition_entitled_capacity=%d\n",
			   partition_active_processors * 100);
	}

	seq_printf(m, "partition_active_processors=%d\n",
		   partition_active_processors);

	seq_printf(m, "partition_potential_processors=%d\n",
		   partition_potential_processors);

	seq_printf(m, "shared_processor_mode=%d\n", lppaca_of(0).shared_proc);

	seq_printf(m, "slb_size=%d\n", mmu_slb_size);

	parse_em_data(m);

	return 0;
}

static ssize_t update_ppp(u64 *entitlement, u8 *weight)
{
	struct hvcall_ppp_data ppp_data;
	u8 new_weight;
	u64 new_entitled;
	ssize_t retval;

	/* Get our current parameters */
	retval = h_get_ppp(&ppp_data);
	if (retval)
		return retval;

	if (entitlement) {
		new_weight = ppp_data.weight;
		new_entitled = *entitlement;
	} else if (weight) {
		new_weight = *weight;
		new_entitled = ppp_data.entitlement;
	} else
		return -EINVAL;

	pr_debug("%s: current_entitled = %llu, current_weight = %u\n",
		 __func__, ppp_data.entitlement, ppp_data.weight);

	pr_debug("%s: new_entitled = %llu, new_weight = %u\n",
		 __func__, new_entitled, new_weight);

	retval = plpar_hcall_norets(H_SET_PPP, new_entitled, new_weight);
	return retval;
}

/**
 * update_mpp
 *
 * Update the memory entitlement and weight for the partition.  Caller must
 * specify either a new entitlement or weight, not both, to be updated
 * since the h_set_mpp call takes both entitlement and weight as parameters.
 */
static ssize_t update_mpp(u64 *entitlement, u8 *weight)
{
	struct hvcall_mpp_data mpp_data;
	u64 new_entitled;
	u8 new_weight;
	ssize_t rc;

	if (entitlement) {
		/* Check with vio to ensure the new memory entitlement
		 * can be handled.
		 */
		rc = vio_cmo_entitlement_update(*entitlement);
		if (rc)
			return rc;
	}

	rc = h_get_mpp(&mpp_data);
	if (rc)
		return rc;

	if (entitlement) {
		new_weight = mpp_data.mem_weight;
		new_entitled = *entitlement;
	} else if (weight) {
		new_weight = *weight;
		new_entitled = mpp_data.entitled_mem;
	} else
		return -EINVAL;

	pr_debug("%s: current_entitled = %lu, current_weight = %u\n",
	         __func__, mpp_data.entitled_mem, mpp_data.mem_weight);

	pr_debug("%s: new_entitled = %llu, new_weight = %u\n",
		 __func__, new_entitled, new_weight);

	rc = plpar_hcall_norets(H_SET_MPP, new_entitled, new_weight);
	return rc;
}

/*
 * Interface for changing system parameters (variable capacity weight
 * and entitled capacity).  Format of input is "param_name=value";
 * anything after value is ignored.  Valid parameters at this time are
 * "partition_entitled_capacity" and "capacity_weight".  We use
 * H_SET_PPP to alter parameters.
 *
 * This function should be invoked only on systems with
 * FW_FEATURE_SPLPAR.
 */
static ssize_t lparcfg_write(struct file *file, const char __user * buf,
			     size_t count, loff_t * off)
{
	int kbuf_sz = 64;
	char kbuf[kbuf_sz];
	char *tmp;
	u64 new_entitled, *new_entitled_ptr = &new_entitled;
	u8 new_weight, *new_weight_ptr = &new_weight;
	ssize_t retval;

	if (!firmware_has_feature(FW_FEATURE_SPLPAR) ||
			firmware_has_feature(FW_FEATURE_ISERIES))
		return -EINVAL;

	if (count > kbuf_sz)
		return -EINVAL;

	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;

	kbuf[count - 1] = '\0';
	tmp = strchr(kbuf, '=');
	if (!tmp)
		return -EINVAL;

	*tmp++ = '\0';

	if (!strcmp(kbuf, "partition_entitled_capacity")) {
		char *endp;
		*new_entitled_ptr = (u64) simple_strtoul(tmp, &endp, 10);
		if (endp == tmp)
			return -EINVAL;

		retval = update_ppp(new_entitled_ptr, NULL);
	} else if (!strcmp(kbuf, "capacity_weight")) {
		char *endp;
		*new_weight_ptr = (u8) simple_strtoul(tmp, &endp, 10);
		if (endp == tmp)
			return -EINVAL;

		retval = update_ppp(NULL, new_weight_ptr);
	} else if (!strcmp(kbuf, "entitled_memory")) {
		char *endp;
		*new_entitled_ptr = (u64) simple_strtoul(tmp, &endp, 10);
		if (endp == tmp)
			return -EINVAL;

		retval = update_mpp(new_entitled_ptr, NULL);
	} else if (!strcmp(kbuf, "entitled_memory_weight")) {
		char *endp;
		*new_weight_ptr = (u8) simple_strtoul(tmp, &endp, 10);
		if (endp == tmp)
			return -EINVAL;

		retval = update_mpp(NULL, new_weight_ptr);
	} else
		return -EINVAL;

	if (retval == H_SUCCESS || retval == H_CONSTRAINED) {
		retval = count;
	} else if (retval == H_BUSY) {
		retval = -EBUSY;
	} else if (retval == H_HARDWARE) {
		retval = -EIO;
	} else if (retval == H_PARAMETER) {
		retval = -EINVAL;
	}

	return retval;
}

#else				/* CONFIG_PPC_PSERIES */

static int pseries_lparcfg_data(struct seq_file *m, void *v)
{
	return 0;
}

static ssize_t lparcfg_write(struct file *file, const char __user * buf,
			     size_t count, loff_t * off)
{
	return -EINVAL;
}

#endif				/* CONFIG_PPC_PSERIES */

static int lparcfg_data(struct seq_file *m, void *v)
{
	struct device_node *rootdn;
	const char *model = "";
	const char *system_id = "";
	const char *tmp;
	const unsigned int *lp_index_ptr;
	unsigned int lp_index = 0;

	seq_printf(m, "%s %s\n", MODULE_NAME, MODULE_VERS);

	rootdn = of_find_node_by_path("/");
	if (rootdn) {
		tmp = of_get_property(rootdn, "model", NULL);
		if (tmp) {
			model = tmp;
			/* Skip "IBM," - see platforms/iseries/dt.c */
			if (firmware_has_feature(FW_FEATURE_ISERIES))
				model += 4;
		}
		tmp = of_get_property(rootdn, "system-id", NULL);
		if (tmp) {
			system_id = tmp;
			/* Skip "IBM," - see platforms/iseries/dt.c */
			if (firmware_has_feature(FW_FEATURE_ISERIES))
				system_id += 4;
		}
		lp_index_ptr = of_get_property(rootdn, "ibm,partition-no",
					NULL);
		if (lp_index_ptr)
			lp_index = *lp_index_ptr;
		of_node_put(rootdn);
	}
	seq_printf(m, "serial_number=%s\n", system_id);
	seq_printf(m, "system_type=%s\n", model);
	seq_printf(m, "partition_id=%d\n", (int)lp_index);

	if (firmware_has_feature(FW_FEATURE_ISERIES))
		return iseries_lparcfg_data(m, v);
	return pseries_lparcfg_data(m, v);
}

static int lparcfg_open(struct inode *inode, struct file *file)
{
	return single_open(file, lparcfg_data, NULL);
}

static const struct file_operations lparcfg_fops = {
	.owner		= THIS_MODULE,
	.read		= seq_read,
	.write		= lparcfg_write,
	.open		= lparcfg_open,
	.release	= single_release,
	.llseek		= seq_lseek,
};

static int __init lparcfg_init(void)
{
	struct proc_dir_entry *ent;
	mode_t mode = S_IRUSR | S_IRGRP | S_IROTH;

	/* Allow writing if we have FW_FEATURE_SPLPAR */
	if (firmware_has_feature(FW_FEATURE_SPLPAR) &&
			!firmware_has_feature(FW_FEATURE_ISERIES))
		mode |= S_IWUSR;

	ent = proc_create("powerpc/lparcfg", mode, NULL, &lparcfg_fops);
	if (!ent) {
		printk(KERN_ERR "Failed to create powerpc/lparcfg\n");
		return -EIO;
	}

	proc_ppc64_lparcfg = ent;
	return 0;
}

static void __exit lparcfg_cleanup(void)
{
	if (proc_ppc64_lparcfg)
		remove_proc_entry("lparcfg", proc_ppc64_lparcfg->parent);
}

module_init(lparcfg_init);
module_exit(lparcfg_cleanup);
MODULE_DESCRIPTION("Interface for LPAR configuration data");
MODULE_AUTHOR("Dave Engebretsen");
MODULE_LICENSE("GPL");
