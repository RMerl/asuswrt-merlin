#include "dvector.h"
#include "dutil.h"

struct __dvhdr {
	size_t size;
	size_t length;
	dvDestructor destroy;
};

dvector
dvCreate(size_t size, dvDestructor destr)
{
	void *mem = xmalloc(sizeof(struct __dvhdr) + (sizeof(void *) * (size+1)));
	struct __dvhdr *hdr=mem;
	dvector ptr = mem+sizeof(struct __dvhdr);

	hdr->size = size;
	hdr->length = 0;
	hdr->destroy = destr;

	return ptr;
}

void
dvAddItem(dvector *vec, void *item)
{
	void *mem = (((void *)*vec) - sizeof(struct __dvhdr));
	struct __dvhdr *hdr=mem;
	size_t size = hdr->size;

	if (hdr->length >= size) {
		size *= 2;
		mem = xrealloc(mem, sizeof(struct __dvhdr) + 
			(sizeof(void *) * (size+1)));
		hdr = mem;
		hdr->size = size;
		*vec = mem + sizeof(struct __dvhdr);
	}
	(*vec)[hdr->length++] = item;
	(*vec)[hdr->length] = NULL;
}

void
dvDestroy(dvector vec)
{
	void *mem = (((void *)vec) - sizeof(struct __dvhdr));
	struct __dvhdr *hdr=mem;

	if (hdr->destroy) {
		uint i;
		for (i=0; i < hdr->length; i++) {
			hdr->destroy(vec[i]);
		}
	}
	xfree(mem);
}

size_t
dvSize(dvector vec)
{
	void *mem = (((void *)vec) - sizeof(struct __dvhdr));
	struct __dvhdr *hdr=mem;
	return hdr->size;
}

size_t
dvLength(dvector vec)
{
	void *mem = (((void *)vec) - sizeof(struct __dvhdr));
	struct __dvhdr *hdr=mem;
	return hdr->length;
}

