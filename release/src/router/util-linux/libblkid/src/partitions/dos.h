#ifndef BLKID_PARTITIONS_DOS_H
#define BLKID_PARTITIONS_DOS_H

struct dos_partition {
	unsigned char boot_ind;		/* 0x80 - active */
	unsigned char bh, bs, bc;	/* begin CHS */
	unsigned char sys_type;
	unsigned char eh, es, ec;	/* end CHS */
	unsigned char start_sect[4];
	unsigned char nr_sects[4];
} __attribute__((packed));

#define BLKID_MSDOS_PT_OFFSET		0x1be

/* assemble badly aligned little endian integer */
static inline unsigned int assemble4le(unsigned char *p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static inline unsigned int dos_partition_start(struct dos_partition *p)
{
	return assemble4le(&(p->start_sect[0]));
}

static inline unsigned int dos_partition_size(struct dos_partition *p)
{
	return assemble4le(&(p->nr_sects[0]));
}

static inline int is_valid_mbr_signature(const unsigned char *mbr)
{
	return mbr[510] == 0x55 && mbr[511] == 0xaa ? 1 : 0;
}

#endif /* BLKID_PARTITIONS_DOS_H */
