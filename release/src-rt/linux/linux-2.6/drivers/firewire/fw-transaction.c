/*
 * Core IEEE1394 transaction logic
 *
 * Copyright (C) 2004-2006 Kristian Hoegsberg <krh@bitplanet.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>

#include "fw-transaction.h"
#include "fw-topology.h"
#include "fw-device.h"

#define HEADER_PRI(pri)			((pri) << 0)
#define HEADER_TCODE(tcode)		((tcode) << 4)
#define HEADER_RETRY(retry)		((retry) << 8)
#define HEADER_TLABEL(tlabel)		((tlabel) << 10)
#define HEADER_DESTINATION(destination)	((destination) << 16)
#define HEADER_SOURCE(source)		((source) << 16)
#define HEADER_RCODE(rcode)		((rcode) << 12)
#define HEADER_OFFSET_HIGH(offset_high)	((offset_high) << 0)
#define HEADER_DATA_LENGTH(length)	((length) << 16)
#define HEADER_EXTENDED_TCODE(tcode)	((tcode) << 0)

#define HEADER_GET_TCODE(q)		(((q) >> 4) & 0x0f)
#define HEADER_GET_TLABEL(q)		(((q) >> 10) & 0x3f)
#define HEADER_GET_RCODE(q)		(((q) >> 12) & 0x0f)
#define HEADER_GET_DESTINATION(q)	(((q) >> 16) & 0xffff)
#define HEADER_GET_SOURCE(q)		(((q) >> 16) & 0xffff)
#define HEADER_GET_OFFSET_HIGH(q)	(((q) >> 0) & 0xffff)
#define HEADER_GET_DATA_LENGTH(q)	(((q) >> 16) & 0xffff)
#define HEADER_GET_EXTENDED_TCODE(q)	(((q) >> 0) & 0xffff)

#define PHY_CONFIG_GAP_COUNT(gap_count)	(((gap_count) << 16) | (1 << 22))
#define PHY_CONFIG_ROOT_ID(node_id)	((((node_id) & 0x3f) << 24) | (1 << 23))
#define PHY_IDENTIFIER(id)		((id) << 30)

static int
close_transaction(struct fw_transaction *transaction,
		  struct fw_card *card, int rcode,
		  u32 *payload, size_t length)
{
	struct fw_transaction *t;
	unsigned long flags;

	spin_lock_irqsave(&card->lock, flags);
	list_for_each_entry(t, &card->transaction_list, link) {
		if (t == transaction) {
			list_del(&t->link);
			card->tlabel_mask &= ~(1 << t->tlabel);
			break;
		}
	}
	spin_unlock_irqrestore(&card->lock, flags);

	if (&t->link != &card->transaction_list) {
		t->callback(card, rcode, payload, length, t->callback_data);
		return 0;
	}

	return -ENOENT;
}

/*
 * Only valid for transactions that are potentially pending (ie have
 * been sent).
 */
int
fw_cancel_transaction(struct fw_card *card,
		      struct fw_transaction *transaction)
{
	/*
	 * Cancel the packet transmission if it's still queued.  That
	 * will call the packet transmission callback which cancels
	 * the transaction.
	 */

	if (card->driver->cancel_packet(card, &transaction->packet) == 0)
		return 0;

	/*
	 * If the request packet has already been sent, we need to see
	 * if the transaction is still pending and remove it in that case.
	 */

	return close_transaction(transaction, card, RCODE_CANCELLED, NULL, 0);
}
EXPORT_SYMBOL(fw_cancel_transaction);

static void
transmit_complete_callback(struct fw_packet *packet,
			   struct fw_card *card, int status)
{
	struct fw_transaction *t =
	    container_of(packet, struct fw_transaction, packet);

	switch (status) {
	case ACK_COMPLETE:
		close_transaction(t, card, RCODE_COMPLETE, NULL, 0);
		break;
	case ACK_PENDING:
		t->timestamp = packet->timestamp;
		break;
	case ACK_BUSY_X:
	case ACK_BUSY_A:
	case ACK_BUSY_B:
		close_transaction(t, card, RCODE_BUSY, NULL, 0);
		break;
	case ACK_DATA_ERROR:
		close_transaction(t, card, RCODE_DATA_ERROR, NULL, 0);
		break;
	case ACK_TYPE_ERROR:
		close_transaction(t, card, RCODE_TYPE_ERROR, NULL, 0);
		break;
	default:
		/*
		 * In this case the ack is really a juju specific
		 * rcode, so just forward that to the callback.
		 */
		close_transaction(t, card, status, NULL, 0);
		break;
	}
}

static void
fw_fill_request(struct fw_packet *packet, int tcode, int tlabel,
		int node_id, int source_id, int generation, int speed,
		unsigned long long offset, void *payload, size_t length)
{
	int ext_tcode;

	if (tcode > 0x10) {
		ext_tcode = tcode - 0x10;
		tcode = TCODE_LOCK_REQUEST;
	} else
		ext_tcode = 0;

	packet->header[0] =
		HEADER_RETRY(RETRY_X) |
		HEADER_TLABEL(tlabel) |
		HEADER_TCODE(tcode) |
		HEADER_DESTINATION(node_id);
	packet->header[1] =
		HEADER_OFFSET_HIGH(offset >> 32) | HEADER_SOURCE(source_id);
	packet->header[2] =
		offset;

	switch (tcode) {
	case TCODE_WRITE_QUADLET_REQUEST:
		packet->header[3] = *(u32 *)payload;
		packet->header_length = 16;
		packet->payload_length = 0;
		break;

	case TCODE_LOCK_REQUEST:
	case TCODE_WRITE_BLOCK_REQUEST:
		packet->header[3] =
			HEADER_DATA_LENGTH(length) |
			HEADER_EXTENDED_TCODE(ext_tcode);
		packet->header_length = 16;
		packet->payload = payload;
		packet->payload_length = length;
		break;

	case TCODE_READ_QUADLET_REQUEST:
		packet->header_length = 12;
		packet->payload_length = 0;
		break;

	case TCODE_READ_BLOCK_REQUEST:
		packet->header[3] =
			HEADER_DATA_LENGTH(length) |
			HEADER_EXTENDED_TCODE(ext_tcode);
		packet->header_length = 16;
		packet->payload_length = 0;
		break;
	}

	packet->speed = speed;
	packet->generation = generation;
	packet->ack = 0;
}

/**
 * This function provides low-level access to the IEEE1394 transaction
 * logic.  Most C programs would use either fw_read(), fw_write() or
 * fw_lock() instead - those function are convenience wrappers for
 * this function.  The fw_send_request() function is primarily
 * provided as a flexible, one-stop entry point for languages bindings
 * and protocol bindings.
 *
 * FIXME: Document this function further, in particular the possible
 * values for rcode in the callback.  In short, we map ACK_COMPLETE to
 * RCODE_COMPLETE, internal errors set errno and set rcode to
 * RCODE_SEND_ERROR (which is out of range for standard ieee1394
 * rcodes).  All other rcodes are forwarded unchanged.  For all
 * errors, payload is NULL, length is 0.
 *
 * Can not expect the callback to be called before the function
 * returns, though this does happen in some cases (ACK_COMPLETE and
 * errors).
 *
 * The payload is only used for write requests and must not be freed
 * until the callback has been called.
 *
 * @param card the card from which to send the request
 * @param tcode the tcode for this transaction.  Do not use
 *   TCODE_LOCK_REQUEST directly, insted use TCODE_LOCK_MASK_SWAP
 *   etc. to specify tcode and ext_tcode.
 * @param node_id the destination node ID (bus ID and PHY ID concatenated)
 * @param generation the generation for which node_id is valid
 * @param speed the speed to use for sending the request
 * @param offset the 48 bit offset on the destination node
 * @param payload the data payload for the request subaction
 * @param length the length in bytes of the data to read
 * @param callback function to be called when the transaction is completed
 * @param callback_data pointer to arbitrary data, which will be
 *   passed to the callback
 */
void
fw_send_request(struct fw_card *card, struct fw_transaction *t,
		int tcode, int node_id, int generation, int speed,
		unsigned long long offset,
		void *payload, size_t length,
		fw_transaction_callback_t callback, void *callback_data)
{
	unsigned long flags;
	int tlabel, source;

	/*
	 * Bump the flush timer up 100ms first of all so we
	 * don't race with a flush timer callback.
	 */

	mod_timer(&card->flush_timer, jiffies + DIV_ROUND_UP(HZ, 10));

	/*
	 * Allocate tlabel from the bitmap and put the transaction on
	 * the list while holding the card spinlock.
	 */

	spin_lock_irqsave(&card->lock, flags);

	source = card->node_id;
	tlabel = card->current_tlabel;
	if (card->tlabel_mask & (1 << tlabel)) {
		spin_unlock_irqrestore(&card->lock, flags);
		callback(card, RCODE_SEND_ERROR, NULL, 0, callback_data);
		return;
	}

	card->current_tlabel = (card->current_tlabel + 1) & 0x1f;
	card->tlabel_mask |= (1 << tlabel);

	list_add_tail(&t->link, &card->transaction_list);

	spin_unlock_irqrestore(&card->lock, flags);

	/* Initialize rest of transaction, fill out packet and send it. */
	t->node_id = node_id;
	t->tlabel = tlabel;
	t->callback = callback;
	t->callback_data = callback_data;

	fw_fill_request(&t->packet, tcode, t->tlabel,
			node_id, source, generation,
			speed, offset, payload, length);
	t->packet.callback = transmit_complete_callback;

	card->driver->send_request(card, &t->packet);
}
EXPORT_SYMBOL(fw_send_request);

static void
transmit_phy_packet_callback(struct fw_packet *packet,
			     struct fw_card *card, int status)
{
	kfree(packet);
}

static void send_phy_packet(struct fw_card *card, u32 data, int generation)
{
	struct fw_packet *packet;

	packet = kzalloc(sizeof(*packet), GFP_ATOMIC);
	if (packet == NULL)
		return;

	packet->header[0] = data;
	packet->header[1] = ~data;
	packet->header_length = 8;
	packet->payload_length = 0;
	packet->speed = SCODE_100;
	packet->generation = generation;
	packet->callback = transmit_phy_packet_callback;

	card->driver->send_request(card, packet);
}

void fw_send_phy_config(struct fw_card *card,
			int node_id, int generation, int gap_count)
{
	u32 q;

	q = PHY_IDENTIFIER(PHY_PACKET_CONFIG) |
		PHY_CONFIG_ROOT_ID(node_id) |
		PHY_CONFIG_GAP_COUNT(gap_count);

	send_phy_packet(card, q, generation);
}

void fw_flush_transactions(struct fw_card *card)
{
	struct fw_transaction *t, *next;
	struct list_head list;
	unsigned long flags;

	INIT_LIST_HEAD(&list);
	spin_lock_irqsave(&card->lock, flags);
	list_splice_init(&card->transaction_list, &list);
	card->tlabel_mask = 0;
	spin_unlock_irqrestore(&card->lock, flags);

	list_for_each_entry_safe(t, next, &list, link) {
		card->driver->cancel_packet(card, &t->packet);

		/*
		 * At this point cancel_packet will never call the
		 * transaction callback, since we just took all the
		 * transactions out of the list.  So do it here.
		 */
		t->callback(card, RCODE_CANCELLED, NULL, 0, t->callback_data);
	}
}

static struct fw_address_handler *
lookup_overlapping_address_handler(struct list_head *list,
				   unsigned long long offset, size_t length)
{
	struct fw_address_handler *handler;

	list_for_each_entry(handler, list, link) {
		if (handler->offset < offset + length &&
		    offset < handler->offset + handler->length)
			return handler;
	}

	return NULL;
}

static struct fw_address_handler *
lookup_enclosing_address_handler(struct list_head *list,
				 unsigned long long offset, size_t length)
{
	struct fw_address_handler *handler;

	list_for_each_entry(handler, list, link) {
		if (handler->offset <= offset &&
		    offset + length <= handler->offset + handler->length)
			return handler;
	}

	return NULL;
}

static DEFINE_SPINLOCK(address_handler_lock);
static LIST_HEAD(address_handler_list);

const struct fw_address_region fw_low_memory_region =
	{ .start = 0x000000000000ULL, .end = 0x000100000000ULL,  };
const struct fw_address_region fw_high_memory_region =
	{ .start = 0x000100000000ULL, .end = 0xffffe0000000ULL,  };
const struct fw_address_region fw_private_region =
	{ .start = 0xffffe0000000ULL, .end = 0xfffff0000000ULL,  };
const struct fw_address_region fw_csr_region =
	{ .start = 0xfffff0000000ULL, .end = 0xfffff0000800ULL,  };
const struct fw_address_region fw_unit_space_region =
	{ .start = 0xfffff0000900ULL, .end = 0x1000000000000ULL, };
EXPORT_SYMBOL(fw_low_memory_region);
EXPORT_SYMBOL(fw_high_memory_region);
EXPORT_SYMBOL(fw_private_region);
EXPORT_SYMBOL(fw_csr_region);
EXPORT_SYMBOL(fw_unit_space_region);

/**
 * Allocate a range of addresses in the node space of the OHCI
 * controller.  When a request is received that falls within the
 * specified address range, the specified callback is invoked.  The
 * parameters passed to the callback give the details of the
 * particular request
 */
int
fw_core_add_address_handler(struct fw_address_handler *handler,
			    const struct fw_address_region *region)
{
	struct fw_address_handler *other;
	unsigned long flags;
	int ret = -EBUSY;

	spin_lock_irqsave(&address_handler_lock, flags);

	handler->offset = region->start;
	while (handler->offset + handler->length <= region->end) {
		other =
		    lookup_overlapping_address_handler(&address_handler_list,
						       handler->offset,
						       handler->length);
		if (other != NULL) {
			handler->offset += other->length;
		} else {
			list_add_tail(&handler->link, &address_handler_list);
			ret = 0;
			break;
		}
	}

	spin_unlock_irqrestore(&address_handler_lock, flags);

	return ret;
}
EXPORT_SYMBOL(fw_core_add_address_handler);

/**
 * Deallocate a range of addresses allocated with fw_allocate.  This
 * will call the associated callback one last time with a the special
 * tcode TCODE_DEALLOCATE, to let the client destroy the registered
 * callback data.  For convenience, the callback parameters offset and
 * length are set to the start and the length respectively for the
 * deallocated region, payload is set to NULL.
 */
void fw_core_remove_address_handler(struct fw_address_handler *handler)
{
	unsigned long flags;

	spin_lock_irqsave(&address_handler_lock, flags);
	list_del(&handler->link);
	spin_unlock_irqrestore(&address_handler_lock, flags);
}
EXPORT_SYMBOL(fw_core_remove_address_handler);

struct fw_request {
	struct fw_packet response;
	u32 request_header[4];
	int ack;
	u32 length;
	u32 data[0];
};

static void
free_response_callback(struct fw_packet *packet,
		       struct fw_card *card, int status)
{
	struct fw_request *request;

	request = container_of(packet, struct fw_request, response);
	kfree(request);
}

void
fw_fill_response(struct fw_packet *response, u32 *request_header,
		 int rcode, void *payload, size_t length)
{
	int tcode, tlabel, extended_tcode, source, destination;

	tcode          = HEADER_GET_TCODE(request_header[0]);
	tlabel         = HEADER_GET_TLABEL(request_header[0]);
	source         = HEADER_GET_DESTINATION(request_header[0]);
	destination    = HEADER_GET_SOURCE(request_header[1]);
	extended_tcode = HEADER_GET_EXTENDED_TCODE(request_header[3]);

	response->header[0] =
		HEADER_RETRY(RETRY_1) |
		HEADER_TLABEL(tlabel) |
		HEADER_DESTINATION(destination);
	response->header[1] =
		HEADER_SOURCE(source) |
		HEADER_RCODE(rcode);
	response->header[2] = 0;

	switch (tcode) {
	case TCODE_WRITE_QUADLET_REQUEST:
	case TCODE_WRITE_BLOCK_REQUEST:
		response->header[0] |= HEADER_TCODE(TCODE_WRITE_RESPONSE);
		response->header_length = 12;
		response->payload_length = 0;
		break;

	case TCODE_READ_QUADLET_REQUEST:
		response->header[0] |=
			HEADER_TCODE(TCODE_READ_QUADLET_RESPONSE);
		if (payload != NULL)
			response->header[3] = *(u32 *)payload;
		else
			response->header[3] = 0;
		response->header_length = 16;
		response->payload_length = 0;
		break;

	case TCODE_READ_BLOCK_REQUEST:
	case TCODE_LOCK_REQUEST:
		response->header[0] |= HEADER_TCODE(tcode + 2);
		response->header[3] =
			HEADER_DATA_LENGTH(length) |
			HEADER_EXTENDED_TCODE(extended_tcode);
		response->header_length = 16;
		response->payload = payload;
		response->payload_length = length;
		break;

	default:
		BUG();
		return;
	}
}
EXPORT_SYMBOL(fw_fill_response);

static struct fw_request *
allocate_request(struct fw_packet *p)
{
	struct fw_request *request;
	u32 *data, length;
	int request_tcode, t;

	request_tcode = HEADER_GET_TCODE(p->header[0]);
	switch (request_tcode) {
	case TCODE_WRITE_QUADLET_REQUEST:
		data = &p->header[3];
		length = 4;
		break;

	case TCODE_WRITE_BLOCK_REQUEST:
	case TCODE_LOCK_REQUEST:
		data = p->payload;
		length = HEADER_GET_DATA_LENGTH(p->header[3]);
		break;

	case TCODE_READ_QUADLET_REQUEST:
		data = NULL;
		length = 4;
		break;

	case TCODE_READ_BLOCK_REQUEST:
		data = NULL;
		length = HEADER_GET_DATA_LENGTH(p->header[3]);
		break;

	default:
		BUG();
		return NULL;
	}

	request = kmalloc(sizeof(*request) + length, GFP_ATOMIC);
	if (request == NULL)
		return NULL;

	t = (p->timestamp & 0x1fff) + 4000;
	if (t >= 8000)
		t = (p->timestamp & ~0x1fff) + 0x2000 + t - 8000;
	else
		t = (p->timestamp & ~0x1fff) + t;

	request->response.speed = p->speed;
	request->response.timestamp = t;
	request->response.generation = p->generation;
	request->response.ack = 0;
	request->response.callback = free_response_callback;
	request->ack = p->ack;
	request->length = length;
	if (data)
		memcpy(request->data, data, length);

	memcpy(request->request_header, p->header, sizeof(p->header));

	return request;
}

void
fw_send_response(struct fw_card *card, struct fw_request *request, int rcode)
{
	/*
	 * Broadcast packets are reported as ACK_COMPLETE, so this
	 * check is sufficient to ensure we don't send response to
	 * broadcast packets or posted writes.
	 */
	if (request->ack != ACK_PENDING) {
		kfree(request);
		return;
	}

	if (rcode == RCODE_COMPLETE)
		fw_fill_response(&request->response, request->request_header,
				 rcode, request->data, request->length);
	else
		fw_fill_response(&request->response, request->request_header,
				 rcode, NULL, 0);

	card->driver->send_response(card, &request->response);
}
EXPORT_SYMBOL(fw_send_response);

void
fw_core_handle_request(struct fw_card *card, struct fw_packet *p)
{
	struct fw_address_handler *handler;
	struct fw_request *request;
	unsigned long long offset;
	unsigned long flags;
	int tcode, destination, source;

	if (p->payload_length > 2048) {
		/* FIXME: send error response. */
		return;
	}

	if (p->ack != ACK_PENDING && p->ack != ACK_COMPLETE)
		return;

	request = allocate_request(p);
	if (request == NULL) {
		/* FIXME: send statically allocated busy packet. */
		return;
	}

	offset      =
		((unsigned long long)
		 HEADER_GET_OFFSET_HIGH(p->header[1]) << 32) | p->header[2];
	tcode       = HEADER_GET_TCODE(p->header[0]);
	destination = HEADER_GET_DESTINATION(p->header[0]);
	source      = HEADER_GET_SOURCE(p->header[0]);

	spin_lock_irqsave(&address_handler_lock, flags);
	handler = lookup_enclosing_address_handler(&address_handler_list,
						   offset, request->length);
	spin_unlock_irqrestore(&address_handler_lock, flags);

	/*
	 * FIXME: lookup the fw_node corresponding to the sender of
	 * this request and pass that to the address handler instead
	 * of the node ID.  We may also want to move the address
	 * allocations to fw_node so we only do this callback if the
	 * upper layers registered it for this node.
	 */

	if (handler == NULL)
		fw_send_response(card, request, RCODE_ADDRESS_ERROR);
	else
		handler->address_callback(card, request,
					  tcode, destination, source,
					  p->generation, p->speed, offset,
					  request->data, request->length,
					  handler->callback_data);
}
EXPORT_SYMBOL(fw_core_handle_request);

void
fw_core_handle_response(struct fw_card *card, struct fw_packet *p)
{
	struct fw_transaction *t;
	unsigned long flags;
	u32 *data;
	size_t data_length;
	int tcode, tlabel, destination, source, rcode;

	tcode       = HEADER_GET_TCODE(p->header[0]);
	tlabel      = HEADER_GET_TLABEL(p->header[0]);
	destination = HEADER_GET_DESTINATION(p->header[0]);
	source      = HEADER_GET_SOURCE(p->header[1]);
	rcode       = HEADER_GET_RCODE(p->header[1]);

	spin_lock_irqsave(&card->lock, flags);
	list_for_each_entry(t, &card->transaction_list, link) {
		if (t->node_id == source && t->tlabel == tlabel) {
			list_del(&t->link);
			card->tlabel_mask &= ~(1 << t->tlabel);
			break;
		}
	}
	spin_unlock_irqrestore(&card->lock, flags);

	if (&t->link == &card->transaction_list) {
		fw_notify("Unsolicited response (source %x, tlabel %x)\n",
			  source, tlabel);
		return;
	}

	/*
	 * FIXME: sanity check packet, is length correct, does tcodes
	 * and addresses match.
	 */

	switch (tcode) {
	case TCODE_READ_QUADLET_RESPONSE:
		data = (u32 *) &p->header[3];
		data_length = 4;
		break;

	case TCODE_WRITE_RESPONSE:
		data = NULL;
		data_length = 0;
		break;

	case TCODE_READ_BLOCK_RESPONSE:
	case TCODE_LOCK_RESPONSE:
		data = p->payload;
		data_length = HEADER_GET_DATA_LENGTH(p->header[3]);
		break;

	default:
		/* Should never happen, this is just to shut up gcc. */
		data = NULL;
		data_length = 0;
		break;
	}

	t->callback(card, rcode, data, data_length, t->callback_data);
}
EXPORT_SYMBOL(fw_core_handle_response);

const struct fw_address_region topology_map_region =
	{ .start = 0xfffff0001000ull, .end = 0xfffff0001400ull, };

static void
handle_topology_map(struct fw_card *card, struct fw_request *request,
		    int tcode, int destination, int source,
		    int generation, int speed,
		    unsigned long long offset,
		    void *payload, size_t length, void *callback_data)
{
	int i, start, end;
	u32 *map;

	if (!TCODE_IS_READ_REQUEST(tcode)) {
		fw_send_response(card, request, RCODE_TYPE_ERROR);
		return;
	}

	if ((offset & 3) > 0 || (length & 3) > 0) {
		fw_send_response(card, request, RCODE_ADDRESS_ERROR);
		return;
	}

	start = (offset - topology_map_region.start) / 4;
	end = start + length / 4;
	map = payload;

	for (i = 0; i < length / 4; i++)
		map[i] = cpu_to_be32(card->topology_map[start + i]);

	fw_send_response(card, request, RCODE_COMPLETE);
}

static struct fw_address_handler topology_map = {
	.length			= 0x200,
	.address_callback	= handle_topology_map,
};

const struct fw_address_region registers_region =
	{ .start = 0xfffff0000000ull, .end = 0xfffff0000400ull, };

static void
handle_registers(struct fw_card *card, struct fw_request *request,
		 int tcode, int destination, int source,
		 int generation, int speed,
		 unsigned long long offset,
		 void *payload, size_t length, void *callback_data)
{
	int reg = offset - CSR_REGISTER_BASE;
	unsigned long long bus_time;
	__be32 *data = payload;

	switch (reg) {
	case CSR_CYCLE_TIME:
	case CSR_BUS_TIME:
		if (!TCODE_IS_READ_REQUEST(tcode) || length != 4) {
			fw_send_response(card, request, RCODE_TYPE_ERROR);
			break;
		}

		bus_time = card->driver->get_bus_time(card);
		if (reg == CSR_CYCLE_TIME)
			*data = cpu_to_be32(bus_time);
		else
			*data = cpu_to_be32(bus_time >> 25);
		fw_send_response(card, request, RCODE_COMPLETE);
		break;

	case CSR_BUS_MANAGER_ID:
	case CSR_BANDWIDTH_AVAILABLE:
	case CSR_CHANNELS_AVAILABLE_HI:
	case CSR_CHANNELS_AVAILABLE_LO:
		/*
		 * FIXME: these are handled by the OHCI hardware and
		 * the stack never sees these request. If we add
		 * support for a new type of controller that doesn't
		 * handle this in hardware we need to deal with these
		 * transactions.
		 */
		BUG();
		break;

	case CSR_BUSY_TIMEOUT:
		/* FIXME: Implement this. */
	default:
		fw_send_response(card, request, RCODE_ADDRESS_ERROR);
		break;
	}
}

static struct fw_address_handler registers = {
	.length			= 0x400,
	.address_callback	= handle_registers,
};

MODULE_AUTHOR("Kristian Hoegsberg <krh@bitplanet.net>");
MODULE_DESCRIPTION("Core IEEE1394 transaction logic");
MODULE_LICENSE("GPL");

static const u32 vendor_textual_descriptor[] = {
	/* textual descriptor leaf () */
	0x00060000,
	0x00000000,
	0x00000000,
	0x4c696e75,		/* L i n u */
	0x78204669,		/* x   F i */
	0x72657769,		/* r e w i */
	0x72650000,		/* r e     */
};

static const u32 model_textual_descriptor[] = {
	/* model descriptor leaf () */
	0x00030000,
	0x00000000,
	0x00000000,
	0x4a756a75,		/* J u j u */
};

static struct fw_descriptor vendor_id_descriptor = {
	.length = ARRAY_SIZE(vendor_textual_descriptor),
	.immediate = 0x03d00d1e,
	.key = 0x81000000,
	.data = vendor_textual_descriptor,
};

static struct fw_descriptor model_id_descriptor = {
	.length = ARRAY_SIZE(model_textual_descriptor),
	.immediate = 0x17000001,
	.key = 0x81000000,
	.data = model_textual_descriptor,
};

static int __init fw_core_init(void)
{
	int retval;

	retval = bus_register(&fw_bus_type);
	if (retval < 0)
		return retval;

	fw_cdev_major = register_chrdev(0, "firewire", &fw_device_ops);
	if (fw_cdev_major < 0) {
		bus_unregister(&fw_bus_type);
		return fw_cdev_major;
	}

	retval = fw_core_add_address_handler(&topology_map,
					     &topology_map_region);
	BUG_ON(retval < 0);

	retval = fw_core_add_address_handler(&registers,
					     &registers_region);
	BUG_ON(retval < 0);

	/* Add the vendor textual descriptor. */
	retval = fw_core_add_descriptor(&vendor_id_descriptor);
	BUG_ON(retval < 0);
	retval = fw_core_add_descriptor(&model_id_descriptor);
	BUG_ON(retval < 0);

	return 0;
}

static void __exit fw_core_cleanup(void)
{
	unregister_chrdev(fw_cdev_major, "firewire");
	bus_unregister(&fw_bus_type);
}

module_init(fw_core_init);
module_exit(fw_core_cleanup);
