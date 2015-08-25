
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       packing.h

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #ifndef __PACKING_H
   #define __PACKING_H

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
   );

   void packcombine(
       int *index,                 /* (i/o) the msb value in the
                                          combined value out */
       int rest,                   /* (i) the lsb value */
       int bitno_rest              /* (i) the number of bits in the
                                          lsb part */
   );

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
   );





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
   );

   #endif

