#include <libexif/exif-mem.h>

#include <stdlib.h>

struct _ExifMem {
	unsigned int ref_count;
	ExifMemAllocFunc alloc_func;
	ExifMemReallocFunc realloc_func;
	ExifMemFreeFunc free_func;
};

/*! Default memory allocation function. */
static void *
exif_mem_alloc_func (ExifLong ds)
{
	return calloc ((size_t) ds, 1);
}

/*! Default memory reallocation function. */
static void *
exif_mem_realloc_func (void *d, ExifLong ds)
{
	return realloc (d, (size_t) ds);
}

/*! Default memory free function. */
static void
exif_mem_free_func (void *d)
{
	free (d);
}

ExifMem *
exif_mem_new (ExifMemAllocFunc alloc_func, ExifMemReallocFunc realloc_func,
	      ExifMemFreeFunc free_func)
{
	ExifMem *mem;

	if (!alloc_func && !realloc_func) 
		return NULL;
	mem = alloc_func ? alloc_func (sizeof (ExifMem)) :
		           realloc_func (NULL, sizeof (ExifMem));
	if (!mem) return NULL;
	mem->ref_count = 1;

	mem->alloc_func   = alloc_func;
	mem->realloc_func = realloc_func;
	mem->free_func    = free_func;

	return mem;
}

void
exif_mem_ref (ExifMem *mem)
{
	if (!mem) return;
	mem->ref_count++;
}

void
exif_mem_unref (ExifMem *mem)
{
	if (!mem) return;
	if (!--mem->ref_count)
		exif_mem_free (mem, mem);
}

void
exif_mem_free (ExifMem *mem, void *d)
{
	if (!mem) return;
	if (mem->free_func) {
		mem->free_func (d);
		return;
	}
}

void *
exif_mem_alloc (ExifMem *mem, ExifLong ds)
{
	if (!mem) return NULL;
	if (mem->alloc_func || mem->realloc_func)
		return mem->alloc_func ? mem->alloc_func (ds) :
					 mem->realloc_func (NULL, ds);
	return NULL;
}

void *
exif_mem_realloc (ExifMem *mem, void *d, ExifLong ds)
{
	return (mem && mem->realloc_func) ? mem->realloc_func (d, ds) : NULL;
}

ExifMem *
exif_mem_new_default (void)
{
	return exif_mem_new (exif_mem_alloc_func, exif_mem_realloc_func,
			     exif_mem_free_func);
}
