/******************************************************************************
 * Copyright(c) 2008 - 2010 Realtek Corporation. All rights reserved.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
******************************************************************************/
#ifndef _TSTYPE_H_
#define _TSTYPE_H_
#include "rtl819x_Qos.h"
#define TS_SETUP_TIMEOUT	60  // In millisecond
#define TS_INACT_TIMEOUT	60
#define TS_ADDBA_DELAY		60

#define TOTAL_TS_NUM		16
#define TCLAS_NUM		4

// This define the Tx/Rx directions
typedef enum _TR_SELECT {
	TX_DIR = 0,
	RX_DIR = 1,
} TR_SELECT, *PTR_SELECT;

typedef struct _TS_COMMON_INFO{
	struct list_head		List;
	struct timer_list		SetupTimer;
	struct timer_list		InactTimer;
	u8				Addr[6];
	TSPEC_BODY			TSpec;
	QOS_TCLAS			TClass[TCLAS_NUM];
	u8				TClasProc;
	u8				TClasNum;
} TS_COMMON_INFO, *PTS_COMMON_INFO;

typedef struct _TX_TS_RECORD{
	TS_COMMON_INFO		TsCommonInfo;
	u16				TxCurSeq;
	BA_RECORD			TxPendingBARecord;  	// For BA Originator
	BA_RECORD			TxAdmittedBARecord;	// For BA Originator
	u8				bAddBaReqInProgress;
	u8				bAddBaReqDelayed;
	u8				bUsingBa;
	u8 				bDisable_AddBa;
	struct timer_list		TsAddBaTimer;
	u8				num;
} TX_TS_RECORD, *PTX_TS_RECORD;

typedef struct _RX_TS_RECORD {
	TS_COMMON_INFO		TsCommonInfo;
	u16				RxIndicateSeq;
	u16				RxTimeoutIndicateSeq;
	struct list_head		RxPendingPktList;
	struct timer_list		RxPktPendingTimer;
	BA_RECORD			RxAdmittedBARecord;	 // For BA Recepient
	u16				RxLastSeqNum;
	u8				RxLastFragNum;
	u8				num;
} RX_TS_RECORD, *PRX_TS_RECORD;

#endif
