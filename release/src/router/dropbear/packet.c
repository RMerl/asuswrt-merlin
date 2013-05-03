/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002,2003 Matt Johnston
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

#include "includes.h"
#include "packet.h"
#include "session.h"
#include "dbutil.h"
#include "ssh.h"
#include "algo.h"
#include "buffer.h"
#include "kex.h"
#include "random.h"
#include "service.h"
#include "auth.h"
#include "channel.h"

static int read_packet_init();
static void make_mac(unsigned int seqno, const struct key_context_directional * key_state,
		buffer * clear_buf, unsigned int clear_len, 
		unsigned char *output_mac);
static int checkmac();

#define ZLIB_COMPRESS_INCR 100
#define ZLIB_DECOMPRESS_INCR 100
#ifndef DISABLE_ZLIB
static buffer* buf_decompress(buffer* buf, unsigned int len);
static void buf_compress(buffer * dest, buffer * src, unsigned int len);
#endif

/* non-blocking function writing out a current encrypted packet */
void write_packet() {

	int len, written;
	buffer * writebuf = NULL;
	time_t now;
	unsigned packet_type;
	int all_ignore = 1;
#ifdef HAVE_WRITEV
	struct iovec *iov = NULL;
	int i;
	struct Link *l;
#endif
	
	TRACE2(("enter write_packet"))
	dropbear_assert(!isempty(&ses.writequeue));

#ifdef HAVE_WRITEV
	iov = m_malloc(sizeof(*iov) * ses.writequeue.count);
	for (l = ses.writequeue.head, i = 0; l; l = l->link, i++)
	{
		writebuf = (buffer*)l->item;
		packet_type = writebuf->data[writebuf->len-1];
		len = writebuf->len - 1 - writebuf->pos;
		dropbear_assert(len > 0);
		all_ignore &= (packet_type == SSH_MSG_IGNORE);
		TRACE2(("write_packet writev #%d  type %d len %d/%d", i, packet_type,
				len, writebuf->len-1))
		iov[i].iov_base = buf_getptr(writebuf, len);
		iov[i].iov_len = len;
	}
	written = writev(ses.sock_out, iov, ses.writequeue.count);
	if (written < 0) {
		if (errno == EINTR) {
			m_free(iov);
			TRACE2(("leave writepacket: EINTR"))
			return;
		} else {
			dropbear_exit("Error writing");
		}
	} 

	if (written == 0) {
		ses.remoteclosed();
	}

	while (written > 0) {
		writebuf = (buffer*)examine(&ses.writequeue);
		len = writebuf->len - 1 - writebuf->pos;
		if (len > written) {
			// partial buffer write
			buf_incrpos(writebuf, written);
			written = 0;
		} else {
			written -= len;
			dequeue(&ses.writequeue);
			buf_free(writebuf);
		}
	}

	m_free(iov);

#else
	/* Get the next buffer in the queue of encrypted packets to write*/
	writebuf = (buffer*)examine(&ses.writequeue);

	/* The last byte of the buffer is not to be transmitted, but is 
	 * a cleartext packet_type indicator */
	packet_type = writebuf->data[writebuf->len-1];
	len = writebuf->len - 1 - writebuf->pos;
	dropbear_assert(len > 0);
	/* Try to write as much as possible */
	written = write(ses.sock_out, buf_getptr(writebuf, len), len);

	if (written < 0) {
		if (errno == EINTR) {
			TRACE2(("leave writepacket: EINTR"))
			return;
		} else {
			dropbear_exit("Error writing");
		}
	} 
	all_ignore = (packet_type == SSH_MSG_IGNORE);

	if (written == 0) {
		ses.remoteclosed();
	}

	if (written == len) {
		/* We've finished with the packet, free it */
		dequeue(&ses.writequeue);
		buf_free(writebuf);
		writebuf = NULL;
	} else {
		/* More packet left to write, leave it in the queue for later */
		buf_incrpos(writebuf, written);
	}

#endif
	now = time(NULL);
	ses.last_trx_packet_time = now;

	if (!all_ignore) {
		ses.last_packet_time = now;
	}

	TRACE2(("leave write_packet"))
}

/* Non-blocking function reading available portion of a packet into the
 * ses's buffer, decrypting the length if encrypted, decrypting the
 * full portion if possible */
void read_packet() {

	int len;
	unsigned int maxlen;
	unsigned char blocksize;

	TRACE2(("enter read_packet"))
	blocksize = ses.keys->recv.algo_crypt->blocksize;
	
	if (ses.readbuf == NULL || ses.readbuf->len < blocksize) {
		int ret;
		/* In the first blocksize of a packet */

		/* Read the first blocksize of the packet, so we can decrypt it and
		 * find the length of the whole packet */
		ret = read_packet_init();

		if (ret == DROPBEAR_FAILURE) {
			/* didn't read enough to determine the length */
			TRACE2(("leave read_packet: packetinit done"))
			return;
		}
	}

	/* Attempt to read the remainder of the packet, note that there
	 * mightn't be any available (EAGAIN) */
	maxlen = ses.readbuf->len - ses.readbuf->pos;
	if (maxlen == 0) {
		/* Occurs when the packet is only a single block long and has all
		 * been read in read_packet_init().  Usually means that MAC is disabled
		 */
		len = 0;
	} else {
		len = read(ses.sock_in, buf_getptr(ses.readbuf, maxlen), maxlen);

		if (len == 0) {
			ses.remoteclosed();
		}

		if (len < 0) {
			if (errno == EINTR || errno == EAGAIN) {
				TRACE2(("leave read_packet: EINTR or EAGAIN"))
				return;
			} else {
				dropbear_exit("Error reading: %s", strerror(errno));
			}
		}

		buf_incrpos(ses.readbuf, len);
	}

	if ((unsigned int)len == maxlen) {
		/* The whole packet has been read */
		decrypt_packet();
		/* The main select() loop process_packet() to
		 * handle the packet contents... */
	}
	TRACE2(("leave read_packet"))
}

/* Function used to read the initial portion of a packet, and determine the
 * length. Only called during the first BLOCKSIZE of a packet. */
/* Returns DROPBEAR_SUCCESS if the length is determined, 
 * DROPBEAR_FAILURE otherwise */
static int read_packet_init() {

	unsigned int maxlen;
	int slen;
	unsigned int len;
	unsigned int blocksize;
	unsigned int macsize;


	blocksize = ses.keys->recv.algo_crypt->blocksize;
	macsize = ses.keys->recv.algo_mac->hashsize;

	if (ses.readbuf == NULL) {
		/* start of a new packet */
		ses.readbuf = buf_new(INIT_READBUF);
	}

	maxlen = blocksize - ses.readbuf->pos;
			
	/* read the rest of the packet if possible */
	slen = read(ses.sock_in, buf_getwriteptr(ses.readbuf, maxlen),
			maxlen);
	if (slen == 0) {
		ses.remoteclosed();
	}
	if (slen < 0) {
		if (errno == EINTR) {
			TRACE2(("leave read_packet_init: EINTR"))
			return DROPBEAR_FAILURE;
		}
		dropbear_exit("Error reading: %s", strerror(errno));
	}

	buf_incrwritepos(ses.readbuf, slen);

	if ((unsigned int)slen != maxlen) {
		/* don't have enough bytes to determine length, get next time */
		return DROPBEAR_FAILURE;
	}

	/* now we have the first block, need to get packet length, so we decrypt
	 * the first block (only need first 4 bytes) */
	buf_setpos(ses.readbuf, 0);
	if (ses.keys->recv.crypt_mode->decrypt(buf_getptr(ses.readbuf, blocksize), 
				buf_getwriteptr(ses.readbuf, blocksize),
				blocksize,
				&ses.keys->recv.cipher_state) != CRYPT_OK) {
		dropbear_exit("Error decrypting");
	}
	len = buf_getint(ses.readbuf) + 4 + macsize;

	TRACE2(("packet size is %d, block %d mac %d", len, blocksize, macsize))


	/* check packet length */
	if ((len > RECV_MAX_PACKET_LEN) ||
		(len < MIN_PACKET_LEN + macsize) ||
		((len - macsize) % blocksize != 0)) {
		dropbear_exit("Integrity error (bad packet size %d)", len);
	}

	if (len > ses.readbuf->size) {
		buf_resize(ses.readbuf, len);		
	}
	buf_setlen(ses.readbuf, len);
	buf_setpos(ses.readbuf, blocksize);
	return DROPBEAR_SUCCESS;
}

/* handle the received packet */
void decrypt_packet() {

	unsigned char blocksize;
	unsigned char macsize;
	unsigned int padlen;
	unsigned int len;

	TRACE2(("enter decrypt_packet"))
	blocksize = ses.keys->recv.algo_crypt->blocksize;
	macsize = ses.keys->recv.algo_mac->hashsize;

	ses.kexstate.datarecv += ses.readbuf->len;

	/* we've already decrypted the first blocksize in read_packet_init */
	buf_setpos(ses.readbuf, blocksize);

	/* decrypt it in-place */
	len = ses.readbuf->len - macsize - ses.readbuf->pos;
	if (ses.keys->recv.crypt_mode->decrypt(
				buf_getptr(ses.readbuf, len), 
				buf_getwriteptr(ses.readbuf, len),
				len,
				&ses.keys->recv.cipher_state) != CRYPT_OK) {
		dropbear_exit("Error decrypting");
	}
	buf_incrpos(ses.readbuf, len);

	/* check the hmac */
	if (checkmac() != DROPBEAR_SUCCESS) {
		dropbear_exit("Integrity error");
	}

	/* get padding length */
	buf_setpos(ses.readbuf, PACKET_PADDING_OFF);
	padlen = buf_getbyte(ses.readbuf);
		
	/* payload length */
	/* - 4 - 1 is for LEN and PADLEN values */
	len = ses.readbuf->len - padlen - 4 - 1 - macsize;
	if ((len > RECV_MAX_PAYLOAD_LEN) || (len < 1)) {
		dropbear_exit("Bad packet size %d", len);
	}

	buf_setpos(ses.readbuf, PACKET_PAYLOAD_OFF);

#ifndef DISABLE_ZLIB
	if (is_compress_recv()) {
		/* decompress */
		ses.payload = buf_decompress(ses.readbuf, len);
	} else 
#endif
	{
		/* copy payload */
		ses.payload = buf_new(len);
		memcpy(ses.payload->data, buf_getptr(ses.readbuf, len), len);
		buf_incrlen(ses.payload, len);
	}

	buf_free(ses.readbuf);
	ses.readbuf = NULL;
	buf_setpos(ses.payload, 0);

	ses.recvseq++;

	TRACE2(("leave decrypt_packet"))
}

/* Checks the mac at the end of a decrypted readbuf.
 * Returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
static int checkmac() {

	unsigned char mac_bytes[MAX_MAC_LEN];
	unsigned int mac_size, contents_len;
	
	mac_size = ses.keys->recv.algo_mac->hashsize;
	contents_len = ses.readbuf->len - mac_size;

	buf_setpos(ses.readbuf, 0);
	make_mac(ses.recvseq, &ses.keys->recv, ses.readbuf, contents_len, mac_bytes);

	/* compare the hash */
	buf_setpos(ses.readbuf, contents_len);
	if (memcmp(mac_bytes, buf_getptr(ses.readbuf, mac_size), mac_size) != 0) {
		return DROPBEAR_FAILURE;
	} else {
		return DROPBEAR_SUCCESS;
	}
}

#ifndef DISABLE_ZLIB
/* returns a pointer to a newly created buffer */
static buffer* buf_decompress(buffer* buf, unsigned int len) {

	int result;
	buffer * ret;
	z_streamp zstream;

	zstream = ses.keys->recv.zstream;
	ret = buf_new(len);

	zstream->avail_in = len;
	zstream->next_in = buf_getptr(buf, len);

	/* decompress the payload, incrementally resizing the output buffer */
	while (1) {

		zstream->avail_out = ret->size - ret->pos;
		zstream->next_out = buf_getwriteptr(ret, zstream->avail_out);

		result = inflate(zstream, Z_SYNC_FLUSH);

		buf_setlen(ret, ret->size - zstream->avail_out);
		buf_setpos(ret, ret->len);

		if (result != Z_BUF_ERROR && result != Z_OK) {
			dropbear_exit("zlib error");
		}

		if (zstream->avail_in == 0 &&
		   		(zstream->avail_out != 0 || result == Z_BUF_ERROR)) {
			/* we can only exit if avail_out hasn't all been used,
			 * and there's no remaining input */
			return ret;
		}

		if (zstream->avail_out == 0) {
			buf_resize(ret, ret->size + ZLIB_DECOMPRESS_INCR);
		}
	}
}
#endif


/* returns 1 if the packet is a valid type during kex (see 7.1 of rfc4253) */
static int packet_is_okay_kex(unsigned char type) {
	if (type >= SSH_MSG_USERAUTH_REQUEST) {
		return 0;
	}
	if (type == SSH_MSG_SERVICE_REQUEST || type == SSH_MSG_SERVICE_ACCEPT) {
		return 0;
	}
	if (type == SSH_MSG_KEXINIT) {
		/* XXX should this die horribly if !dataallowed ?? */
		return 0;
	}
	return 1;
}

static void enqueue_reply_packet() {
	struct packetlist * new_item = NULL;
	new_item = m_malloc(sizeof(struct packetlist));
	new_item->next = NULL;
	
	new_item->payload = buf_newcopy(ses.writepayload);
	buf_setpos(ses.writepayload, 0);
	buf_setlen(ses.writepayload, 0);
	
	if (ses.reply_queue_tail) {
		ses.reply_queue_tail->next = new_item;
	} else {
		ses.reply_queue_head = new_item;
	}
	ses.reply_queue_tail = new_item;
}

void maybe_flush_reply_queue() {
	struct packetlist *tmp_item = NULL, *curr_item = NULL;
	if (!ses.dataallowed)
	{
		TRACE(("maybe_empty_reply_queue - no data allowed"))
		return;
	}
		
	for (curr_item = ses.reply_queue_head; curr_item; ) {
		CHECKCLEARTOWRITE();
		buf_putbytes(ses.writepayload,
			curr_item->payload->data, curr_item->payload->len);
			
		buf_free(curr_item->payload);
		tmp_item = curr_item;
		curr_item = curr_item->next;
		m_free(tmp_item);
		encrypt_packet();
	}
	ses.reply_queue_head = ses.reply_queue_tail = NULL;
}
	
/* encrypt the writepayload, putting into writebuf, ready for write_packet()
 * to put on the wire */
void encrypt_packet() {

	unsigned char padlen;
	unsigned char blocksize, mac_size;
	buffer * writebuf; /* the packet which will go on the wire. This is 
	                      encrypted in-place. */
	unsigned char packet_type;
	unsigned int len, encrypt_buf_size;
	unsigned char mac_bytes[MAX_MAC_LEN];
	
	TRACE2(("enter encrypt_packet()"))

	buf_setpos(ses.writepayload, 0);
	packet_type = buf_getbyte(ses.writepayload);
	buf_setpos(ses.writepayload, 0);

	TRACE2(("encrypt_packet type is %d", packet_type))
	
	if ((!ses.dataallowed && !packet_is_okay_kex(packet_type))) {
		/* During key exchange only particular packets are allowed.
			Since this packet_type isn't OK we just enqueue it to send 
			after the KEX, see maybe_flush_reply_queue */
		enqueue_reply_packet();
		return;
	}
		
	blocksize = ses.keys->trans.algo_crypt->blocksize;
	mac_size = ses.keys->trans.algo_mac->hashsize;

	/* Encrypted packet len is payload+5. We need to then make sure
	 * there is enough space for padding or MIN_PACKET_LEN. 
	 * Add extra 3 since we need at least 4 bytes of padding */
	encrypt_buf_size = (ses.writepayload->len+4+1) 
		+ MAX(MIN_PACKET_LEN, blocksize) + 3
	/* add space for the MAC at the end */
				+ mac_size
#ifndef DISABLE_ZLIB
	/* some extra in case 'compression' makes it larger */
				+ ZLIB_COMPRESS_INCR
#endif
	/* and an extra cleartext (stripped before transmission) byte for the
	 * packet type */
				+ 1;

	writebuf = buf_new(encrypt_buf_size);
	buf_setlen(writebuf, PACKET_PAYLOAD_OFF);
	buf_setpos(writebuf, PACKET_PAYLOAD_OFF);

#ifndef DISABLE_ZLIB
	/* compression */
	if (is_compress_trans()) {
		int compress_delta;
		buf_compress(writebuf, ses.writepayload, ses.writepayload->len);
		compress_delta = (writebuf->len - PACKET_PAYLOAD_OFF) - ses.writepayload->len;

		/* Handle the case where 'compress' increased the size. */
		if (compress_delta > ZLIB_COMPRESS_INCR) {
			buf_resize(writebuf, writebuf->size + compress_delta);
		}
	} else
#endif
	{
		memcpy(buf_getwriteptr(writebuf, ses.writepayload->len),
				buf_getptr(ses.writepayload, ses.writepayload->len),
				ses.writepayload->len);
		buf_incrwritepos(writebuf, ses.writepayload->len);
	}

	/* finished with payload */
	buf_setpos(ses.writepayload, 0);
	buf_setlen(ses.writepayload, 0);

	/* length of padding - packet length must be a multiple of blocksize,
	 * with a minimum of 4 bytes of padding */
	padlen = blocksize - (writebuf->len) % blocksize;
	if (padlen < 4) {
		padlen += blocksize;
	}
	/* check for min packet length */
	if (writebuf->len + padlen < MIN_PACKET_LEN) {
		padlen += blocksize;
	}

	buf_setpos(writebuf, 0);
	/* packet length excluding the packetlength uint32 */
	buf_putint(writebuf, writebuf->len + padlen - 4);

	/* padding len */
	buf_putbyte(writebuf, padlen);
	/* actual padding */
	buf_setpos(writebuf, writebuf->len);
	buf_incrlen(writebuf, padlen);
	genrandom(buf_getptr(writebuf, padlen), padlen);

	make_mac(ses.transseq, &ses.keys->trans, writebuf, writebuf->len, mac_bytes);

	/* do the actual encryption, in-place */
	buf_setpos(writebuf, 0);
	/* encrypt it in-place*/
	len = writebuf->len;
	if (ses.keys->trans.crypt_mode->encrypt(
				buf_getptr(writebuf, len),
				buf_getwriteptr(writebuf, len),
				len,
				&ses.keys->trans.cipher_state) != CRYPT_OK) {
		dropbear_exit("Error encrypting");
	}
	buf_incrpos(writebuf, len);

    /* stick the MAC on it */
    buf_putbytes(writebuf, mac_bytes, mac_size);

	/* The last byte of the buffer stores the cleartext packet_type. It is not
	 * transmitted but is used for transmit timeout purposes */
	buf_putbyte(writebuf, packet_type);
	/* enqueue the packet for sending. It will get freed after transmission. */
	buf_setpos(writebuf, 0);
	enqueue(&ses.writequeue, (void*)writebuf);

	/* Update counts */
	ses.kexstate.datatrans += writebuf->len;
	ses.transseq++;

	TRACE2(("leave encrypt_packet()"))
}


/* Create the packet mac, and append H(seqno|clearbuf) to the output */
/* output_mac must have ses.keys->trans.algo_mac->hashsize bytes. */
static void make_mac(unsigned int seqno, const struct key_context_directional * key_state,
		buffer * clear_buf, unsigned int clear_len, 
		unsigned char *output_mac) {
	unsigned char seqbuf[4];
	unsigned long bufsize;
	hmac_state hmac;

	if (key_state->algo_mac->hashsize > 0) {
		/* calculate the mac */
		if (hmac_init(&hmac, 
					key_state->hash_index,
					key_state->mackey,
					key_state->algo_mac->keysize) != CRYPT_OK) {
			dropbear_exit("HMAC error");
		}
	
		/* sequence number */
		STORE32H(seqno, seqbuf);
		if (hmac_process(&hmac, seqbuf, 4) != CRYPT_OK) {
			dropbear_exit("HMAC error");
		}
	
		/* the actual contents */
		buf_setpos(clear_buf, 0);
		if (hmac_process(&hmac, 
					buf_getptr(clear_buf, clear_len),
					clear_len) != CRYPT_OK) {
			dropbear_exit("HMAC error");
		}
	
        bufsize = MAX_MAC_LEN;
		if (hmac_done(&hmac, output_mac, &bufsize) != CRYPT_OK) {
			dropbear_exit("HMAC error");
		}
	}
	TRACE2(("leave writemac"))
}

#ifndef DISABLE_ZLIB
/* compresses len bytes from src, outputting to dest (starting from the
 * respective current positions. */
static void buf_compress(buffer * dest, buffer * src, unsigned int len) {

	unsigned int endpos = src->pos + len;
	int result;

	TRACE2(("enter buf_compress"))

	while (1) {

		ses.keys->trans.zstream->avail_in = endpos - src->pos;
		ses.keys->trans.zstream->next_in = 
			buf_getptr(src, ses.keys->trans.zstream->avail_in);

		ses.keys->trans.zstream->avail_out = dest->size - dest->pos;
		ses.keys->trans.zstream->next_out =
			buf_getwriteptr(dest, ses.keys->trans.zstream->avail_out);

		result = deflate(ses.keys->trans.zstream, Z_SYNC_FLUSH);

		buf_setpos(src, endpos - ses.keys->trans.zstream->avail_in);
		buf_setlen(dest, dest->size - ses.keys->trans.zstream->avail_out);
		buf_setpos(dest, dest->len);

		if (result != Z_OK) {
			dropbear_exit("zlib error");
		}

		if (ses.keys->trans.zstream->avail_in == 0) {
			break;
		}

		dropbear_assert(ses.keys->trans.zstream->avail_out == 0);

		/* the buffer has been filled, we must extend. This only happens in
		 * unusual circumstances where the data grows in size after deflate(),
		 * but it is possible */
		buf_resize(dest, dest->size + ZLIB_COMPRESS_INCR);

	}
	TRACE2(("leave buf_compress"))
}
#endif
