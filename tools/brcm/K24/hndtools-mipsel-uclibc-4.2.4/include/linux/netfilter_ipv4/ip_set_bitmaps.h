#ifndef __IP_SET_BITMAPS_H
#define __IP_SET_BITMAPS_H

/* Macros to generate functions */

#ifdef __KERNEL__
#define BITMAP_CREATE(type)						\
static int								\
type##_create(struct ip_set *set, const void *data, u_int32_t size)	\
{									\
	int newbytes;							\
	const struct ip_set_req_##type##_create *req = data;		\
	struct ip_set_##type *map;					\
									\
	if (req->from > req->to) {					\
		DP("bad range");					\
		return -ENOEXEC;					\
	}								\
									\
	map = kmalloc(sizeof(struct ip_set_##type), GFP_KERNEL);	\
	if (!map) {							\
		DP("out of memory for %zu bytes",			\
		   sizeof(struct ip_set_##type));			\
		return -ENOMEM;						\
	}								\
	map->first_ip = req->from;					\
	map->last_ip = req->to;						\
									\
	newbytes = __##type##_create(req, map);				\
	if (newbytes < 0) {						\
		kfree(map);						\
		return newbytes;					\
	}								\
									\
	map->size = newbytes;						\
	map->members = ip_set_malloc(newbytes);				\
	if (!map->members) {						\
		DP("out of memory for %i bytes", newbytes);		\
		kfree(map);						\
		return -ENOMEM;						\
	}								\
	memset(map->members, 0, newbytes);				\
									\
	set->data = map;						\
	return 0;							\
}

#define BITMAP_DESTROY(type)						\
static void								\
type##_destroy(struct ip_set *set)					\
{									\
	struct ip_set_##type *map = set->data;				\
									\
	ip_set_free(map->members, map->size);				\
	kfree(map);							\
									\
	set->data = NULL;						\
}

#define BITMAP_FLUSH(type)						\
static void								\
type##_flush(struct ip_set *set)					\
{									\
	struct ip_set_##type *map = set->data;				\
	memset(map->members, 0, map->size);				\
}

#define BITMAP_LIST_HEADER(type)					\
static void								\
type##_list_header(const struct ip_set *set, void *data)		\
{									\
	const struct ip_set_##type *map = set->data;			\
	struct ip_set_req_##type##_create *header = data;		\
									\
	header->from = map->first_ip;					\
	header->to = map->last_ip;					\
	__##type##_list_header(map, header);				\
}

#define BITMAP_LIST_MEMBERS_SIZE(type)					\
static int								\
type##_list_members_size(const struct ip_set *set)			\
{									\
	const struct ip_set_##type *map = set->data;			\
									\
	return map->size;						\
}

#define BITMAP_LIST_MEMBERS(type)					\
static void								\
type##_list_members(const struct ip_set *set, void *data)		\
{									\
	const struct ip_set_##type *map = set->data;			\
									\
	memcpy(data, map->members, map->size);				\
}

#define IP_SET_TYPE(type, __features)					\
struct ip_set_type ip_set_##type = {					\
	.typename		= #type,				\
	.features		= __features,				\
	.protocol_version	= IP_SET_PROTOCOL_VERSION,		\
	.create			= &type##_create,			\
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
#endif /* __KERNEL */

#endif /* __IP_SET_BITMAPS_H */
