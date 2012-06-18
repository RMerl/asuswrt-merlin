#ifndef __IP_SET_HASHES_H
#define __IP_SET_HASHES_H

#define initval_t uint32_t

/* Macros to generate functions */

#ifdef __KERNEL__
#define HASH_RETRY0(type, dtype, cond)					\
static int								\
type##_retry(struct ip_set *set)					\
{									\
	struct ip_set_##type *map = set->data, *tmp;			\
	dtype *elem;							\
	void *members;							\
	u_int32_t i, hashsize = map->hashsize;				\
	int res;							\
									\
	if (map->resize == 0)						\
		return -ERANGE;						\
									\
    again:								\
    	res = 0;							\
    									\
	/* Calculate new hash size */					\
	hashsize += (hashsize * map->resize)/100;			\
	if (hashsize == map->hashsize)					\
		hashsize++;						\
									\
	ip_set_printk("rehashing of set %s triggered: "			\
		      "hashsize grows from %lu to %lu",			\
		      set->name,					\
		      (long unsigned)map->hashsize,			\
		      (long unsigned)hashsize);				\
		      							\
	tmp = kmalloc(sizeof(struct ip_set_##type)			\
		      + map->probes * sizeof(initval_t), GFP_ATOMIC);	\
	if (!tmp) {							\
		DP("out of memory for %zu bytes",			\
		   sizeof(struct ip_set_##type)				\
		   + map->probes * sizeof(initval_t));			\
		return -ENOMEM;						\
	}								\
	tmp->members = harray_malloc(hashsize, sizeof(dtype), GFP_ATOMIC);\
	if (!tmp->members) {						\
		DP("out of memory for %zu bytes", hashsize * sizeof(dtype));\
		kfree(tmp);						\
		return -ENOMEM;						\
	}								\
	tmp->hashsize = hashsize;					\
	tmp->elements = 0;						\
	tmp->probes = map->probes;					\
	tmp->resize = map->resize;					\
	memcpy(tmp->initval, map->initval, map->probes * sizeof(initval_t));\
	__##type##_retry(tmp, map);					\
									\
	write_lock_bh(&set->lock);					\
	map = set->data; /* Play safe */				\
	for (i = 0; i < map->hashsize && res == 0; i++) {		\
		elem = HARRAY_ELEM(map->members, dtype *, i);		\
		if (cond)						\
			res = __##type##_add(tmp, elem);		\
	}								\
	if (res) {							\
		/* Failure, try again */				\
		write_unlock_bh(&set->lock);				\
		harray_free(tmp->members);				\
		kfree(tmp);						\
		goto again;						\
	}								\
									\
	/* Success at resizing! */					\
	members = map->members;						\
									\
	map->hashsize = tmp->hashsize;					\
	map->members = tmp->members;					\
	write_unlock_bh(&set->lock);					\
									\
	harray_free(members);						\
	kfree(tmp);							\
									\
	return 0;							\
}

#define HASH_RETRY(type, dtype)						\
	HASH_RETRY0(type, dtype, *elem)

#define HASH_RETRY2(type, dtype)						\
	HASH_RETRY0(type, dtype, elem->ip || elem->ip1)

#define HASH_CREATE(type, dtype)					\
static int								\
type##_create(struct ip_set *set, const void *data, u_int32_t size)	\
{									\
	const struct ip_set_req_##type##_create *req = data;		\
	struct ip_set_##type *map;					\
	uint16_t i;							\
									\
	if (req->hashsize < 1) {					\
		ip_set_printk("hashsize too small");			\
		return -ENOEXEC;					\
	}								\
									\
	if (req->probes < 1) {						\
		ip_set_printk("probes too small");			\
		return -ENOEXEC;					\
	}								\
									\
	map = kmalloc(sizeof(struct ip_set_##type)			\
		      + req->probes * sizeof(initval_t), GFP_KERNEL);	\
	if (!map) {							\
		DP("out of memory for %zu bytes",			\
		   sizeof(struct ip_set_##type)				\
		   + req->probes * sizeof(initval_t));			\
		return -ENOMEM;						\
	}								\
	for (i = 0; i < req->probes; i++)				\
		get_random_bytes(((initval_t *) map->initval)+i, 4);	\
	map->elements = 0;						\
	map->hashsize = req->hashsize;					\
	map->probes = req->probes;					\
	map->resize = req->resize;					\
	if (__##type##_create(req, map)) {				\
		kfree(map);						\
		return -ENOEXEC;					\
	}								\
	map->members = harray_malloc(map->hashsize, sizeof(dtype), GFP_KERNEL);\
	if (!map->members) {						\
		DP("out of memory for %zu bytes", map->hashsize * sizeof(dtype));\
		kfree(map);						\
		return -ENOMEM;						\
	}								\
									\
	set->data = map;						\
	return 0;							\
}

#define HASH_DESTROY(type)						\
static void								\
type##_destroy(struct ip_set *set)					\
{									\
	struct ip_set_##type *map = set->data;				\
									\
	harray_free(map->members);					\
	kfree(map);							\
									\
	set->data = NULL;						\
}

#define HASH_FLUSH(type, dtype)						\
static void								\
type##_flush(struct ip_set *set)					\
{									\
	struct ip_set_##type *map = set->data;				\
	harray_flush(map->members, map->hashsize, sizeof(dtype));	\
	map->elements = 0;						\
}

#define HASH_FLUSH_CIDR(type, dtype)					\
static void								\
type##_flush(struct ip_set *set)					\
{									\
	struct ip_set_##type *map = set->data;				\
	harray_flush(map->members, map->hashsize, sizeof(dtype));	\
	memset(map->cidr, 0, sizeof(map->cidr));			\
	memset(map->nets, 0, sizeof(map->nets));			\
	map->elements = 0;						\
}

#define HASH_LIST_HEADER(type)						\
static void								\
type##_list_header(const struct ip_set *set, void *data)		\
{									\
	const struct ip_set_##type *map = set->data;			\
	struct ip_set_req_##type##_create *header = data;		\
									\
	header->hashsize = map->hashsize;				\
	header->probes = map->probes;					\
	header->resize = map->resize;					\
	__##type##_list_header(map, header);				\
}

#define HASH_LIST_MEMBERS_SIZE(type, dtype)				\
static int								\
type##_list_members_size(const struct ip_set *set)			\
{									\
	const struct ip_set_##type *map = set->data;			\
									\
	return (map->hashsize * sizeof(dtype));				\
}

#define HASH_LIST_MEMBERS(type, dtype)					\
static void								\
type##_list_members(const struct ip_set *set, void *data)		\
{									\
	const struct ip_set_##type *map = set->data;			\
	dtype *elem;							\
	uint32_t i;							\
									\
	for (i = 0; i < map->hashsize; i++) {				\
		elem = HARRAY_ELEM(map->members, dtype *, i);		\
		((dtype *)data)[i] = *elem;				\
	}								\
}

#define HASH_LIST_MEMBERS_MEMCPY(type, dtype)				\
static void								\
type##_list_members(const struct ip_set *set, void *data)		\
{									\
	const struct ip_set_##type *map = set->data;			\
	dtype *elem;							\
	uint32_t i;							\
									\
	for (i = 0; i < map->hashsize; i++) {				\
		elem = HARRAY_ELEM(map->members, dtype *, i);		\
		memcpy((((dtype *)data)+i), elem, sizeof(dtype));	\
	}								\
}

#define IP_SET_RTYPE(type, __features)					\
struct ip_set_type ip_set_##type = {					\
	.typename		= #type,				\
	.features		= __features,				\
	.protocol_version	= IP_SET_PROTOCOL_VERSION,		\
	.create			= &type##_create,			\
	.retry			= &type##_retry,			\
	.destroy		= &type##_destroy,			\
	.flush			= &type##_flush,			\
	.reqsize		= sizeof(struct ip_set_req_##type),	\
	.addip			= &type##_uadd,				\
	.addip_kernel		= &type##_kadd,				\
	.delip			= &type##_udel,				\
	.delip_kernel		= &type##_kdel,				\
	.testip			= &type##_utest,			\
	.testip_kernel		= &type##_ktest,			\
	.header_size		= sizeof(struct ip_set_req_##type##_create),\
	.list_header		= &type##_list_header,			\
	.list_members_size	= &type##_list_members_size,		\
	.list_members		= &type##_list_members,			\
	.me			= THIS_MODULE,				\
};

/* Helper functions */
static inline void
add_cidr_size(uint8_t *cidr, uint8_t size)
{
	uint8_t next;
	int i;
	
	for (i = 0; i < 30 && cidr[i]; i++) {
		if (cidr[i] < size) {
			next = cidr[i];
			cidr[i] = size;
			size = next;
		}
	}
	if (i < 30)
		cidr[i] = size;
}

static inline void
del_cidr_size(uint8_t *cidr, uint8_t size)
{
	int i;
	
	for (i = 0; i < 29 && cidr[i]; i++) {
		if (cidr[i] == size)
			cidr[i] = size = cidr[i+1];
	}
	cidr[29] = 0;
}
#else
#include <arpa/inet.h>
#endif /* __KERNEL */

#ifndef UINT16_MAX
#define UINT16_MAX 65535
#endif

static unsigned char shifts[] = {255, 253, 249, 241, 225, 193, 129, 1};

static inline ip_set_ip_t 
pack_ip_cidr(ip_set_ip_t ip, unsigned char cidr)
{
	ip_set_ip_t addr, *paddr = &addr;
	unsigned char n, t, *a;

	addr = htonl(ip & (0xFFFFFFFF << (32 - (cidr))));
#ifdef __KERNEL__
	DP("ip:%u.%u.%u.%u/%u", NIPQUAD(addr), cidr);
#endif
	n = cidr / 8;
	t = cidr % 8;	
	a = &((unsigned char *)paddr)[n];
	*a = *a /(1 << (8 - t)) + shifts[t];
#ifdef __KERNEL__
	DP("n: %u, t: %u, a: %u", n, t, *a);
	DP("ip:%u.%u.%u.%u/%u, %u.%u.%u.%u",
	   HIPQUAD(ip), cidr, NIPQUAD(addr));
#endif

	return ntohl(addr);
}


#endif /* __IP_SET_HASHES_H */
