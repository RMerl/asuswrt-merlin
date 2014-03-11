/** @file tlv_00120f.h 

License: See LICENSE file for more info.

Authors: Terry Simons (terry.simons@gmail.com)
*/

#ifndef LLDP_TLV_00120F_H
#define LLDP_TLV_00120F_H

/* 802.1AB Annex G IEEE 802.3 Organizationally SPecific TLVs */
/* 0 reserved */
#define MAC_PHY_CONFIGURATION_STATUS_TLV 1 /* Subclause Reference: G.2 */
#define POWER_VIA_MDI_TLV                2 /* Subclause Reference: G.3 */
#define LINK_AGGREGATION_TLV             3 /* Subclause Reference: G.4 */
#define MAXIMUM_FRAME_SIZE_TLV           4 /* Subclause Reference: G.5 */
/* 5 - 255 reseved */
/* End 802.1AB Annex G IEEE 802.3 Organizationally Specific TLVs */

/* IEEE 802.3 auto-negotiation Support/Status Masks */
#define AUTO_NEGOTIATION_SUPPORT 1 /* bit 0 */
#define AUTO_NEGOTIATION_STATUS  2 /* bit 1 */
/* bits 2 - 7 reserved */
/* End IEEE 802.3 auto-negotiation Support/Status Masks */

/* IEEE 802.3 Aggregation Capability/Status Masks */
#define AGGREGATION_CAPABILITY 1 /* bit 0 */
#define AGGREGATION_STATUS     2 /* bit 1 */
/* bits 2 - 7 reserved */
/* End IEEE 802.3 Aggregation Capability/Status Masks */

/*
       ifMauAutoNegCapAdvertisedBits OBJECT-TYPE

       SYNTAX      BITS {
	 bOther(0),        -- other or unknown
	   b10baseT(1),      -- 10BASE-T  half duplex mode
	   b10baseTFD(2),    -- 10BASE-T  full duplex mode
	   b100baseT4(3),    -- 100BASE-T4
	   b100baseTX(4),    -- 100BASE-TX half duplex mode
	   b100baseTXFD(5),  -- 100BASE-TX full duplex mode
	   b100baseT2(6),    -- 100BASE-T2 half duplex mode
	   b100baseT2FD(7),  -- 100BASE-T2 full duplex mode
	   bFdxPause(8),     -- PAUSE for full-duplex links
						bFdxAPause(9),    -- Asymmetric PAUSE for full-duplex
                                 --     links
												bFdxSPause(10),   -- Symmetric PAUSE for full-duplex
                                 --     links
																	       bFdxBPause(11),   -- Asymmetric and Symmetric PAUSE for
                                 --     full-duplex links
					      b1000baseX(12),   -- 1000BASE-X, -LX, -SX, -CX half
                                 --     duplex mode
					      b1000baseXFD(13), -- 1000BASE-X, -LX, -SX, -CX full
                                 --     duplex mode
					      b1000baseT(14),   -- 1000BASE-T half duplex mode
					      b1000baseTFD(15)  -- 1000BASE-T full duplex mode
					      }

*/

// RFC 3636        ifMauAutoNegCapAdvertisedBits
// Referenced in Appendix G.2

enum ifMauAutoNegCapAdvertisedBits {
  bOther,
  b10baseT,
  b10baseTFD,
  b100baseT4,
  b100baseTX,
  b100baseTXFD,
  b100baseT2,
  b100baseT2FD,
  bFdxPause,
  bFdxAPause,
  bFdxSPause,
  bFdxBPause,
  b1000baseX,
  b1000baseXFD,
  b1000baseT,
  b1000baseTFD
};

/*
enum dot3MauType {
  
};
*/


#endif /* LLDP_TLV_00120F_H */
