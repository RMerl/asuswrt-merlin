#include <stdint.h>

#define PKT_SIZE 2820

struct image_pkt_hdr {
	uint32_t resend;
	uint32_t totcrc;
	uint32_t nr_blocks;
	uint32_t blocksize;
	uint32_t block_crc;
	uint32_t block_nr;
	uint32_t pkt_sequence;
	uint16_t pkt_nr;
	uint16_t nr_pkts;
	uint32_t thislen;
	uint32_t thiscrc;
};

struct image_pkt {
	struct image_pkt_hdr hdr;
	unsigned char data[PKT_SIZE];
};

struct fec_parms;

/* k - number of actual data packets
 * n - total number of packets including data and redundant packets
 *   (actual packet size isn't relevant here) */
struct fec_parms *fec_new(int k, int n);
void fec_free(struct fec_parms *p);

/* src   - array of (n) pointers to data packets
 * fec   - buffer for packet to be generated
 * index - index of packet to be generated (0 <= index < n)
 * sz    - data packet size
 *
 * _linear version just takes a pointer to the raw data; no
 * mucking about with packet pointers.
 */
void fec_encode(struct fec_parms *code, unsigned char *src[],
		unsigned char *fec, int index, int sz);
void fec_encode_linear(struct fec_parms *code, unsigned char *src,
		       unsigned char *fec, int index, int sz);

/* data  - array of (k) pointers to data packets, in arbitrary order (see i)
 * i     - indices of (data) packets
 * sz    - data packet size
 *
 * Will never fail as long as you give it (k) individual data packets.
 * Will re-order the (data) pointers but not the indices -- data packets
 * are ordered on return.
 */
int fec_decode(struct fec_parms *code, unsigned char *data[],
	       int i[], int sz);
