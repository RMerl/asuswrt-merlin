#ifndef _IP_SET_MALLOC_H
#define _IP_SET_MALLOC_H

#ifdef __KERNEL__
#include <linux/vmalloc.h> 

static size_t max_malloc_size = 0, max_page_size = 0;
static size_t default_max_malloc_size = 131072;			/* Guaranteed: slab.c */

static inline int init_max_page_size(void)
{
/* Compatibility glues to support 2.4.36 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#define __GFP_NOWARN		0

	/* Guaranteed: slab.c */
	max_malloc_size = max_page_size = default_max_malloc_size;
#else
	size_t page_size = 0;

#define CACHE(x) if (max_page_size == 0 || x < max_page_size)	\
			page_size = x;
#include <linux/kmalloc_sizes.h>
#undef CACHE
	if (page_size) {
		if (max_malloc_size == 0)
			max_malloc_size = page_size;

		max_page_size = page_size;

		return 1;
	}
#endif
	return 0;
}

struct harray {
	size_t max_elements;
	void *arrays[0];
};

static inline void * 
__harray_malloc(size_t hashsize, size_t typesize, gfp_t flags)
{
	struct harray *harray;
	size_t max_elements, size, i, j;

	BUG_ON(max_page_size == 0);

	if (typesize > max_page_size)
		return NULL;

	max_elements = max_page_size/typesize;
	size = hashsize/max_elements;
	if (hashsize % max_elements)
		size++;
	
	/* Last pointer signals end of arrays */
	harray = kmalloc(sizeof(struct harray) + (size + 1) * sizeof(void *),
			 flags);

	if (!harray)
		return NULL;
	
	for (i = 0; i < size - 1; i++) {
		harray->arrays[i] = kmalloc(max_elements * typesize, flags);
		if (!harray->arrays[i])
			goto undo;
		memset(harray->arrays[i], 0, max_elements * typesize);
	}
	harray->arrays[i] = kmalloc((hashsize - i * max_elements) * typesize, 
				    flags);
	if (!harray->arrays[i])
		goto undo;
	memset(harray->arrays[i], 0, (hashsize - i * max_elements) * typesize);

	harray->max_elements = max_elements;
	harray->arrays[size] = NULL;
	
	return (void *)harray;

    undo:
    	for (j = 0; j < i; j++) {
    		kfree(harray->arrays[j]);
    	}
    	kfree(harray);
    	return NULL;
}

static inline void *
harray_malloc(size_t hashsize, size_t typesize, gfp_t flags)
{
	void *harray;
	
	do {
		harray = __harray_malloc(hashsize, typesize, flags|__GFP_NOWARN);
	} while (harray == NULL && init_max_page_size());
	
	return harray;
}		

static inline void harray_free(void *h)
{
	struct harray *harray = (struct harray *) h;
	size_t i;
	
    	for (i = 0; harray->arrays[i] != NULL; i++)
    		kfree(harray->arrays[i]);
    	kfree(harray);
}

static inline void harray_flush(void *h, size_t hashsize, size_t typesize)
{
	struct harray *harray = (struct harray *) h;
	size_t i;
	
    	for (i = 0; harray->arrays[i+1] != NULL; i++)
		memset(harray->arrays[i], 0, harray->max_elements * typesize);
	memset(harray->arrays[i], 0, 
	       (hashsize - i * harray->max_elements) * typesize);
}

#define HARRAY_ELEM(h, type, which)				\
({								\
	struct harray *__h = (struct harray *)(h);		\
	((type)((__h)->arrays[(which)/(__h)->max_elements])	\
		+ (which)%(__h)->max_elements);			\
})

/* General memory allocation and deallocation */
static inline void * ip_set_malloc(size_t bytes)
{
	BUG_ON(max_malloc_size == 0);

	if (bytes > default_max_malloc_size)
		return vmalloc(bytes);
	else
		return kmalloc(bytes, GFP_KERNEL | __GFP_NOWARN);
}

static inline void ip_set_free(void * data, size_t bytes)
{
	BUG_ON(max_malloc_size == 0);

	if (bytes > default_max_malloc_size)
		vfree(data);
	else
		kfree(data);
}

#endif				/* __KERNEL__ */

#endif /*_IP_SET_MALLOC_H*/
