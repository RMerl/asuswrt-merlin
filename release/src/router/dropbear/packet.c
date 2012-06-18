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

static void read_packet_init();
static void writemac(buffer * outputbuffer, buffer * clearwritebuf);
static int checkmac(buffer* hashbuf, buffer* readbuf);

#define ZLIB_COMPRESS_INCR 20 /* this is 12 bytes + 0.1% of 8000 bytes */
#define ZLIB_DECOMPRESS_INCR 100
#ifndef DISABLE_ZLIB
static buffer* buf_decompress(buffer* buf, unsigned int len);
static void buf_compress(buffer * dest, buffer * src, unsigned int len);
#endif

/* non-blocking function writing out a current encrypted packet */
void write_packet() {

	int len, written;
	buffer * writebuf = NULL;
	
	TRACE(("enter write_packet"))
	dropbear_assert(!isempty(&ses.writequeue));

	/* Get the next buffer in the queue of encrypted packets to write*/
	writebuf = (buffer*)examine(&ses.writequeue);

	len = writebuf->len - writebuf->pos;
	dropbear_assert(len > 0);
	/* Try to write as much as possible */
	written = write(ses.sock_out, buf_getptr(writebuf, len), len);

	if (written < 0) {
		if (errno == EINTR) {
			TRACE(("leave writepacket: EINTR"))
			return;
		} else {
			dropbear_exit("error writing");
		}
	} 
	
	ses.last_trx_packet_time = time(NULL);
	ses.last_packet_time = time(NULL);

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

	TRACE(("leave write_packet"))
}

/* Non-blocking function reading available portion of a packet into the
 * ses's buffer, decrypting the length if encrypted, decrypting the
 * full portion if possible */
void read_packet() {

	int len;
	unsigned int maxlen;
	unsigned char blocksize;

	TRACE(("enter read_packet"))
	blocksize = ses.keys->recv_algo_crypt->blocksize;
	
	if (ses.readbuf == NULL || ses.readbuf->len < blocksize) {
		/* In the first blocksize of a packet */

		/* Read the first blocksize of the packet, so we can decrypt it and
		 * find the length of the whole packet */
		read_packet_init();

		/* If we don't have the length of decryptreadbuf, we didn't read
		 * a whole blocksize and should exit */
		if (ses.decryptreadbuf->len == 0) {
			TRACE(("leave read_packet: packetinit done"))
			return;
		}
	}

	/* Attempt to read the remainder of the packet, note that there
	 * mightn't be any available (EAGAIN) */
	dropbear_assert(ses.readbuf != NULL);
	maxlen = ses.readbuf->len - ses.readbuf->pos;
	len = read(ses.sock_in, buf_getptr(ses.readbuf, maxlen), maxlen);

	if (len == 0) {
		ses.remoteclosed();
	}

	if (len < 0) {
		if (errno == EINTR || errno == EAGAIN) {
			TRACE(("leave read_packet: EINTR or EAGAIN"))
			return;
		} else {
			dropbear_exit("error reading: %s", strerror(errno));
		}
	}

	buf_incrpos(ses.readbuf, len);

	if ((unsigned int)len == maxlen) {
		/* The whole packet has been read */
		decrypt_packet();
		/* The main select() loop process_packet() to
		 * handle the packet contents... */
	}
	TRACE(("leave read_packet"))
}

/* Function used to read the initial portion of a packet, and determine the
 * length. Only called during the first BLOCKSIZE of a packet. */
static void read_packet_init() {

	unsigned int maxlen;
	int len;
	unsigned char blocksize;
	unsigned char macsize;


	blocksize = ses.keys->recv_algo_crypt->blocksize;
	macsize = ses.keys->recv_algo_mac->hashsize;

	if (ses.readbuf == NULL) {
		/* start of a new packet */
		ses.readbuf = buf_new(INIT_READBUF);
		dropbear_assert(ses.decryptreadbuf == NULL);
		ses.decryptreadbuf = buf_new(blocksize);
	}

	maxlen = blocksize - ses.readbuf->pos;
			
	/* read the rest of the packet if possible */
	len = read(ses.sock_in, buf_getwriteptr(ses.readbuf, maxlen),
			maxlen);
	if (len == 0) {
		ses.remoteclosed();
	}
	if (len < 0) {
		if (errno == EINTR) {
			TRACE(("leave read_packet_init: EINTR"))
			return;
		}
		dropbear_exit("error reading: %s", strerror(errno));
	}

	buf_incrwritepos(ses.readbuf, len);

	if ((unsigned int)len != maxlen) {
		/* don't have enough bytes to determine length, get next time */
		return;
	}

	/* now we have the first block, need to get packet length, so we decrypt
	 * the first block (only need first 4 bytes) */
	buf_setpos(ses.readbuf, 0);
	if (ses.keys->recv_crypt_mode->decrypt(buf_getptr(ses.readbuf, blocksize), 
				buf_getwriteptr(ses.decryptreadbuf,blocksize),
				blocksize,
				&ses.keys->recv_cipher_state) != CRYPT_OK) {
		dropbear_exit("error decrypting");
	}
	buf_setlen(ses.decryptreadbuf, blocksize);
	len = buf_getint(ses.decryptreadbuf) + 4 + macsize;

	buf_setpos(ses.readbuf, blocksize);

	/* check packet length */
	if ((len > RECV_MAX_PACKET_LEN) ||
		(len < MIN_PACKET_LEN + macsize) ||
		((len - macsize) % blocksize != 0)) {
		dropbear_exit("bad packet size %d", len);
	}

	buf_resize(ses.readbuf, len);
	buf_setlen(ses.readbuf, len);

}

/* handle the received packet */
void decrypt_packet() {

	unsigned char blocksize;
	unsigned char macsize;
	unsigned int padlen;
	unsigned int len;

	TRACE(("enter decrypt_packet"))
	blocksize = ses.keys->recv_algo_crypt->blocksize;
	macsize = ses.keys->recv_algo_mac->hashsize;

	ses.kexstate.datarecv += ses.readbuf->len;

	/* we've already decrypted the first blocksize in read_packet_init */
	buf_setpos(ses.readbuf, blocksize);

	buf_resize(ses.decryptreadbuf, ses.readbuf->len - macsize);
	buf_setlen(ses.decryptreadbuf, ses.decryptreadbuf->size);
	buf_setpos(ses.decryptreadbuf, blocksize);

	/* decrypt it */
	while (ses.readbuf->pos < ses.readbuf->len - macsize) {
		if (ses.keys->recv_crypt_mode->decrypt(
					buf_getptr(ses.readbuf, blocksize), 
					buf_getwriteptr(ses.decryptreadbuf, blocksize),
					blocksize,
					&ses.keys->recv_cipher_state) != CRYPT_OK) {
			dropbear_exit("error decrypting");
		}
		buf_incrpos(ses.readbuf, blocksize);
		buf_incrwritepos(ses.decryptreadbuf, blocksize);
	}

	/* check the hmac */
	buf_setpos(ses.readbuf, ses.readbuf->len - macsize);
	if (checkmac(ses.readbuf, ses.decryptreadbuf) != DROPBEAR_SUCCESS) {
		dropbear_exit("Integrity error");
	}

	/* readbuf no longer required */
	buf_free(ses.readbuf);
	ses.readbuf = NULL;

	/* get padding length */
	buf_setpos(ses.decryptreadbuf, PACKET_PADDING_OFF);
	padlen = buf_getbyte(ses.decryptreadbuf);
		
	/* payload length */
	/* - 4 - 1 is for LEN and PADLEN values */
	len = ses.decryptreadbuf->len - padlen - 4 - 1;
	if ((len > RECV_MAX_PAYLOAD_LEN) || (len < 1)) {
		dropbear_exit("bad packet size");
	}

	buf_setpos(ses.decryptreadbuf, PACKET_PAYLOAD_OFF);

#ifndef DISABLE_ZLIB
	if (is_compress_recv()) {
		/* decompress */
		ses.payload = buf_decompress(ses.decryptreadbuf, len);
	} else 
#endif
	{
		/* copy payload */
		ses.payload = buf_new(len);
		memcpy(ses.payload->data, buf_getptr(ses.decryptreadbuf, len), len);
		buf_incrlen(ses.payload, len);
	}

	buf_free(ses.decryptreadbuf);
	ses.decryptreadbuf = NULL;
	buf_setpos(ses.payload, 0);

	ses.recvseq++;

	TRACE(("leave decrypt_packet"))
}

/* Checks the mac in hashbuf, for the data in readbuf.
 * Returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
static int checkmac(buffer* macbuf, buffer* sourcebuf) {

	unsigned int macsize;
	hmac_state hmac;
	unsigned char tempbuf[MAX_MAC_LEN];
	unsigned long bufsize;
	unsigned int len;

	macsize = ses.keys->recv_algo_mac->hashsize;
	if (macsize == 0) {
		return DROPBEAR_SUCCESS;
	}

	/* calculate the mac */
	if (hmac_init(&hmac, 
				find_hash(ses.keys->recv_algo_mac->hashdesc->name), 
				ses.keys->recvmackey, 
				ses.keys->recv_algo_mac->keysize) 
				!= CRYPT_OK) {
		dropbear_exit("HMAC error");
	}
	
	/* sequence number */
	STORE32H(ses.recvseq, tempbuf);
	if (hmac_process(&hmac, tempbuf, 4) != CRYPT_OK) {
		dropbear_exit("HMAC error");
	}

	buf_setpos(sourcebuf, 0);
	len = sourcebuf->len;
	if (hmac_process(&hmac, buf_getptr(sourcebuf, len), len) != CRYPT_OK) {
		dropbear_exit("HMAC error");
	}

	bufsize = sizeof(tempbuf);
	if (hmac_done(&hmac, tempbuf, &bufsize) != CRYPT_OK) {
		dropbear_exit("HMAC error");
	}

	/* compare the hash */
	if (memcmp(tempbuf, buf_getptr(macbuf, macsize), macsize) != 0) {
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

	zstream = ses.keys->recv_zstream;
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
	TRACE(("leave enqueue_reply_packet"))
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
	unsigned char blocksize, macsize;
	buffer * writebuf; /* the packet which will go on the wire */
	buffer * clearwritebuf; /* unencrypted, possibly compressed */
	unsigned char type;
	unsigned int clear_len;
	
	type = ses.writepayload->data[0];
	TRACE(("enter encrypt_packet()"))
	TRACE(("encrypt_packet type is %d", type))
	
	if (!ses.dataallowed && !packet_is_okay_kex(type)) {
		/* During key exchange only particular packets are allowed.
			Since this type isn't OK we just enqueue it to send 
			after the KEX, see maybe_flush_reply_queue */
		enqueue_reply_packet();
		return;
	}
		
	blocksize = ses.keys->trans_algo_crypt->blocksize;
	macsize = ses.keys->trans_algo_mac->hashsize;

	/* Encrypted packet len is payload+5, then worst case is if we are 3 away
	 * from a blocksize multiple. In which case we need to pad to the
	 * multiple, then add another blocksize (or MIN_PACKET_LEN) */
	clear_len = (ses.writepayload->len+4+1) + MIN_PACKET_LEN + 3;

#ifndef DISABLE_ZLIB
	clear_len += ZLIB_COMPRESS_INCR; /* bit of a kludge, but we can't know len*/
#endif
	clearwritebuf = buf_new(clear_len);
	buf_setlen(clearwritebuf, PACKET_PAYLOAD_OFF);
	buf_setpos(clearwritebuf, PACKET_PAYLOAD_OFF);

	buf_setpos(ses.writepayload, 0);

#ifndef DISABLE_ZLIB
	/* compression */
	if (is_compress_trans()) {
		buf_compress(clearwritebuf, ses.writepayload, ses.writepayload->len);
	} else
#endif
	{
		memcpy(buf_getwriteptr(clearwritebuf, ses.writepayload->len),
				buf_getptr(ses.writepayload, ses.writepayload->len),
				ses.writepayload->len);
		buf_incrwritepos(clearwritebuf, ses.writepayload->len);
	}

	/* finished with payload */
	buf_setpos(ses.writepayload, 0);
	buf_setlen(ses.writepayload, 0);

	/* length of padding - packet length must be a multiple of blocksize,
	 * with a minimum of 4 bytes of padding */
	padlen = blocksize - (clearwritebuf->len) % blocksize;
	if (padlen < 4) {
		padlen += blocksize;
	}
	/* check for min packet length */
	if (clearwritebuf->len + padlen < MIN_PACKET_LEN) {
		padlen += blocksize;
	}

	buf_setpos(clearwritebuf, 0);
	/* packet length excluding the packetlength uint32 */
	buf_putint(clearwritebuf, clearwritebuf->len + padlen - 4);

	/* padding len */
	buf_putbyte(clearwritebuf, padlen);
	/* actual padding */
	buf_setpos(clearwritebuf, clearwritebuf->len);
	buf_incrlen(clearwritebuf, padlen);
	genrandom(buf_getptr(clearwritebuf, padlen), padlen);

	/* do the actual encryption */
	buf_setpos(clearwritebuf, 0);
	/* create a new writebuffer, this is freed when it has been put on the 
	 * wire by writepacket() */
	writebuf = buf_new(clearwritebuf->len + macsize);

	/* encrypt it */
	while (clearwritebuf->pos < clearwritebuf->len) {
		if (ses.keys->trans_crypt_mode->encrypt(
					buf_getptr(clearwritebuf, blocksize),
					buf_getwriteptr(writebuf, blocksize),
					blocksize,
					&ses.keys->trans_cipher_state) != CRYPT_OK) {
			dropbear_exit("error encrypting");
		}
		buf_incrpos(clearwritebuf, blocksize);
		buf_incrwritepos(writebuf, blocksize);
	}

	/* now add a hmac and we're done */
	writemac(writebuf, clearwritebuf);

	/* clearwritebuf is finished with */
	buf_free(clearwritebuf);
	clearwritebuf = NULL;

	/* enqueue the packet for sending */
	buf_setpos(writebuf, 0);
	enqueue(&ses.writequeue, (void*)writebuf);

	/* Update counts */
	ses.kexstate.datatrans += writebuf->len;
	ses.transseq++;

	TRACE(("leave encrypt_packet()"))
}


/* Create the packet mac, and append H(seqno|clearbuf) to the output */
static void writemac(buffer * outputbuffer, buffer * clearwritebuf) {

	unsigned int macsize;
	unsigned char seqbuf[4];
	unsigned char tempbuf[MAX_MAC_LEN];
	unsigned long bufsize;
	hmac_state hmac;

	TRACE(("enter writemac"))

	macsize = ses.keys->trans_algo_mac->hashsize;
	if (macsize > 0) {
		/* calculate the mac */
		if (hmac_init(&hmac, 
					find_hash(ses.keys->trans_algo_mac->hashdesc->name), 
					ses.keys->transmackey, 
					ses.keys->trans_algo_mac->keysize) != CRYPT_OK) {
			dropbear_exit("HMAC error");
		}
	
		/* sequence number */
		STORE32H(ses.transseq, seqbuf);
		if (hmac_process(&hmac, seqbuf, 4) != CRYPT_OK) {
			dropbear_exit("HMAC error");
		}
	
		/* the actual contents */
		buf_setpos(clearwritebuf, 0);
		if (hmac_process(&hmac, 
					buf_getptr(clearwritebuf, 
						clearwritebuf->len),
					clearwritebuf->len) != CRYPT_OK) {
			dropbear_exit("HMAC error");
		}
	
		bufsize = sizeof(tempbuf);
		if (hmac_done(&hmac, tempbuf, &bufsize) 
				!= CRYPT_OK) {
			dropbear_exit("HMAC error");
		}
		buf_putbytes(outputbuffer, tempbuf, macsize);
	}
	TRACE(("leave writemac"))
}

#ifndef DISABLE_ZLIB
/* compresses len bytes from src, outputting to dest (starting from the
 * respective current positions. */
static void buf_compress(buffer * dest, buffer * src, unsigned int len) {

	unsigned int endpos = src->pos + len;
	int result;

	TRACE(("enter buf_compress"))

	while (1) {

		ses.keys->trans_zstream->avail_in = endpos - src->pos;
		ses.keys->trans_zstream->next_in = 
			buf_getptr(src, ses.keys->trans_zstream->avail_in);

		ses.keys->trans_zstream->avail_out = dest->size - dest->pos;
		ses.keys->trans_zstream->next_out =
			buf_getwriteptr(dest, ses.keys->trans_zstream->avail_out);

		result = deflate(ses.keys->trans_zstream, Z_SYNC_FLUSH);

		buf_setpos(src, endpos - ses.keys->trans_zstream->avail_in);
		buf_setlen(dest, dest->size - ses.keys->trans_zstream->avail_out);
		buf_setpos(dest, dest->len);

		if (result != Z_OK) {
			dropbear_exit("zlib error");
		}

		if (ses.keys->trans_zstream->avail_in == 0) {
			break;
		}

		dropbear_assert(ses.keys->trans_zstream->avail_out == 0);

		/* the buffer has been filled, we must extend. This only happens in
		 * unusual circumstances where the data grows in size after deflate(),
		 * but it is possible */
		buf_resize(dest, dest->size + ZLIB_COMPRESS_INCR);

	}
	TRACE(("leave buf_compress"))
}
#endif
