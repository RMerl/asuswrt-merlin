/*
 * Checksum routine for Internet Protocol family headers (C Version).
 *
 * Refer to "Computing the Internet Checksum" by R. Braden, D. Borman and
 * C. Partridge, Computer Communication Review, Vol. 19, No. 2, April 1989,
 * pp. 86-101, for additional details on computing this checksum.
 */

#include <zebra.h>
#include "checksum.h"

int			/* return checksum in low-order 16 bits */
in_cksum(void *parg, int nbytes)
{
	u_short *ptr = parg;
	register long		sum;		/* assumes long == 32 bits */
	u_short			oddbyte;
	register u_short	answer;		/* assumes u_short == 16 bits */

	/*
	 * Our algorithm is simple, using a 32-bit accumulator (sum),
	 * we add sequential 16-bit words to it, and at the end, fold back
	 * all the carry bits from the top 16 bits into the lower 16 bits.
	 */

	sum = 0;
	while (nbytes > 1)  {
		sum += *ptr++;
		nbytes -= 2;
	}

				/* mop up an odd byte, if necessary */
	if (nbytes == 1) {
		oddbyte = 0;		/* make sure top half is zero */
		*((u_char *) &oddbyte) = *(u_char *)ptr;   /* one byte only */
		sum += oddbyte;
	}

	/*
	 * Add back carry outs from top 16 bits to low 16 bits.
	 */

	sum  = (sum >> 16) + (sum & 0xffff);	/* add high-16 to low-16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;		/* ones-complement, then truncate to 16 bits */
	return(answer);
}

/* Fletcher Checksum -- Refer to RFC1008. */
#define MODX                 4102   /* 5802 should be fine */

/* To be consistent, offset is 0-based index, rather than the 1-based 
   index required in the specification ISO 8473, Annex C.1 */
/* calling with offset == FLETCHER_CHECKSUM_VALIDATE will validate the checksum
   without modifying the buffer; a valid checksum returns 0 */
u_int16_t
fletcher_checksum(u_char * buffer, const size_t len, const uint16_t offset)
{
  u_int8_t *p;
  int x, y, c0, c1;
  u_int16_t checksum;
  u_int16_t *csum;
  size_t partial_len, i, left = len;
  
  checksum = 0;


  if (offset != FLETCHER_CHECKSUM_VALIDATE)
    /* Zero the csum in the packet. */
    {
      assert (offset < (len - 1)); /* account for two bytes of checksum */
      csum = (u_int16_t *) (buffer + offset);
      *(csum) = 0;
    }

  p = buffer;
  c0 = 0;
  c1 = 0;

  while (left != 0)
    {
      partial_len = MIN(left, MODX);

      for (i = 0; i < partial_len; i++)
	{
	  c0 = c0 + *(p++);
	  c1 += c0;
	}

      c0 = c0 % 255;
      c1 = c1 % 255;

      left -= partial_len;
    }

  /* The cast is important, to ensure the mod is taken as a signed value. */
  x = (int)((len - offset - 1) * c0 - c1) % 255;

  if (x <= 0)
    x += 255;
  y = 510 - c0 - x;
  if (y > 255)  
    y -= 255;

  if (offset == FLETCHER_CHECKSUM_VALIDATE)
    {
      checksum = (c1 << 8) + c0;
    }
  else
    {
      /*
       * Now we write this to the packet.
       * We could skip this step too, since the checksum returned would
       * be stored into the checksum field by the caller.
       */
      buffer[offset] = x;
      buffer[offset + 1] = y;

      /* Take care of the endian issue */
      checksum = htons((x << 8) | (y & 0xFF));
    }

  return checksum;
}
