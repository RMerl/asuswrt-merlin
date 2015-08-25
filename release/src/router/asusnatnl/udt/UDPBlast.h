#ifndef __UDPBLAST_H__
#define __UDPBLAST_H__

#include "ccc.h"

class CUDPBlast: public CCC 
{
public:
   CUDPBlast();

public:
   void setRate(int mbps);

protected:
   static const int m_iSMSS = 1500;
};


#endif //__UDPBLAST_H__