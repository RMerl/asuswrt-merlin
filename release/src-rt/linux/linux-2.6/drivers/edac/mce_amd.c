#include <linux/module.h>
#include <linux/slab.h>

#include "mce_amd.h"

static struct amd_decoder_ops *fam_ops;

static u8 xec_mask	 = 0xf;
static u8 nb_err_cpumask = 0xf;

static bool report_gart_errors;
static void (*nb_bus_decoder)(int node_id, struct mce *m, u32 nbcfg);

void amd_report_gart_errors(bool v)
{
	report_gart_errors = v;
}
EXPORT_SYMBOL_GPL(amd_report_gart_errors);

void amd_register_ecc_decoder(void (*f)(int, struct mce *, u32))
{
	nb_bus_decoder = f;
}
EXPORT_SYMBOL_GPL(amd_register_ecc_decoder);

void amd_unregister_ecc_decoder(void (*f)(int, struct mce *, u32))
{
	if (nb_bus_decoder) {
		WARN_ON(nb_bus_decoder != f);

		nb_bus_decoder = NULL;
	}
}
EXPORT_SYMBOL_GPL(amd_unregister_ecc_decoder);

/*
 * string representation for the different MCA reported error types, see F3x48
 * or MSR0000_0411.
 */

/* transaction type */
const char *tt_msgs[] = { "INSN", "DATA", "GEN", "RESV" };
EXPORT_SYMBOL_GPL(tt_msgs);

/* cache level */
const char *ll_msgs[] = { "RESV", "L1", "L2", "L3/GEN" };
EXPORT_SYMBOL_GPL(ll_msgs);

/* memory transaction type */
const char *rrrr_msgs[] = {
       "GEN", "RD", "WR", "DRD", "DWR", "IRD", "PRF", "EV", "SNP"
};
EXPORT_SYMBOL_GPL(rrrr_msgs);

/* participating processor */
const char *pp_msgs[] = { "SRC", "RES", "OBS", "GEN" };
EXPORT_SYMBOL_GPL(pp_msgs);

/* request timeout */
const char *to_msgs[] = { "no timeout",	"timed out" };
EXPORT_SYMBOL_GPL(to_msgs);

/* memory or i/o */
const char *ii_msgs[] = { "MEM", "RESV", "IO", "GEN" };
EXPORT_SYMBOL_GPL(ii_msgs);

static const char *f10h_nb_mce_desc[] = {
	"HT link data error",
	"Protocol error (link, L3, probe filter, etc.)",
	"Parity error in NB-internal arrays",
	"Link Retry due to IO link transmission error",
	"L3 ECC data cache error",
	"ECC error in L3 cache tag",
	"L3 LRU parity bits error",
	"ECC Error in the Probe Filter directory"
};

static const char * const f15h_ic_mce_desc[] = {
	"UC during a demand linefill from L2",
	"Parity error during data load from IC",
	"Parity error for IC valid bit",
	"Main tag parity error",
	"Parity error in prediction queue",
	"PFB data/address parity error",
	"Parity error in the branch status reg",
	"PFB promotion address error",
	"Tag error during probe/victimization",
	"Parity error for IC probe tag valid bit",
	"PFB non-cacheable bit parity error",
	"PFB valid bit parity error",			/* xec = 0xd */
	"patch RAM",					/* xec = 010 */
	"uop queue",
	"insn buffer",
	"predecode buffer",
	"fetch address FIFO"
};

static const char * const f15h_cu_mce_desc[] = {
	"Fill ECC error on data fills",			/* xec = 0x4 */
	"Fill parity error on insn fills",
	"Prefetcher request FIFO parity error",
	"PRQ address parity error",
	"PRQ data parity error",
	"WCC Tag ECC error",
	"WCC Data ECC error",
	"WCB Data parity error",
	"VB Data/ECC error",
	"L2 Tag ECC error",				/* xec = 0x10 */
	"Hard L2 Tag ECC error",
	"Multiple hits on L2 tag",
	"XAB parity error",
	"PRB address parity error"
};

static const char * const fr_ex_mce_desc[] = {
	"CPU Watchdog timer expire",
	"Wakeup array dest tag",
	"AG payload array",
	"EX payload array",
	"IDRF array",
	"Retire dispatch queue",
	"Mapper checkpoint array",
	"Physical register file EX0 port",
	"Physical register file EX1 port",
	"Physical register file AG0 port",
	"Physical register file AG1 port",
	"Flag register file",
	"DE correctable error could not be corrected"
};

static bool f12h_dc_mce(u16 ec, u8 xec)
{
	bool ret = false;

	if (MEM_ERROR(ec)) {
		u8 ll = LL(ec);
		ret = true;

		if (ll == LL_L2)
			pr_cont("during L1 linefill from L2.\n");
		else if (ll == LL_L1)
			pr_cont("Data/Tag %s error.\n", R4_MSG(ec));
		else
			ret = false;
	}
	return ret;
}

static bool f10h_dc_mce(u16 ec, u8 xec)
{
	if (R4(ec) == R4_GEN && LL(ec) == LL_L1) {
		pr_cont("during data scrub.\n");
		return true;
	}
	return f12h_dc_mce(ec, xec);
}

static bool k8_dc_mce(u16 ec, u8 xec)
{
	if (BUS_ERROR(ec)) {
		pr_cont("during system linefill.\n");
		return true;
	}

	return f10h_dc_mce(ec, xec);
}

static bool f14h_dc_mce(u16 ec, u8 xec)
{
	u8 r4	 = R4(ec);
	bool ret = true;

	if (MEM_ERROR(ec)) {

		if (TT(ec) != TT_DATA || LL(ec) != LL_L1)
			return false;

		switch (r4) {
		case R4_DRD:
		case R4_DWR:
			pr_cont("Data/Tag parity error due to %s.\n",
				(r4 == R4_DRD ? "load/hw prf" : "store"));
			break;
		case R4_EVICT:
			pr_cont("Copyback parity error on a tag miss.\n");
			break;
		case R4_SNOOP:
			pr_cont("Tag parity error during snoop.\n");
			break;
		default:
			ret = false;
		}
	} else if (BUS_ERROR(ec)) {

		if ((II(ec) != II_MEM && II(ec) != II_IO) || LL(ec) != LL_LG)
			return false;

		pr_cont("System read data error on a ");

		switch (r4) {
		case R4_RD:
			pr_cont("TLB reload.\n");
			break;
		case R4_DWR:
			pr_cont("store.\n");
			break;
		case R4_DRD:
			pr_cont("load.\n");
			break;
		default:
			ret = false;
		}
	} else {
		ret = false;
	}

	return ret;
}

static bool f15h_dc_mce(u16 ec, u8 xec)
{
	bool ret = true;

	if (MEM_ERROR(ec)) {

		switch (xec) {
		case 0x0:
			pr_cont("Data Array access error.\n");
			break;

		case 0x1:
			pr_cont("UC error during a linefill from L2/NB.\n");
			break;

		case 0x2:
		case 0x11:
			pr_cont("STQ access error.\n");
			break;

		case 0x3:
			pr_cont("SCB access error.\n");
			break;

		case 0x10:
			pr_cont("Tag error.\n");
			break;

		case 0x12:
			pr_cont("LDQ access error.\n");
			break;

		default:
			ret = false;
		}
	} else if (BUS_ERROR(ec)) {

		if (!xec)
			pr_cont("during system linefill.\n");
		else
			pr_cont(" Internal %s condition.\n",
				((xec == 1) ? "livelock" : "deadlock"));
	} else
		ret = false;

	return ret;
}

static void amd_decode_dc_mce(struct mce *m)
{
	u16 ec = EC(m->status);
	u8 xec = XEC(m->status, xec_mask);

	pr_emerg(HW_ERR "Data Cache Error: ");

	/* TLB error signatures are the same across families */
	if (TLB_ERROR(ec)) {
		if (TT(ec) == TT_DATA) {
			pr_cont("%s TLB %s.\n", LL_MSG(ec),
				((xec == 2) ? "locked miss"
					    : (xec ? "multimatch" : "parity")));
			return;
		}
	} else if (fam_ops->dc_mce(ec, xec))
		;
	else
		pr_emerg(HW_ERR "Corrupted DC MCE info?\n");
}

static bool k8_ic_mce(u16 ec, u8 xec)
{
	u8 ll	 = LL(ec);
	bool ret = true;

	if (!MEM_ERROR(ec))
		return false;

	if (ll == 0x2)
		pr_cont("during a linefill from L2.\n");
	else if (ll == 0x1) {
		switch (R4(ec)) {
		case R4_IRD:
			pr_cont("Parity error during data load.\n");
			break;

		case R4_EVICT:
			pr_cont("Copyback Parity/Victim error.\n");
			break;

		case R4_SNOOP:
			pr_cont("Tag Snoop error.\n");
			break;

		default:
			ret = false;
			break;
		}
	} else
		ret = false;

	return ret;
}

static bool f14h_ic_mce(u16 ec, u8 xec)
{
	u8 r4    = R4(ec);
	bool ret = true;

	if (MEM_ERROR(ec)) {
		if (TT(ec) != 0 || LL(ec) != 1)
			ret = false;

		if (r4 == R4_IRD)
			pr_cont("Data/tag array parity error for a tag hit.\n");
		else if (r4 == R4_SNOOP)
			pr_cont("Tag error during snoop/victimization.\n");
		else
			ret = false;
	}
	return ret;
}

static bool f15h_ic_mce(u16 ec, u8 xec)
{
	bool ret = true;

	if (!MEM_ERROR(ec))
		return false;

	switch (xec) {
	case 0x0 ... 0xa:
		pr_cont("%s.\n", f15h_ic_mce_desc[xec]);
		break;

	case 0xd:
		pr_cont("%s.\n", f15h_ic_mce_desc[xec-2]);
		break;

	case 0x10 ... 0x14:
		pr_cont("Decoder %s parity error.\n", f15h_ic_mce_desc[xec-4]);
		break;

	default:
		ret = false;
	}
	return ret;
}

static void amd_decode_ic_mce(struct mce *m)
{
	u16 ec = EC(m->status);
	u8 xec = XEC(m->status, xec_mask);

	pr_emerg(HW_ERR "Instruction Cache Error: ");

	if (TLB_ERROR(ec))
		pr_cont("%s TLB %s.\n", LL_MSG(ec),
			(xec ? "multimatch" : "parity error"));
	else if (BUS_ERROR(ec)) {
		bool k8 = (boot_cpu_data.x86 == 0xf && (m->status & BIT_64(58)));

		pr_cont("during %s.\n", (k8 ? "system linefill" : "NB data read"));
	} else if (fam_ops->ic_mce(ec, xec))
		;
	else
		pr_emerg(HW_ERR "Corrupted IC MCE info?\n");
}

static void amd_decode_bu_mce(struct mce *m)
{
	u16 ec = EC(m->status);
	u8 xec = XEC(m->status, xec_mask);

	pr_emerg(HW_ERR "Bus Unit Error");

	if (xec == 0x1)
		pr_cont(" in the write data buffers.\n");
	else if (xec == 0x3)
		pr_cont(" in the victim data buffers.\n");
	else if (xec == 0x2 && MEM_ERROR(ec))
		pr_cont(": %s error in the L2 cache tags.\n", R4_MSG(ec));
	else if (xec == 0x0) {
		if (TLB_ERROR(ec))
			pr_cont(": %s error in a Page Descriptor Cache or "
				"Guest TLB.\n", TT_MSG(ec));
		else if (BUS_ERROR(ec))
			pr_cont(": %s/ECC error in data read from NB: %s.\n",
				R4_MSG(ec), PP_MSG(ec));
		else if (MEM_ERROR(ec)) {
			u8 r4 = R4(ec);

			if (r4 >= 0x7)
				pr_cont(": %s error during data copyback.\n",
					R4_MSG(ec));
			else if (r4 <= 0x1)
				pr_cont(": %s parity/ECC error during data "
					"access from L2.\n", R4_MSG(ec));
			else
				goto wrong_bu_mce;
		} else
			goto wrong_bu_mce;
	} else
		goto wrong_bu_mce;

	return;

wrong_bu_mce:
	pr_emerg(HW_ERR "Corrupted BU MCE info?\n");
}

static void amd_decode_cu_mce(struct mce *m)
{
	u16 ec = EC(m->status);
	u8 xec = XEC(m->status, xec_mask);

	pr_emerg(HW_ERR "Combined Unit Error: ");

	if (TLB_ERROR(ec)) {
		if (xec == 0x0)
			pr_cont("Data parity TLB read error.\n");
		else if (xec == 0x1)
			pr_cont("Poison data provided for TLB fill.\n");
		else
			goto wrong_cu_mce;
	} else if (BUS_ERROR(ec)) {
		if (xec > 2)
			goto wrong_cu_mce;

		pr_cont("Error during attempted NB data read.\n");
	} else if (MEM_ERROR(ec)) {
		switch (xec) {
		case 0x4 ... 0xc:
			pr_cont("%s.\n", f15h_cu_mce_desc[xec - 0x4]);
			break;

		case 0x10 ... 0x14:
			pr_cont("%s.\n", f15h_cu_mce_desc[xec - 0x7]);
			break;

		default:
			goto wrong_cu_mce;
		}
	}

	return;

wrong_cu_mce:
	pr_emerg(HW_ERR "Corrupted CU MCE info?\n");
}

static void amd_decode_ls_mce(struct mce *m)
{
	u16 ec = EC(m->status);
	u8 xec = XEC(m->status, xec_mask);

	if (boot_cpu_data.x86 >= 0x14) {
		pr_emerg("You shouldn't be seeing an LS MCE on this cpu family,"
			 " please report on LKML.\n");
		return;
	}

	pr_emerg(HW_ERR "Load Store Error");

	if (xec == 0x0) {
		u8 r4 = R4(ec);

		if (!BUS_ERROR(ec) || (r4 != R4_DRD && r4 != R4_DWR))
			goto wrong_ls_mce;

		pr_cont(" during %s.\n", R4_MSG(ec));
	} else
		goto wrong_ls_mce;

	return;

wrong_ls_mce:
	pr_emerg(HW_ERR "Corrupted LS MCE info?\n");
}

static bool k8_nb_mce(u16 ec, u8 xec)
{
	bool ret = true;

	switch (xec) {
	case 0x1:
		pr_cont("CRC error detected on HT link.\n");
		break;

	case 0x5:
		pr_cont("Invalid GART PTE entry during GART table walk.\n");
		break;

	case 0x6:
		pr_cont("Unsupported atomic RMW received from an IO link.\n");
		break;

	case 0x0:
	case 0x8:
		if (boot_cpu_data.x86 == 0x11)
			return false;

		pr_cont("DRAM ECC error detected on the NB.\n");
		break;

	case 0xd:
		pr_cont("Parity error on the DRAM addr/ctl signals.\n");
		break;

	default:
		ret = false;
		break;
	}

	return ret;
}

static bool f10h_nb_mce(u16 ec, u8 xec)
{
	bool ret = true;
	u8 offset = 0;

	if (k8_nb_mce(ec, xec))
		return true;

	switch(xec) {
	case 0xa ... 0xc:
		offset = 10;
		break;

	case 0xe:
		offset = 11;
		break;

	case 0xf:
		if (TLB_ERROR(ec))
			pr_cont("GART Table Walk data error.\n");
		else if (BUS_ERROR(ec))
			pr_cont("DMA Exclusion Vector Table Walk error.\n");
		else
			ret = false;

		goto out;
		break;

	case 0x19:
		if (boot_cpu_data.x86 == 0x15)
			pr_cont("Compute Unit Data Error.\n");
		else
			ret = false;

		goto out;
		break;

	case 0x1c ... 0x1f:
		offset = 24;
		break;

	default:
		ret = false;

		goto out;
		break;
	}

	pr_cont("%s.\n", f10h_nb_mce_desc[xec - offset]);

out:
	return ret;
}

static bool nb_noop_mce(u16 ec, u8 xec)
{
	return false;
}

void amd_decode_nb_mce(int node_id, struct mce *m, u32 nbcfg)
{
	struct cpuinfo_x86 *c = &boot_cpu_data;
	u16 ec   = EC(m->status);
	u8 xec   = XEC(m->status, 0x1f);
	u32 nbsh = (u32)(m->status >> 32);
	int core = -1;

	pr_emerg(HW_ERR "Northbridge Error (node %d", node_id);

	/* F10h, revD can disable ErrCpu[3:0] through ErrCpuVal */
	if (c->x86 == 0x10 && c->x86_model > 7) {
		if (nbsh & NBSH_ERR_CPU_VAL)
			core = nbsh & nb_err_cpumask;
	} else {
		u8 assoc_cpus = nbsh & nb_err_cpumask;

		if (assoc_cpus > 0)
			core = fls(assoc_cpus) - 1;
	}

	if (core >= 0)
		pr_cont(", core %d): ", core);
	else
		pr_cont("): ");

	switch (xec) {
	case 0x2:
		pr_cont("Sync error (sync packets on HT link detected).\n");
		return;

	case 0x3:
		pr_cont("HT Master abort.\n");
		return;

	case 0x4:
		pr_cont("HT Target abort.\n");
		return;

	case 0x7:
		pr_cont("NB Watchdog timeout.\n");
		return;

	case 0x9:
		pr_cont("SVM DMA Exclusion Vector error.\n");
		return;

	default:
		break;
	}

	if (!fam_ops->nb_mce(ec, xec))
		goto wrong_nb_mce;

	if (c->x86 == 0xf || c->x86 == 0x10 || c->x86 == 0x15)
		if ((xec == 0x8 || xec == 0x0) && nb_bus_decoder)
			nb_bus_decoder(node_id, m, nbcfg);

	return;

wrong_nb_mce:
	pr_emerg(HW_ERR "Corrupted NB MCE info?\n");
}
EXPORT_SYMBOL_GPL(amd_decode_nb_mce);

static void amd_decode_fr_mce(struct mce *m)
{
	struct cpuinfo_x86 *c = &boot_cpu_data;
	u8 xec = XEC(m->status, xec_mask);

	if (c->x86 == 0xf || c->x86 == 0x11)
		goto wrong_fr_mce;

	if (c->x86 != 0x15 && xec != 0x0)
		goto wrong_fr_mce;

	pr_emerg(HW_ERR "%s Error: ",
		 (c->x86 == 0x15 ? "Execution Unit" : "FIROB"));

	if (xec == 0x0 || xec == 0xc)
		pr_cont("%s.\n", fr_ex_mce_desc[xec]);
	else if (xec < 0xd)
		pr_cont("%s parity error.\n", fr_ex_mce_desc[xec]);
	else
		goto wrong_fr_mce;

	return;

wrong_fr_mce:
	pr_emerg(HW_ERR "Corrupted FR MCE info?\n");
}

static void amd_decode_fp_mce(struct mce *m)
{
	u8 xec = XEC(m->status, xec_mask);

	pr_emerg(HW_ERR "Floating Point Unit Error: ");

	switch (xec) {
	case 0x1:
		pr_cont("Free List");
		break;

	case 0x2:
		pr_cont("Physical Register File");
		break;

	case 0x3:
		pr_cont("Retire Queue");
		break;

	case 0x4:
		pr_cont("Scheduler table");
		break;

	case 0x5:
		pr_cont("Status Register File");
		break;

	default:
		goto wrong_fp_mce;
		break;
	}

	pr_cont(" parity error.\n");

	return;

wrong_fp_mce:
	pr_emerg(HW_ERR "Corrupted FP MCE info?\n");
}

static inline void amd_decode_err_code(u16 ec)
{

	pr_emerg(HW_ERR "cache level: %s", LL_MSG(ec));

	if (BUS_ERROR(ec))
		pr_cont(", mem/io: %s", II_MSG(ec));
	else
		pr_cont(", tx: %s", TT_MSG(ec));

	if (MEM_ERROR(ec) || BUS_ERROR(ec)) {
		pr_cont(", mem-tx: %s", R4_MSG(ec));

		if (BUS_ERROR(ec))
			pr_cont(", part-proc: %s (%s)", PP_MSG(ec), TO_MSG(ec));
	}

	pr_cont("\n");
}

/*
 * Filter out unwanted MCE signatures here.
 */
static bool amd_filter_mce(struct mce *m)
{
	u8 xec = (m->status >> 16) & 0x1f;

	/*
	 * NB GART TLB error reporting is disabled by default.
	 */
	if (m->bank == 4 && xec == 0x5 && !report_gart_errors)
		return true;

	return false;
}

int amd_decode_mce(struct notifier_block *nb, unsigned long val, void *data)
{
	struct mce *m = (struct mce *)data;
	struct cpuinfo_x86 *c = &boot_cpu_data;
	int node, ecc;

	if (amd_filter_mce(m))
		return NOTIFY_STOP;

	pr_emerg(HW_ERR "MC%d_STATUS[%s|%s|%s|%s|%s",
		m->bank,
		((m->status & MCI_STATUS_OVER)	? "Over"  : "-"),
		((m->status & MCI_STATUS_UC)	? "UE"	  : "CE"),
		((m->status & MCI_STATUS_MISCV)	? "MiscV" : "-"),
		((m->status & MCI_STATUS_PCC)	? "PCC"	  : "-"),
		((m->status & MCI_STATUS_ADDRV)	? "AddrV" : "-"));

	if (c->x86 == 0x15)
		pr_cont("|%s|%s",
			((m->status & BIT_64(44)) ? "Deferred" : "-"),
			((m->status & BIT_64(43)) ? "Poison"   : "-"));

	/* do the two bits[14:13] together */
	ecc = (m->status >> 45) & 0x3;
	if (ecc)
		pr_cont("|%sECC", ((ecc == 2) ? "C" : "U"));

	pr_cont("]: 0x%016llx\n", m->status);


	switch (m->bank) {
	case 0:
		amd_decode_dc_mce(m);
		break;

	case 1:
		amd_decode_ic_mce(m);
		break;

	case 2:
		if (c->x86 == 0x15)
			amd_decode_cu_mce(m);
		else
			amd_decode_bu_mce(m);
		break;

	case 3:
		amd_decode_ls_mce(m);
		break;

	case 4:
		node = amd_get_nb_id(m->extcpu);
		amd_decode_nb_mce(node, m, 0);
		break;

	case 5:
		amd_decode_fr_mce(m);
		break;

	case 6:
		amd_decode_fp_mce(m);
		break;

	default:
		break;
	}

	amd_decode_err_code(m->status & 0xffff);

	return NOTIFY_STOP;
}
EXPORT_SYMBOL_GPL(amd_decode_mce);

static struct notifier_block amd_mce_dec_nb = {
	.notifier_call	= amd_decode_mce,
};

static int __init mce_amd_init(void)
{
	struct cpuinfo_x86 *c = &boot_cpu_data;

	if (c->x86_vendor != X86_VENDOR_AMD)
		return 0;

	if ((c->x86 < 0xf || c->x86 > 0x12) &&
	    (c->x86 != 0x14 || c->x86_model > 0xf) &&
	    (c->x86 != 0x15 || c->x86_model > 0xf))
		return 0;

	fam_ops = kzalloc(sizeof(struct amd_decoder_ops), GFP_KERNEL);
	if (!fam_ops)
		return -ENOMEM;

	switch (c->x86) {
	case 0xf:
		fam_ops->dc_mce = k8_dc_mce;
		fam_ops->ic_mce = k8_ic_mce;
		fam_ops->nb_mce = k8_nb_mce;
		break;

	case 0x10:
		fam_ops->dc_mce = f10h_dc_mce;
		fam_ops->ic_mce = k8_ic_mce;
		fam_ops->nb_mce = f10h_nb_mce;
		break;

	case 0x11:
		fam_ops->dc_mce = k8_dc_mce;
		fam_ops->ic_mce = k8_ic_mce;
		fam_ops->nb_mce = f10h_nb_mce;
		break;

	case 0x12:
		fam_ops->dc_mce = f12h_dc_mce;
		fam_ops->ic_mce = k8_ic_mce;
		fam_ops->nb_mce = nb_noop_mce;
		break;

	case 0x14:
		nb_err_cpumask  = 0x3;
		fam_ops->dc_mce = f14h_dc_mce;
		fam_ops->ic_mce = f14h_ic_mce;
		fam_ops->nb_mce = nb_noop_mce;
		break;

	case 0x15:
		xec_mask = 0x1f;
		fam_ops->dc_mce = f15h_dc_mce;
		fam_ops->ic_mce = f15h_ic_mce;
		fam_ops->nb_mce = f10h_nb_mce;
		break;

	default:
		printk(KERN_WARNING "Huh? What family is that: %d?!\n", c->x86);
		kfree(fam_ops);
		return -EINVAL;
	}

	pr_info("MCE: In-kernel MCE decoding enabled.\n");

	atomic_notifier_chain_register(&x86_mce_decoder_chain, &amd_mce_dec_nb);

	return 0;
}
early_initcall(mce_amd_init);

#ifdef MODULE
static void __exit mce_amd_exit(void)
{
	atomic_notifier_chain_unregister(&x86_mce_decoder_chain, &amd_mce_dec_nb);
	kfree(fam_ops);
}

MODULE_DESCRIPTION("AMD MCE decoder");
MODULE_ALIAS("edac-mce-amd");
MODULE_LICENSE("GPL");
module_exit(mce_amd_exit);
#endif
