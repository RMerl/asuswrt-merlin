/*
 * Copyright (c) 2013 Quantenna Communications, Inc.
 */

#ifndef _QTN_WMM_AC_H
#define _QTN_WMM_AC_H

#define WMM_AC_BE	0
#define WMM_AC_BK	1
#define WMM_AC_VI	2
#define WMM_AC_VO	3
#define WMM_AC_NUM	4
#define WMM_AC_INVALID	WMM_AC_NUM

#define WMM_AC_TO_TID(_ac) (		\
	(_ac == WMM_AC_VO) ? 6 :	\
	(_ac == WMM_AC_VI) ? 5 :	\
	(_ac == WMM_AC_BK) ? 1 :	\
	0)

#define QTN_AC_MGMT	WMM_AC_VO

#define TID_TO_WMM_AC(_tid) (		\
	((_tid & 7) < 1) ? WMM_AC_BE :	\
	((_tid & 7) < 3) ? WMM_AC_BK :	\
	((_tid & 7) < 6) ? WMM_AC_VI :	\
	WMM_AC_VO)

#define AC_TO_QTN_QNUM(_ac)		\
	(((_ac) == WME_AC_BE) ? 1 :	\
	 ((_ac) == WME_AC_BK) ? 0 :	\
	  (_ac))

#define WLAN_MGMT_TID	7

#define WLAN_TX_TIDS	{ 7, 6, 5, 0, 1 }

#define QTN_AC_ORDER	{ WMM_AC_VO, WMM_AC_VI, WMM_AC_BE, WMM_AC_BK }

#endif	/* _QTN_WMM_AC_H */

