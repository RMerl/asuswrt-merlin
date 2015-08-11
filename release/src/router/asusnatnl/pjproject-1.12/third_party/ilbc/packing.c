
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       packing.c

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #include <math.h>
   #include <stdlib.h>

   #include "iLBC_define.h"
   #include "constants.h"
   #include "helpfun.h"
   #include "string.h"

   /*----------------------------------------------------------------*
    *  splitting an integer into first most significant bits and
    *  remaining least significant bits
    *---------------------------------------------------------------*/

   void packsplit(
       int *index,                 /* (i) the value to split */
       int *firstpart,             /* (o) the value specified by most
                                          significant bits */
       int *rest,                  /* (o) the value specified by least
                                          significant bits */





       int bitno_firstpart,    /* (i) number of bits in most
                                          significant part */
       int bitno_total             /* (i) number of bits in full range
                                          of value */
   ){
       int bitno_rest = bitno_total-bitno_firstpart;

       *firstpart = *index>>(bitno_rest);
       *rest = *index-(*firstpart<<(bitno_rest));
   }

   /*----------------------------------------------------------------*
    *  combining a value corresponding to msb's with a value
    *  corresponding to lsb's
    *---------------------------------------------------------------*/

   void packcombine(
       int *index,                 /* (i/o) the msb value in the
                                          combined value out */
       int rest,                   /* (i) the lsb value */
       int bitno_rest              /* (i) the number of bits in the
                                          lsb part */
   ){
       *index = *index<<bitno_rest;
       *index += rest;
   }

   /*----------------------------------------------------------------*
    *  packing of bits into bitstream, i.e., vector of bytes
    *---------------------------------------------------------------*/

   void dopack(
       unsigned char **bitstream,  /* (i/o) on entrance pointer to
                                          place in bitstream to pack
                                          new data, on exit pointer
                                          to place in bitstream to
                                          pack future data */
       int index,                  /* (i) the value to pack */
       int bitno,                  /* (i) the number of bits that the
                                          value will fit within */
       int *pos                /* (i/o) write position in the
                                          current byte */
   ){
       int posLeft;

       /* Clear the bits before starting in a new byte */

       if ((*pos)==0) {





           **bitstream=0;
       }

       while (bitno>0) {

           /* Jump to the next byte if end of this byte is reached*/

           if (*pos==8) {
               *pos=0;
               (*bitstream)++;
               **bitstream=0;
           }

           posLeft=8-(*pos);

           /* Insert index into the bitstream */

           if (bitno <= posLeft) {
               **bitstream |= (unsigned char)(index<<(posLeft-bitno));
               *pos+=bitno;
               bitno=0;
           } else {
               **bitstream |= (unsigned char)(index>>(bitno-posLeft));

               *pos=8;
               index-=((index>>(bitno-posLeft))<<(bitno-posLeft));

               bitno-=posLeft;
           }
       }
   }

   /*----------------------------------------------------------------*
    *  unpacking of bits from bitstream, i.e., vector of bytes
    *---------------------------------------------------------------*/

   void unpack(
       unsigned char **bitstream,  /* (i/o) on entrance pointer to
                                          place in bitstream to
                                          unpack new data from, on
                                          exit pointer to place in
                                          bitstream to unpack future
                                          data from */
       int *index,                 /* (o) resulting value */
       int bitno,                  /* (i) number of bits used to
                                          represent the value */
       int *pos                /* (i/o) read position in the
                                          current byte */





   ){
       int BitsLeft;

       *index=0;

       while (bitno>0) {

           /* move forward in bitstream when the end of the
              byte is reached */

           if (*pos==8) {
               *pos=0;
               (*bitstream)++;
           }

           BitsLeft=8-(*pos);

           /* Extract bits to index */

           if (BitsLeft>=bitno) {
               *index+=((((**bitstream)<<(*pos)) & 0xFF)>>(8-bitno));

               *pos+=bitno;
               bitno=0;
           } else {

               if ((8-bitno)>0) {
                   *index+=((((**bitstream)<<(*pos)) & 0xFF)>>
                       (8-bitno));
                   *pos=8;
               } else {
                   *index+=(((int)(((**bitstream)<<(*pos)) & 0xFF))<<
                       (bitno-8));
                   *pos=8;
               }
               bitno-=BitsLeft;
           }
       }
   }

