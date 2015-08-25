#include "UDPBlast.h"

CUDPBlast::CUDPBlast()
{
    m_dCWndSize= 83333.0; 
}

#if 0
CUDPBlast::~CUDPBlast()
{
 
}
#endif

void CUDPBlast::setRate(int mbps)
{
    m_dPktSndPeriod = (m_iSMSS * 8.0) / mbps;
}

