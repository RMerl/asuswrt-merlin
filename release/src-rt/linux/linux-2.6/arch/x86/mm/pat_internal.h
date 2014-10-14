#ifndef __PAT_INTERNAL_H_
#define __PAT_INTERNAL_H_

extern int pat_debug_enable;

#define dprintk(fmt, arg...) \
	do { if (pat_debug_enable) printk(KERN_INFO fmt, ##arg); } while (0)

struct memtype {
	u64			start;
	u64			end;
	u64			subtree_max_end;
	unsigned long		type;
	struct rb_node		rb;
};

static inline char *cattr_name(unsigned long flags)
{
	switch (flags & _PAGE_CACHE_MASK) {
	case _PAGE_CACHE_UC:		return "uncached";
	case _PAGE_CACHE_UC_MINUS:	return "uncached-minus";
	case _PAGE_CACHE_WB:		return "write-back";
	case _PAGE_CACHE_WC:		return "write-combining";
	default:			return "broken";
	}
}

#ifdef CONFIG_X86_PAT
extern int rbt_memtype_check_insert(struct memtype *new,
					unsigned long *new_type);
extern struct memtype *rbt_memtype_erase(u64 start, u64 end);
extern struct memtype *rbt_memtype_lookup(u64 addr);
extern int rbt_memtype_copy_nth_element(struct memtype *out, loff_t pos);
#else
static inline int rbt_memtype_check_insert(struct memtype *new,
					unsigned long *new_type)
{ return 0; }
static inline struct memtype *rbt_memtype_erase(u64 start, u64 end)
{ return NULL; }
static inline struct memtype *rbt_memtype_lookup(u64 addr)
{ return NULL; }
static inline int rbt_memtype_copy_nth_element(struct memtype *out, loff_t pos)
{ return 0; }
#endif

#endif /* __PAT_INTERNAL_H_ */
