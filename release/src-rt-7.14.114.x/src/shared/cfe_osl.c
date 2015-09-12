/*
 * CFE OS Independent Layer
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: cfe_osl.c 310902 2012-01-26 19:45:33Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>

/* Global ASSERT type */
uint32 g_assert_type = 0;

osl_t *
osl_attach(void *pdev)
{
	osl_t *osh;

	osh = (osl_t *)KMALLOC(sizeof(osl_t), 0);
	ASSERT(osh);

	bzero(osh, sizeof(osl_t));
	osh->pdev = pdev;
	return osh;
}

void
osl_detach(osl_t *osh)
{
	if (osh == NULL)
		return;
	KFREE((void*) KERNADDR(PHYSADDR((ulong)osh)));
}

struct lbuf *
osl_pktget(uint len)
{
	uchar *buf;
	struct lbuf *lb;

	ASSERT(len <= LBDATASZ);

	if (!(buf = KMALLOC(LBUFSZ, 0)))
		return NULL;

	lb = (struct lbuf *) &buf[LBDATASZ];
	bzero(lb, sizeof(struct lbuf));
	lb->head = lb->data = buf;
	lb->end = buf + len;
	lb->len = len;
	lb->tail = lb->data + len;
	return lb;
}

void
osl_pktfree(osl_t *osh, struct lbuf *lb, bool send)
{
	struct lbuf *next;

	if (send && osh->tx_fn)
		osh->tx_fn(osh->tx_ctx, lb, 0);

	for (; lb; lb = next) {
		ASSERT(!lb->link);
		next = lb->next;
		KFREE((void *) KERNADDR(PHYSADDR((ulong) lb->head)));
	}
}

struct lbuf *
osl_pktdup(struct lbuf *lb)
{
	struct lbuf *dup;

	if (!(dup = osl_pktget(lb->len)))
		return NULL;

	bcopy(lb->data, dup->data, lb->len);
	ASSERT(!lb->link);
	return dup;
}

void
osl_pktsetlen(struct lbuf *lb, uint len)
{
	ASSERT((lb->data + len) <= lb->end);

	lb->len = len;
	lb->tail = lb->data + len;
}

uchar *
osl_pktpush(struct lbuf *lb, uint bytes)
{
	ASSERT((lb->data - bytes) >= lb->head);

	lb->data -= bytes;
	lb->len += bytes;

	return lb->data;
}

uchar *
osl_pktpull(struct lbuf *lb, uint bytes)
{
	ASSERT((lb->data + bytes) <= lb->end);
	ASSERT(lb->len >= bytes);

	lb->data += bytes;
	lb->len -= bytes;

	return lb->data;
}

void *
osl_dma_alloc_consistent(uint size, uint16 align_bits, uint *alloced, ulong *pap)
{
	void *buf;
	uint16 align = (1 << align_bits);

	/* fix up the alignment requirements first */
	if (!ISALIGNED(DMA_CONSISTENT_ALIGN, align))
		size += align;
	*alloced = size;

	if (!(buf = KMALLOC(size, DMA_CONSISTENT_ALIGN)))
		return NULL;

	*((ulong *) pap) = PHYSADDR((ulong) buf);

	cfe_flushcache(CFE_CACHE_FLUSH_D);

	return (void *) UNCADDR((ulong) buf);
}

void
osl_dma_free_consistent(void *va)
{
	KFREE((void *) KERNADDR(PHYSADDR((ulong) va)));
}


int
osl_busprobe(uint32 *val, uint32 addr)
{
	*val = R_REG(NULL, (volatile uint32 *) addr);

	return 0;
}

/* translate bcmerros */
int
osl_error(int bcmerror)
{
	if (bcmerror)
		return -1;
	else
		return 0;
}

/* Converts a OS packet to driver packet.
 * The original packet data is copied to the new driver packet
 */
void
osl_pkt_frmnative(iocb_buffer_t *buffer, struct lbuf *lb)
{
	bcopy(buffer->buf_ptr, PKTDATA(NULL, lb), buffer->buf_length);
}

/* Converts a driver packet into OS packet.
 * The data is copied to the OS packet
 */
void
osl_pkt_tonative(struct lbuf* lb, iocb_buffer_t *buffer)
{
	bcopy(PKTDATA(NULL, lb), buffer->buf_ptr, PKTLEN(NULL, lb));
	buffer->buf_retlen = PKTLEN(NULL, lb);

	/* RFC894: Minimum length of IP over Ethernet packet is 46 octets */
	if (buffer->buf_retlen < 60) {
		bzero(buffer->buf_ptr + buffer->buf_retlen, 60 - buffer->buf_retlen);
		buffer->buf_retlen = 60;
	}
}
