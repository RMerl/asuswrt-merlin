#ifndef _TC_CBQ_H_
#define _TC_CBQ_H_ 1

unsigned tc_cbq_calc_maxidle(unsigned bndw, unsigned rate, unsigned avpkt,
			     int ewma_log, unsigned maxburst);
unsigned tc_cbq_calc_offtime(unsigned bndw, unsigned rate, unsigned avpkt,
			     int ewma_log, unsigned minburst);

#endif
