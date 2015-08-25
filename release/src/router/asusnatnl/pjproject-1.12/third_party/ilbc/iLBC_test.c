
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       iLBC_test.c

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #include <math.h>
   #include <stdlib.h>
   #include <stdio.h>
   #include <string.h>
   #include "iLBC_define.h"
   #include "iLBC_encode.h"
   #include "iLBC_decode.h"

   /* Runtime statistics */
   #include <time.h>

   #define ILBCNOOFWORDS_MAX   (NO_OF_BYTES_30MS/2)

   /*----------------------------------------------------------------*
    *  Encoder interface function





    *---------------------------------------------------------------*/

   short encode(   /* (o) Number of bytes encoded */
       iLBC_Enc_Inst_t *iLBCenc_inst,
                                   /* (i/o) Encoder instance */
       short *encoded_data,    /* (o) The encoded bytes */
       short *data                 /* (i) The signal block to encode*/
   ){
       float block[BLOCKL_MAX];
       int k;

       /* convert signal to float */

       for (k=0; k<iLBCenc_inst->blockl; k++)
           block[k] = (float)data[k];

       /* do the actual encoding */

       iLBC_encode((unsigned char *)encoded_data, block, iLBCenc_inst);


       return (iLBCenc_inst->no_of_bytes);
   }

   /*----------------------------------------------------------------*
    *  Decoder interface function
    *---------------------------------------------------------------*/

   short decode(       /* (o) Number of decoded samples */
       iLBC_Dec_Inst_t *iLBCdec_inst,  /* (i/o) Decoder instance */
       short *decoded_data,        /* (o) Decoded signal block*/
       short *encoded_data,        /* (i) Encoded bytes */
       short mode                       /* (i) 0=PL, 1=Normal */
   ){
       int k;
       float decblock[BLOCKL_MAX], dtmp;

       /* check if mode is valid */

       if (mode<0 || mode>1) {
           printf("\nERROR - Wrong mode - 0, 1 allowed\n"); exit(3);}

       /* do actual decoding of block */

       iLBC_decode(decblock, (unsigned char *)encoded_data,
           iLBCdec_inst, mode);

       /* convert to short */





       for (k=0; k<iLBCdec_inst->blockl; k++){
           dtmp=decblock[k];

           if (dtmp<MIN_SAMPLE)
               dtmp=MIN_SAMPLE;
           else if (dtmp>MAX_SAMPLE)
               dtmp=MAX_SAMPLE;
           decoded_data[k] = (short) dtmp;
       }

       return (iLBCdec_inst->blockl);
   }

   /*---------------------------------------------------------------*
    *  Main program to test iLBC encoding and decoding
    *
    *  Usage:
    *    exefile_name.exe <infile> <bytefile> <outfile> <channel>
    *
    *    <infile>   : Input file, speech for encoder (16-bit pcm file)
    *    <bytefile> : Bit stream output from the encoder
    *    <outfile>  : Output file, decoded speech (16-bit pcm file)
    *    <channel>  : Bit error file, optional (16-bit)
    *                     1 - Packet received correctly
    *                     0 - Packet Lost
    *
    *--------------------------------------------------------------*/

   int main(int argc, char* argv[])
   {

       /* Runtime statistics */

       float starttime;
       float runtime;
       float outtime;

       FILE *ifileid,*efileid,*ofileid, *cfileid;
       short data[BLOCKL_MAX];
       short encoded_data[ILBCNOOFWORDS_MAX], decoded_data[BLOCKL_MAX];
       int len;
       short pli, mode;
       int blockcount = 0;
       int packetlosscount = 0;

       /* Create structs */
       iLBC_Enc_Inst_t Enc_Inst;
       iLBC_Dec_Inst_t Dec_Inst;





       /* get arguments and open files */

       if ((argc!=5) && (argc!=6)) {
           fprintf(stderr,
           "\n*-----------------------------------------------*\n");
           fprintf(stderr,
           "   %s <20,30> input encoded decoded (channel)\n\n",
               argv[0]);
           fprintf(stderr,
           "   mode    : Frame size for the encoding/decoding\n");
           fprintf(stderr,
           "                 20 - 20 ms\n");
           fprintf(stderr,
           "                 30 - 30 ms\n");
           fprintf(stderr,
           "   input   : Speech for encoder (16-bit pcm file)\n");
           fprintf(stderr,
           "   encoded : Encoded bit stream\n");
           fprintf(stderr,
           "   decoded : Decoded speech (16-bit pcm file)\n");
           fprintf(stderr,
           "   channel : Packet loss pattern, optional (16-bit)\n");
           fprintf(stderr,
           "                  1 - Packet received correctly\n");
           fprintf(stderr,
           "                  0 - Packet Lost\n");
           fprintf(stderr,
           "*-----------------------------------------------*\n\n");
           exit(1);
       }
       mode=atoi(argv[1]);
       if (mode != 20 && mode != 30) {
           fprintf(stderr,"Wrong mode %s, must be 20, or 30\n",
               argv[1]);
           exit(2);
       }
       if ( (ifileid=fopen(argv[2],"rb")) == NULL) {
           fprintf(stderr,"Cannot open input file %s\n", argv[2]);
           exit(2);}
       if ( (efileid=fopen(argv[3],"wb")) == NULL) {
           fprintf(stderr, "Cannot open encoded file %s\n",
               argv[3]); exit(1);}
       if ( (ofileid=fopen(argv[4],"wb")) == NULL) {
           fprintf(stderr, "Cannot open decoded file %s\n",
               argv[4]); exit(1);}
       if (argc==6) {
           if( (cfileid=fopen(argv[5],"rb")) == NULL) {
               fprintf(stderr, "Cannot open channel file %s\n",





                   argv[5]);
               exit(1);
           }
       } else {
           cfileid=NULL;
       }

       /* print info */

       fprintf(stderr, "\n");
       fprintf(stderr,
           "*---------------------------------------------------*\n");
       fprintf(stderr,
           "*                                                   *\n");
       fprintf(stderr,
           "*      iLBC test program                            *\n");
       fprintf(stderr,
           "*                                                   *\n");
       fprintf(stderr,
           "*                                                   *\n");
       fprintf(stderr,
           "*---------------------------------------------------*\n");
       fprintf(stderr,"\nMode           : %2d ms\n", mode);
       fprintf(stderr,"Input file     : %s\n", argv[2]);
       fprintf(stderr,"Encoded file   : %s\n", argv[3]);
       fprintf(stderr,"Output file    : %s\n", argv[4]);
       if (argc==6) {
           fprintf(stderr,"Channel file   : %s\n", argv[5]);
       }
       fprintf(stderr,"\n");

       /* Initialization */

       initEncode(&Enc_Inst, mode);
       initDecode(&Dec_Inst, mode, 1);

       /* Runtime statistics */

       starttime=clock()/(float)CLOCKS_PER_SEC;

       /* loop over input blocks */

       while (fread(data,sizeof(short),Enc_Inst.blockl,ifileid)==
               (size_t)Enc_Inst.blockl) {

           blockcount++;

           /* encoding */





           fprintf(stderr, "--- Encoding block %i --- ",blockcount);
           len=encode(&Enc_Inst, encoded_data, data);
           fprintf(stderr, "\r");

           /* write byte file */

           fwrite(encoded_data, sizeof(unsigned char), len, efileid);

           /* get channel data if provided */
           if (argc==6) {
               if (fread(&pli, sizeof(short), 1, cfileid)) {
                   if ((pli!=0)&&(pli!=1)) {
                       fprintf(stderr, "Error in channel file\n");
                       exit(0);
                   }
                   if (pli==0) {
                       /* Packet loss -> remove info from frame */
                       memset(encoded_data, 0,
                           sizeof(short)*ILBCNOOFWORDS_MAX);
                       packetlosscount++;
                   }
               } else {
                   fprintf(stderr, "Error. Channel file too short\n");
                   exit(0);
               }
           } else {
               pli=1;
           }

           /* decoding */

           fprintf(stderr, "--- Decoding block %i --- ",blockcount);

           len=decode(&Dec_Inst, decoded_data, encoded_data, pli);
           fprintf(stderr, "\r");

           /* write output file */

           fwrite(decoded_data,sizeof(short),len,ofileid);
       }

       /* Runtime statistics */

       runtime = (float)(clock()/(float)CLOCKS_PER_SEC-starttime);
       outtime = (float)((float)blockcount*(float)mode/1000.0);
       printf("\n\nLength of speech file: %.1f s\n", outtime);
       printf("Packet loss          : %.1f%%\n",
           100.0*(float)packetlosscount/(float)blockcount);





       printf("Time to run iLBC     :");
       printf(" %.1f s (%.1f %% of realtime)\n\n", runtime,
           (100*runtime/outtime));

       /* close files */

       fclose(ifileid);  fclose(efileid); fclose(ofileid);
       if (argc==6) {
           fclose(cfileid);
       }
       return(0);
   }

