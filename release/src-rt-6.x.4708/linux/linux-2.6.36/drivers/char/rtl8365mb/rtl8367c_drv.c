#include <linux/init.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <asm/uaccess.h>

#include <smi.h>
#include <rtk_types.h>
#include <rtk_switch.h>
#include <rtk_error.h>
#include <linux/major.h>
#include <port.h>
#include <stat.h>
#include <cpu.h>
#include <rtl8367c_asicdrv_phy.h>
#include <rtl8367c_asicdrv_port.h>

#define RTL8367R_DEVNAME	"rtkswitch"
static DEFINE_MUTEX(rtl8367rb_lock);

#define NAME			"rtl8365mb"

MODULE_DESCRIPTION("Realtek " NAME " support");
MODULE_AUTHOR("ASUS");
MODULE_LICENSE("Proprietary");

#define CONTROL_REG_PORT_POWER_BIT	0x800

#define LAN_PORT_1			3	/* P3 LAN1 port is most closed to WAN port*/
#define LAN_PORT_2			2	/* P2 */
#define LAN_PORT_3			1	/* P1 */
#define LAN_PORT_4			0	/* P0 */
#if 0
#define WAN_PORT

#if defined(CONFIG_SWITCH_CHIP_RTL8367RB)
    #define LAN_MAC_PORT			RTK_EXT_1_MAC
    #if defined(CONFIG_RAETH_GMAC2)
	#if defined(CONFIG_WLAN2_ON_SWITCH_GMAC2)		//RTL8367RB GMAC2 for wifi2
		#define WAN_MAC_PORT		RTK_EXT_1_MAC
		#define WAN_MAC_PORT_MASK	(1U << WAN_MAC_PORT)
	#else					//RTL8367RB GMAC2 for wan
		#define WAN_MAC_PORT		RTK_EXT_2_MAC
		#define WAN_MAC_PORT_MASK	(1U << WAN_MAC_PORT)
	#endif
    #else
	#if 1 //has WAN
		#define WAN_MAC_PORT		RTK_EXT_1_MAC
		#define WAN_MAC_PORT_MASK	(1U << WAN_MAC_PORT)
	#else //no WAN for QATool test
		#define WAN_MAC_PORT		(0)
		#define WAN_MAC_PORT_MASK	(0)
	#endif
    #endif
#elif defined(CONFIG_SWITCH_CHIP_RTL8368MB)
	#if !defined(CONFIG_RAETH_GMAC2)
		#error need CONFIG_RAETH_GMAC2
	#endif
	#define LAN_MAC_PORT		RTK_EXT_0_MAC
	#define WAN_MAC_PORT		RTK_EXT_1_MAC
	#define WAN_MAC_PORT_MASK	(1U << WAN_MAC_PORT)
#endif

/* Port bitmask definitions base on above definitions*/
#define WAN_PORT_MASK			(1U << WAN_PORT)
#define WAN_ALL_PORTS_MASK		(WAN_MAC_PORT_MASK | WAN_PORT_MASK)
#endif

#define LAN_MAC_PORT			EXT_PORT0

#define WAN_PORT_MASK			0
#define LAN_PORT_1_MASK			(1U << LAN_PORT_1)
#define LAN_PORT_2_MASK			(1U << LAN_PORT_2)
#define LAN_PORT_3_MASK			(1U << LAN_PORT_3)
#define LAN_PORT_4_MASK			(1U << LAN_PORT_4)
#define LAN_MAC_PORT_MASK		(1U << LAN_MAC_PORT)
#define WAN_MAC_PORT_MASK		0
#define WLAN2_MAC_PORT_MASK		0

#define LAN_ALL_PORTS_MASK		(LAN_MAC_PORT_MASK | LAN_PORT_1_MASK | LAN_PORT_2_MASK | LAN_PORT_3_MASK | LAN_PORT_4_MASK | WLAN2_MAC_PORT_MASK)
#define WAN_ALL_PORTS_MASK		(WAN_MAC_PORT_MASK | WAN_PORT_MASK)
#define LAN_PORTS_MASK			(LAN_PORT_1_MASK | LAN_PORT_2_MASK | LAN_PORT_3_MASK | LAN_PORT_4_MASK)
#define ALL_PORTS_MASK			(WAN_ALL_PORTS_MASK | LAN_ALL_PORTS_MASK)

#if 0
#if defined(SMI_SCK_GPIO)
#define SMI_SCK	SMI_SCK_GPIO		/* Use SMI_SCK_GPIO as SMI_SCK */
#else
#define SMI_SCK	2			/* Use SMI_SCK/GPIO#2 as SMI_SCK */
#endif

#if defined(SMI_SDA_GPIO)
#define SMI_SDA	SMI_SDA_GPIO		/* Use SMI_SDA_GPIO as SMI_SDA */
#else
#define SMI_SDA	1			/* Use SMI_SDA/GPIO#1 as SMI_SDA */
#endif

struct trafficCount_t {
	u64	rxByteCount;
	u64	txByteCount;
};

enum { //acl id from 0 ~ 63
	ACLID_REDIRECT = 0,
	ACLID_INIC_CTRL_PKT,
	ACLID_WAN_TR_SOC,
	ACLID_SOC_TR_WAN,
};
#endif

#define ENUM_PORT_BEGIN(p, m, port_mask, cond)	\
	for (p = 0, m = (port_mask); 		\
		cond && m > 0; 			\
		m >>= 1, p++) {			\
		if (!(m & 1U))			\
			continue;

#define ENUM_PORT_END	}

static const unsigned int s_wan_stb_array[7] = {
	/* 0:LLLLW	LAN: P0,P1,P2,P3	WAN: P4, STB: N/A (default mode) */
	LLLLW,
	/* 1:LLLWW	LAN: P0,P1,P2		WAN: P4, STB: P3 */
	LAN_PORT_1_MASK,
	/* 2:LLWLW	LAN: P0,P1,P3		WAN: P4, STB: P2 */
	LAN_PORT_2_MASK,
	/* 3:LWLLW	LAN: P0,P2,P3		WAN: P4, STB: P1 */
	LAN_PORT_3_MASK,
	/* 4:WLLLW	LAN: P1,P2,P3		WAN: P4, STB: P0 */
	LAN_PORT_4_MASK,
	/* 5:LLWWW	LAN: P0,P1		WAN: P4, STB: P2,P3 */
	LAN_PORT_1_MASK | LAN_PORT_2_MASK,
	/* 6:WWLLW	LAN: P2,P3		WAN: P4, STB: P0,P1 */
	LAN_PORT_3_MASK | LAN_PORT_4_MASK,
};

#if 0
/* Test control interface between CPU and Realtek switch.
 * @return:
 * 	0:	Success
 *  otherwise:	Fail
 */
static int test_smi_signal_and_wait(unsigned int test_count)
{
#define MIN_GOOD_COUNT	6
	int i, good = 0, r = -1;
	rtk_uint32 data;
	rtk_api_ret_t retVal;

	for (i = 0; i < test_count && good < MIN_GOOD_COUNT; i++) {
		if ((retVal = rtl8367c_getAsicReg(0x1202, &data)) != RT_ERR_OK)
			printk("error = %d\n", retVal);

		if (data)
			printk("0x%x,", data);
		else
			printk(".");

		if (data == 0x88a8)
			good++;
		else
			good = 0;

		udelay(50000);
	}
	printk("\n");

	if (good >= MIN_GOOD_COUNT)
		r = 0;

	return r;
}

/* Power ON/OFF port
 * @port:	port id
 * @onoff:
 *  0:		means off
 *  otherwise:	means on
 * @return:
 *  0:		success
 * -1:		invalid parameter
 */
static int __ctrl_port_power(rtk_port_t port, int onoff)
{
	rtk_port_phy_data_t pData;

	if (port > 4) {
		printk("%s(): Invalid port id %d\n", __func__, port);
		return -1;
	}

	rtk_port_phyReg_get(port, PHY_CONTROL_REG, &pData);
	if (onoff)
		pData &= ~CONTROL_REG_PORT_POWER_BIT;
	else
		pData |= CONTROL_REG_PORT_POWER_BIT;
	rtk_port_phyReg_set(port, PHY_CONTROL_REG, pData);

	return 0;
}

static inline int power_up_port(rtk_port_t port)
{
	return __ctrl_port_power(port, 1);
}

static inline int power_down_port(rtk_port_t port)
{
	return __ctrl_port_power(port, 0);
}

static void setRedirect(int inputPortMask, int outputPortMask)
{ // forward frames received from inputPorts and forward to outputPorts
	int retVal, ruleNum;
	rtk_filter_field_t field;
	rtk_filter_cfg_t cfg;
	rtk_filter_action_t act;
	int rule_id = ACLID_REDIRECT;

	rtk_filter_igrAcl_cfg_del(rule_id);

	if (inputPortMask == 0)
	{
		printk("### remove redirect ACL ###\n");
		return;
	}

	memset(&field, 0, sizeof(field));
	memset(&cfg, 0, sizeof(cfg));
	memset(&act, 0, sizeof(act));

	field.fieldType = FILTER_FIELD_DMAC;
	field.filter_pattern_union.dmac.dataType = FILTER_FIELD_DATA_MASK;
	field.filter_pattern_union.dmac.value.octet[0] = 0;
	field.filter_pattern_union.dmac.value.octet[1] = 0;
	field.filter_pattern_union.dmac.value.octet[2] = 0;
	field.filter_pattern_union.dmac.value.octet[3] = 0;
	field.filter_pattern_union.dmac.value.octet[4] = 0;
	field.filter_pattern_union.dmac.value.octet[5] = 0;
	field.filter_pattern_union.dmac.mask.octet[0]  = 0;
	field.filter_pattern_union.dmac.mask.octet[1]  = 0;
	field.filter_pattern_union.dmac.mask.octet[2]  = 0;
	field.filter_pattern_union.dmac.mask.octet[3]  = 0;
	field.filter_pattern_union.dmac.mask.octet[4]  = 0;
	field.filter_pattern_union.dmac.mask.octet[5]  = 0;
	field.next = NULL;
	if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &field)) != RT_ERR_OK)
	{
		printk("### set redirect ACL field fail(%d) ###\n", retVal);
		return;
	}

	act.actEnable[FILTER_ENACT_REDIRECT] = TRUE;
	act.filterRedirectPortmask = outputPortMask; //0xFF;  //Forward to all ports
	cfg.activeport.dataType = FILTER_FIELD_DATA_MASK;
	cfg.activeport.value = inputPortMask; //0x1; //By your own decision
	cfg.activeport.mask = 0xFF;
	if ((retVal = rtk_filter_igrAcl_cfg_add(rule_id, &cfg, &act, &ruleNum)) != RT_ERR_OK)
	{
		printk("### set redirect ACL cfg fail(%d) ###\n", retVal);
		return;
	}
	printk("### set redirect rule success ruleNum(%d) ###\n", ruleNum);
}
#endif

static int get_wan_stb_lan_port_mask(int wan_stb_x, unsigned int *wan_pmsk, unsigned int *stb_pmsk, unsigned int *lan_pmsk, int need_mac_port)
{
	int ret = 0;
	unsigned int lan_ports_mask = LAN_PORTS_MASK;
	unsigned int wan_ports_mask = WAN_PORT_MASK;

	if (!wan_pmsk || !stb_pmsk || !lan_pmsk)
		return -EINVAL;

	if (need_mac_port) {
		lan_ports_mask |= LAN_ALL_PORTS_MASK;	/* incl. WLAN2_MAC_PORT_MASK */
		wan_ports_mask |= WAN_MAC_PORT_MASK;
	}

	if (wan_stb_x >= 0 && wan_stb_x < ARRAY_SIZE(s_wan_stb_array)) {
		*stb_pmsk = s_wan_stb_array[wan_stb_x];
		*lan_pmsk = lan_ports_mask & ~*stb_pmsk;
		*wan_pmsk = wan_ports_mask | *stb_pmsk;
	} else if (wan_stb_x == ALL_LAN) {
		*stb_pmsk = 0;
		*lan_pmsk = lan_ports_mask | WAN_PORT_MASK;
		*wan_pmsk = 0;	/* Skip WAN_MAC_PORT_MASK */
	} else if (wan_stb_x == NO_WAN_PORT) {
		*stb_pmsk = 0;
		*lan_pmsk = lan_ports_mask;
		*wan_pmsk = 0;	/* Skip WAN_MAC_PORT_MASK */
	} else {
		printk(KERN_WARNING "%pF() pass invalid invalid wan_stb_x %d to %s()\n",
			__builtin_return_address(0), wan_stb_x, __func__);
		ret = -EINVAL;
	}

	return 0;
}

#if 0
static int __LANWANPartition(int wan_stb_x)
{
	rtk_portmask_t fwd_mask;
	unsigned int port, mask, lan_port_mask, wan_port_mask, stb_port_mask;

	if (get_wan_stb_lan_port_mask(wan_stb_x, &wan_port_mask, &stb_port_mask, &lan_port_mask, 1))
		return -EINVAL;

	printk(KERN_INFO "wan_stb_x %d STB,LAN/WAN ports mask 0x%03x,%03x/%03x\n",
		wan_stb_x, stb_port_mask, lan_port_mask, wan_port_mask);

	if( RT_ERR_OK != rtk_vlan_init())
		printk("\r\nVLAN Initial Failed!!!");

	/* LAN */
	ENUM_PORT_BEGIN(port, mask, lan_port_mask, 1)
		fwd_mask.bits[0] = lan_port_mask;
		rtk_port_isolation_set(port, fwd_mask);
		rtk_port_efid_set(port, 0);
	ENUM_PORT_END

	/* WAN */
	ENUM_PORT_BEGIN(port, mask, wan_port_mask, 1)
		fwd_mask.bits[0] = wan_port_mask;
		rtk_port_isolation_set(port, fwd_mask);
#if (LAN_MAC_PORT == WAN_MAC_PORT) //vlan mode
		rtk_port_efid_set(port, 0);
#else
		rtk_port_efid_set(port, 1);
#endif
	ENUM_PORT_END

#if (LAN_MAC_PORT == WAN_MAC_PORT) //vlan mode
#define VLAN_ID_WAN 2

	port = LAN_MAC_PORT;
	fwd_mask.bits[0] = lan_port_mask | wan_port_mask;
	rtk_port_isolation_set(port, fwd_mask);
	rtk_port_efid_set(port, 0);

	{
		rtk_portmask_t mbrmsk, untagmsk;

		mbrmsk.bits[0] = lan_port_mask;
		untagmsk.bits[0] = lan_port_mask & ~LAN_MAC_PORT_MASK; //SoC should get tag packet
		if( RT_ERR_OK != rtk_vlan_set(1, mbrmsk, untagmsk, 1))
			printk("\r\nVLAN 1 Setup Failed!!!");

		mbrmsk.bits[0] = wan_port_mask;
		untagmsk.bits[0] = wan_port_mask & ~LAN_MAC_PORT_MASK; //SoC should get tag packet
		if( RT_ERR_OK != rtk_vlan_set(VLAN_ID_WAN, mbrmsk, untagmsk, 2))
			printk("\r\nVLAN 2 Setup Failed!!!");
#ifdef CONFIG_WLAN2_ON_SWITCH_GMAC2
		mbrmsk.bits[0] = LAN_MAC_PORT_MASK | WLAN2_MAC_PORT_MASK;
		untagmsk.bits[0] = ALL_PORTS_MASK;
		if( RT_ERR_OK != rtk_vlan_set(3, mbrmsk, untagmsk, 3))
			printk("\r\nVLAN 3 Setup Failed!!!");
#if defined(CONFIG_RT3352_INIC_MII) || defined(CONFIG_RT3352_INIC_MII_MODULE) // for lanaccess control
		mbrmsk.bits[0] = LAN_MAC_PORT_MASK | WLAN2_MAC_PORT_MASK; //wireless pkt through iNIC that NOT allow to access LAN devices
		untagmsk.bits[0] = 0x0000;
		if( RT_ERR_OK != rtk_vlan_set(4, mbrmsk, untagmsk, 4))
			printk("\r\nVLAN 4 Setup Failed!!!");
		if( RT_ERR_OK != rtk_vlan_set(5, mbrmsk, untagmsk, 5))
			printk("\r\nVLAN 5 Setup Failed!!!");
		if( RT_ERR_OK != rtk_vlan_set(6, mbrmsk, untagmsk, 6))
			printk("\r\nVLAN 6 Setup Failed!!!");
#endif
#endif
	}

	/* LAN */
	ENUM_PORT_BEGIN(port, mask, lan_port_mask, 1)
		if(RT_ERR_OK != rtk_vlan_portPvid_set(port, 1, 0))
			printk("\r\nPort %d Setup Failed!!!", port);
	ENUM_PORT_END

	/* WAN */
	ENUM_PORT_BEGIN(port, mask, wan_port_mask, 1)
		if(RT_ERR_OK != rtk_vlan_portPvid_set(port, VLAN_ID_WAN, 0))
			printk("\r\nPort %d Setup Failed!!!", port);
	ENUM_PORT_END

#ifdef CONFIG_WLAN2_ON_SWITCH_GMAC2
	{
		int retVal, ruleNum;
		rtk_filter_field_t field;
		rtk_filter_cfg_t cfg;
		rtk_filter_action_t act;

		rtk_filter_igrAcl_cfg_del(ACLID_INIC_CTRL_PKT);

		memset(&cfg, 0, sizeof(rtk_filter_cfg_t));
		memset(&act, 0, sizeof(rtk_filter_action_t));
		memset(&field, 0, sizeof(rtk_filter_field_t));
		field.fieldType = FILTER_FIELD_ETHERTYPE;
		field.filter_pattern_union.etherType.dataType = FILTER_FIELD_DATA_MASK;
		field.filter_pattern_union.etherType.value = 0xFFFF;
		field.filter_pattern_union.etherType.mask = 0xFFFF;
		if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &field)) != RT_ERR_OK)
		{
			printk("### set iNIC ACL field fail(%d) ###\n", retVal);
			return RT_ERR_FAILED;
		}

		cfg.activeport.dataType = FILTER_FIELD_DATA_MASK;
		cfg.activeport.value = 0xC0; //Port 6&7
		cfg.activeport.mask = 0xFF;
		cfg.invert = FALSE;
		act.actEnable[FILTER_ENACT_INGRESS_CVLAN_VID] = TRUE;
		act.filterIngressCvlanVid= 3;
		act.actEnable[FILTER_ENACT_PRIORITY] = TRUE;
		act.filterPriority = 7;
		if ((retVal = rtk_filter_igrAcl_cfg_add(ACLID_INIC_CTRL_PKT, &cfg, &act, &ruleNum)) != RT_ERR_OK)
		{
			printk("### set iNIC ACL cfg fail(%d) ###\n", retVal);
			return RT_ERR_FAILED;
		}
		printk("### set iNIC rule success ruleNum(%d) ###\n", ruleNum);
	}
	{
		rtk_priority_select_t priSel;

		rtk_qos_init(2);	//use 2 output queue for frame priority
		priSel.acl_pri   = 7;
		priSel.dot1q_pri = 6;
		priSel.port_pri  = 5;
		priSel.dscp_pri  = 4;
		priSel.cvlan_pri = 3;
		priSel.svlan_pri = 2;
		priSel.dmac_pri  = 1;
		priSel.smac_pri  = 0;
		rtk_qos_priSel_set(&priSel);

/*
		rtk_qos_pri2queue_t pri2qid;
		pri2qid.pri2queue[0] = 0;
		pri2qid.pri2queue[7] = 7;
		rtk_qos_priMap_set(, &pri2qid);
*/
		printk("# set QoS for iNIC control traffic #\n");
	}
#endif //CONFIG_WLAN2_ON_SWITCH_GMAC2
#endif

	return 0;
}

#endif

static int wan_stb_g = NO_WAN_PORT;

#if 0
void LANWANPartition_adv(int wan_stb_x)
{
	__LANWANPartition(wan_stb_x);
}

static int voip_port_g = 0;
static rtk_vlan_t vlan_vid = 0;
static rtk_vlan_t vlan_prio = 0;
static int wans_lanport = -1;

#if defined(CONFIG_SWITCH_LAN_WAN_SWAP)
#define CONVERT_LAN_WAN_PORTS(portMsk)	(portMsk = (portMsk & ~0x1f) | ((portMsk << 1) & 0x1e) | ((portMsk >> 4) & 1))
#else
#define CONVERT_LAN_WAN_PORTS(portMsk)
#endif

void initialVlan(u32 portinfo)/*Initalize VLAN. Cherry Cho added in 2011/7/15. */
{
	rtk_portmask_t lanmask, wanmask, tmpmask;
	int i = 0;
	unsigned int mask, port_mask;
	u32 laninfo = 0, waninfo = 0;

	if(portinfo != 0)
	{
		CONVERT_LAN_WAN_PORTS(portinfo);
		laninfo = ~portinfo & LAN_ALL_PORTS_MASK;
		waninfo = portinfo | WAN_ALL_PORTS_MASK;
		lanmask.bits[0] = laninfo;
		wanmask.bits[0] = waninfo;
		printk("initialVlan - portinfo = 0x%X LAN portmask= 0x%X WAN portmask = 0x%X\n", portinfo, lanmask.bits[0], wanmask.bits[0]);
		for(i = 0; laninfo && i <= 15; i++, laninfo >>= 1)//LAN
		{
			if(!(laninfo & 0x1))
				continue;

			rtk_port_isolation_set(i, lanmask);
			rtk_port_efid_set(i, 0);
		}

		for(i = 0; waninfo && i <= 3; i++, waninfo >>= 1)//STB
		{
			if(!(waninfo & 0x1))
				continue;

			tmpmask.bits[0] = WAN_PORT_MASK | (1 << i);
			rtk_port_isolation_set(i, tmpmask);
#if (LAN_MAC_PORT == WAN_MAC_PORT) //vlan mode
			rtk_port_efid_set(i, 0);
#else
			rtk_port_efid_set(i, 1);
#endif
		}

#if (LAN_MAC_PORT == WAN_MAC_PORT) //vlan mode
		rtk_port_isolation_set(WAN_PORT, wanmask);
		rtk_port_efid_set(WAN_PORT, 0);
		wanmask.bits[0] = WAN_ALL_PORTS_MASK | lanmask.bits[0];
		rtk_port_isolation_set(WAN_MAC_PORT, wanmask);
		rtk_port_efid_set(WAN_MAC_PORT, 0);
#elif (WAN_MAC_PORT != 0) //non vlan mode
		rtk_port_isolation_set(WAN_PORT, wanmask);
		rtk_port_efid_set(WAN_PORT, 1);
		wanmask.bits[0] = WAN_ALL_PORTS_MASK;
		rtk_port_isolation_set(WAN_MAC_PORT, wanmask);
		rtk_port_efid_set(WAN_MAC_PORT, 1);
#endif
	}

        /* set VLAN filtering for each LAN port */
	port_mask = LAN_PORTS_MASK | WAN_PORT_MASK | (1 << RTK_EXT_0_MAC) | (1 << RTK_EXT_1_MAC) | (1 << RTK_EXT_2_MAC);
	ENUM_PORT_BEGIN(i, mask, port_mask, 1)
		rtk_vlan_portIgrFilterEnable_set(i, ENABLED);
	ENUM_PORT_END
}

static inline u32 fixExtPortMapping(u32 v) // port8, port9 (for RTL8367M) --> port6, port7 (for RTL8367RB)
{
#if defined(CONFIG_SWITCH_CHIP_RTL8367RB)
	return (((v & 0x300) >> 2) | (v & ~0x300));
#elif defined(CONFIG_SWITCH_CHIP_RTL8368MB)	//port5, port6 as LAN, WAN
	return (((v & 0x300) >> 3) | (v & ~0x300));
#endif
}

/* portInfo:  bit0-bit15 :  member mask
	     bit16-bit31 :  untag mask */
void createVlan(u32 portinfo)/* Cherry Cho added in 2011/7/14. */
{
	rtk_portmask_t mbrmsk, untagmsk;
	int i = 0;

	mbrmsk.bits[0] = portinfo & 0x0000FFFF;
	untagmsk.bits[0] = portinfo >> 16;
	CONVERT_LAN_WAN_PORTS(mbrmsk.bits[0]);
	CONVERT_LAN_WAN_PORTS(untagmsk.bits[0]);
	portinfo = mbrmsk.bits[0] | (untagmsk.bits[0] << 16);
	printk("createVlan - vid = %d prio = %d mbrmsk = 0x%X untagmsk = 0x%X\n", vlan_vid, vlan_prio, mbrmsk.bits[0], untagmsk.bits[0]);

	/* convert to match system spec */
	mbrmsk.bits[0]   = fixExtPortMapping(mbrmsk.bits[0]);
	untagmsk.bits[0] = fixExtPortMapping(untagmsk.bits[0]);

#if (LAN_MAC_PORT == WAN_MAC_PORT) //vlan mode
	if(mbrmsk.bits[0] & WLAN2_MAC_PORT_MASK)
		mbrmsk.bits[0] = (mbrmsk.bits[0] & ~(WLAN2_MAC_PORT_MASK)) | (WAN_MAC_PORT_MASK);
	if(untagmsk.bits[0] & WLAN2_MAC_PORT_MASK)
		untagmsk.bits[0] = (untagmsk.bits[0] & ~(WLAN2_MAC_PORT_MASK)) | (WAN_MAC_PORT_MASK);
#endif
#if defined(CONFIG_WLAN2_ON_SWITCH_GMAC2)
	if(mbrmsk.bits[0]   & 0x800)
		mbrmsk.bits[0]   = (mbrmsk.bits[0]    & ~(0x800)) | WLAN2_MAC_PORT_MASK;
	if(untagmsk.bits[0] & 0x800)
		untagmsk.bits[0] = (untagmsk.bits[0]  & ~(0x800)) | WLAN2_MAC_PORT_MASK;
#else
	mbrmsk.bits[0]   &= ~(0x800);
	untagmsk.bits[0] &= ~(0x800);
#endif
	printk("createVlan2- vid = %d prio = %d mbrmsk = 0x%X untagmsk = 0x%X\n", vlan_vid, vlan_prio, mbrmsk.bits[0], untagmsk.bits[0]);

	rtk_vlan_set(vlan_vid, mbrmsk, untagmsk, 0);

	for(i = 0; i <= 9; i++)
	{
		if((portinfo >>i ) & 0x1)
			rtk_vlan_portPvid_set (i, vlan_vid, vlan_prio);
	}

#if (LAN_MAC_PORT == WAN_MAC_PORT) //vlan mode
	if(mbrmsk.bits[0] & WAN_MAC_PORT_MASK)
	{ // traffic through WAN port
		int retVal, ruleNum;
		rtk_filter_field_t  filter_field;
		rtk_filter_cfg_t    cfg;
		rtk_filter_action_t act;

//set traffic from ISR (WAN)
		memset(&filter_field, 0, sizeof(filter_field));
		memset(&cfg, 0, sizeof(cfg));
		memset(&act, 0, sizeof(act));
		filter_field.fieldType = FILTER_FIELD_CTAG;
		filter_field.filter_pattern_union.ctag.vid.dataType = FILTER_FIELD_DATA_MASK;
		filter_field.filter_pattern_union.ctag.vid.value = vlan_vid;		//target vlan id
		filter_field.filter_pattern_union.ctag.vid.mask= 0xFFF;
		filter_field.filter_pattern_union.ctag.pri.value = vlan_prio;		//target vlan priority
		filter_field.filter_pattern_union.ctag.pri.mask= 0x7;
		filter_field.next = NULL;
		if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field)) != RT_ERR_OK)
		{
			printk("### set vlan tr1 ACL field fail(%d) ###\n", retVal);
			return;
		}

		/*Add port 4 to active ports*/
		cfg.activeport.dataType = FILTER_FIELD_DATA_MASK;
		cfg.activeport.value = WAN_PORT_MASK;
		cfg.activeport.mask = 0xFF;
		cfg.invert =FALSE;
		/*Set Action to Change VALN to vlan id of WAN */
		act.actEnable[FILTER_ENACT_INGRESS_CVLAN_VID] = TRUE;
		act.filterIngressCvlanVid = VLAN_ID_WAN;				//vlan id of WAN
		/*Set Action to Change Priority 0 */
		act.actEnable[FILTER_ENACT_PRIORITY] = TRUE;
		act.filterPriority = 0;							//vlan priority of WAN
		if ((retVal = rtk_filter_igrAcl_cfg_add(ACLID_WAN_TR_SOC, &cfg, &act, &ruleNum)) != RT_ERR_OK)
		{
			printk("### set vlan tr1 ACL cfg fail(%d) ###\n", retVal);
			return;
		}
		printk("### set vlan id exchange rule success ruleNum(%d) ###\n", ruleNum);

//set traffic from SoC
		memset(&filter_field, 0, sizeof(filter_field));
		memset(&cfg, 0, sizeof(cfg));
		memset(&act, 0, sizeof(act));
		filter_field.fieldType = FILTER_FIELD_CTAG;
		filter_field.filter_pattern_union.ctag.vid.dataType = FILTER_FIELD_DATA_MASK;
		filter_field.filter_pattern_union.ctag.vid.value = VLAN_ID_WAN;		//target vlan id
		filter_field.filter_pattern_union.ctag.vid.mask= 0xFFF;
		filter_field.filter_pattern_union.ctag.pri.value = 0;			//target vlan priority
		filter_field.filter_pattern_union.ctag.pri.mask= 0x7;
		filter_field.next = NULL;
		if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field)) != RT_ERR_OK)
		{
			printk("### set vlan tr2 ACL field fail(%d) ###\n", retVal);
			return;
		}

		/*Add port 4 to active ports*/
		cfg.activeport.dataType = FILTER_FIELD_DATA_MASK;
		cfg.activeport.value = WAN_MAC_PORT_MASK;
		cfg.activeport.mask = 0xFF;
		cfg.invert =FALSE;
		/*Set Action to Change VALN to vlan id of WAN */
		act.actEnable[FILTER_ENACT_INGRESS_CVLAN_VID] = TRUE;
		act.filterIngressCvlanVid = vlan_vid;					//vlan id to WAN
		/*Set Action to Change Priority 0 */
		act.actEnable[FILTER_ENACT_PRIORITY] = TRUE;
		act.filterPriority = vlan_prio;						//vlan priority to WAN
		if ((retVal = rtk_filter_igrAcl_cfg_add(ACLID_SOC_TR_WAN, &cfg, &act, &ruleNum)) != RT_ERR_OK)
		{
			printk("### set vlan tr2 ACL cfg fail(%d) ###\n", retVal);
			return;
		}
		printk("### set vlan id exchange rule2 success ruleNum(%d) ###\n", ruleNum);
	}
#endif
}
#endif

rtk_api_ret_t rtk_port_linkStatus_get(rtk_port_t port, rtk_port_linkStatus_t *pLinkStatus)
{
    rtk_api_ret_t retVal;
    rtk_uint32 phyData;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;

    if ((retVal = rtl8367c_setAsicPHYReg(port,RTL8367C_PHY_PAGE_ADDRESS,0))!=RT_ERR_OK)
        return retVal;

    /*Get PHY status register*/
    if (RT_ERR_OK != rtl8367c_getAsicPHYReg(port,PHY_STATUS_REG,&phyData))
        return RT_ERR_FAILED;

    /*check link status*/
    if (phyData & (1<<2))
    {
        *pLinkStatus = 1;
    }
    else
    {
        *pLinkStatus = 0;
    }

    return RT_ERR_OK;
}

typedef struct {
	unsigned int idx;
	unsigned int value;
} asus_gpio_info;

typedef struct {
	unsigned int link[4];
	unsigned int speed[4];
} phyState;

static unsigned int txDelay_user = 1;
static unsigned int rxDelay_user = 1;
static unsigned int txDelay_user_ary[] = { 1, 1, 1 };
static unsigned int rxDelay_user_ary[] = { 1, 1, 1 };

struct ext_port_tbl_s {
	rtk_port_t id;
	rtk_data_t tx_delay, rx_delay;
};

static struct ext_port_tbl_s ext_port_tbl[] = {
		{ EXT_PORT0, 1, 0 }			/* Ext1: RT-N36U3 LAN; RT-N65U LAN/WAN */
};
#if 0

static rtk_port_t conv_ext_port_id(int port_nr)
{
	rtk_port_t ext_port_id = EXT_PORT_END;

	switch (port_nr) {
#if defined(CONFIG_SWITCH_HAS_EXT0)
	case 0:
		ext_port_id = EXT_PORT_0;
		break;
#endif
#if defined(CONFIG_SWITCH_HAS_EXT1)
	case 1:
		ext_port_id = EXT_PORT_1;
		break;
#endif
#if defined(CONFIG_SWITCH_HAS_EXT2)
	case 2:
		ext_port_id = EXT_PORT_2;
		break;
#endif
	default:
		printk("Invalid ext. port id %d\n", port_nr);
		break;
	}

	return ext_port_id;
}

static rtk_port_t get_wan_ext_port_id(unsigned int *id)
{
	rtk_port_t ext_port_id = EXT_PORT_END;

	switch (WAN_MAC_PORT) {
#if defined(CONFIG_SWITCH_HAS_EXT0)
	case RTK_EXT_0_MAC:
		ext_port_id = EXT_PORT_0;
		if (id) *id = 0;
		break;
#endif
#if defined(CONFIG_SWITCH_HAS_EXT1)
	case RTK_EXT_1_MAC:
		ext_port_id = EXT_PORT_1;
		if (id) *id = 1;
		break;
#endif
#if defined(CONFIG_SWITCH_HAS_EXT2)
	case RTK_EXT_2_MAC:
		ext_port_id = EXT_PORT_2;
		if (id) *id = 2;
		break;
#endif
	default:
		printk("Invalid WAN ext. port id %d\n", WAN_MAC_PORT);
		break;
	}

	return ext_port_id;
}
#endif

static int initialize_switch(void)
{
	int ret = -1, retry = 0;
	rtk_port_mac_ability_t pa;

	for(retry = 0; ret && retry < 10; ++retry)
	{
		ret = rtk_switch_init();	
		if(ret) msleep(100);
		else break;
	}
	printk(NAME "rtl8365mb initialized(%d)(retry:%d) %s\n", ret, retry, ret?"failed":"");

	msleep(100);

        ret = rtk_port_phyEnableAll_set(1);
        if(ret) printk("rtk port_phyEnableAll Failed!(%d)\n", ret);
        else printk("rtk port_phyEnableAll ok\n");

	msleep(100);

        /* configure & set GMAC ports */
        pa.forcemode       = MAC_FORCE;
        pa.speed           = SPD_1000M;
        pa.duplex          = FULL_DUPLEX;
        pa.link            = 1;
        pa.nway            = 0;
        pa.rxpause         = 0;
        pa.txpause         = 0;

        ret = rtk_port_macForceLinkExt_set(EXT_PORT0, MODE_EXT_RGMII, &pa);
        if(ret) printk("rtk port_macForceLink_set ext_Port0 Failed!(%d)\n", ret);
        else printk("rtk port_macForceLink_set ext_Port0 ok\n");

	msleep(100);

        /* asic chk */
        rtk_uint32 retVal, retVal2;
        rtk_uint32 data, data2;

        if((retVal = rtl8367c_getAsicReg(0x1311, &data)) != RT_ERR_OK || (retVal2 = rtl8367c_getAsicReg(0x1305, &data2)) != RT_ERR_OK) {
                printk("get failed(%d)(%d). (%x)(supposed to be 0x1016)\n", retVal, retVal2, data);
        }
        else {      /* get ok, then chk val*/
                printk("get ok, chk 0x1311:%x(0x1016), 0x1305:%x(b4~b7:0x1)\n", data, data2);
                if(data != 0x1016){
                        if((retVal = rtl8367c_setAsicReg(0x1311, 0x1016)) != RT_ERR_OK)
                                printk("set 0x1311(0x1016) failed(%d)\n", retVal);
                        else
                                printk("reset 0x1311 (0x1016)\n");
                }
                if((data2 & 0xf0) != 0x10) {
                        data2 &= ~0xf0;
                        data2 |= 0x10;
                        if((retVal2 = rtl8367c_setAsicReg(0x1305, data2)) != RT_ERR_OK)
                                printk("set 0x1305(%x) failed(%d)\n", data2, retVal2);
                        else
                                printk("reset 0x1305 (%x)\n", data2);
                }
        }

	msleep(100);

        /* chk again */
        if((retVal = rtl8367c_getAsicReg(0x1311, &data)) != RT_ERR_OK || (retVal2 = rtl8367c_getAsicReg(0x1305, &data2)) != RT_ERR_OK) {
                printk("get failed(%d)(%d). (%x)(supposed to be 0x1016)\n", retVal, retVal2, data);
        }
        else    /* get ok, then chk val*/
                printk("(2) get again ok, chk again 0x1311:%x(0x1016), 0x1305:%x(b4~b7:0x1)\n", data, data2);
        /*~asic chk*/

	return 0;
}

void show_port_stat(rtk_stat_port_cntr_t *pPort_cntrs)
{
	printk("ifInOctets: %lld \ndot3StatsFCSErrors: %d \ndot3StatsSymbolErrors: %d \n"
		"dot3InPauseFrames: %d \ndot3ControlInUnknownOpcodes: %d \n"
		"etherStatsFragments: %d \netherStatsJabbers: %d \nifInUcastPkts: %d \n"
		"etherStatsDropEvents: %d \netherStatsOctets: %lld \netherStatsUndersizePkts: %d \n"
		"etherStatsOversizePkts: %d \netherStatsPkts64Octets: %d \n"
		"etherStatsPkts65to127Octets: %d \netherStatsPkts128to255Octets: %d \n"
		"etherStatsPkts256to511Octets: %d \netherStatsPkts512to1023Octets: %d \n"
		"etherStatsPkts1024toMaxOctets: %d \netherStatsMcastPkts: %d \n"
		"etherStatsBcastPkts: %d \nifOutOctets: %lld \ndot3StatsSingleCollisionFrames: %d \n"
		"dot3StatsMultipleCollisionFrames: %d \ndot3StatsDeferredTransmissions: %d \n"
		"dot3StatsLateCollisions: %d \netherStatsCollisions: %d \n"
		"dot3StatsExcessiveCollisions: %d \ndot3OutPauseFrames: %d \n"
		"dot1dBasePortDelayExceededDiscards: %d \ndot1dTpPortInDiscards: %d \n"
		"ifOutUcastPkts: %d \nifOutMulticastPkts: %d \nifOutBrocastPkts: %d \n"
		"outOampduPkts: %d \ninOampduPkts: %d \npktgenPkts: %d\n\n",
		pPort_cntrs->ifInOctets,
		pPort_cntrs->dot3StatsFCSErrors,
		pPort_cntrs->dot3StatsSymbolErrors,
		pPort_cntrs->dot3InPauseFrames,
		pPort_cntrs->dot3ControlInUnknownOpcodes,
		pPort_cntrs->etherStatsFragments,
		pPort_cntrs->etherStatsJabbers,
		pPort_cntrs->ifInUcastPkts,
		pPort_cntrs->etherStatsDropEvents,
		pPort_cntrs->etherStatsOctets,
		pPort_cntrs->etherStatsUndersizePkts,
		pPort_cntrs->etherStatsOversizePkts,
		pPort_cntrs->etherStatsPkts64Octets,
		pPort_cntrs->etherStatsPkts65to127Octets,
		pPort_cntrs->etherStatsPkts128to255Octets,
		pPort_cntrs->etherStatsPkts256to511Octets,
		pPort_cntrs->etherStatsPkts512to1023Octets,
		pPort_cntrs->etherStatsPkts1024toMaxOctets,
		pPort_cntrs->etherStatsMcastPkts,
		pPort_cntrs->etherStatsBcastPkts,
		pPort_cntrs->ifOutOctets,
		pPort_cntrs->dot3StatsSingleCollisionFrames,
		pPort_cntrs->dot3StatsMultipleCollisionFrames,
		pPort_cntrs->dot3StatsDeferredTransmissions,
		pPort_cntrs->dot3StatsLateCollisions,
		pPort_cntrs->etherStatsCollisions,
		pPort_cntrs->dot3StatsExcessiveCollisions,
		pPort_cntrs->dot3OutPauseFrames,
		pPort_cntrs->dot1dBasePortDelayExceededDiscards,
		pPort_cntrs->dot1dTpPortInDiscards,
		pPort_cntrs->ifOutUcastPkts,
		pPort_cntrs->ifOutMulticastPkts,
		pPort_cntrs->ifOutBrocastPkts,
		pPort_cntrs->outOampduPkts,
		pPort_cntrs->inOampduPkts,
		pPort_cntrs->pktgenPkts
	);
}
#if 0

int get_traffic_iNIC(struct trafficCount_t *pTF)
{
	rtk_api_ret_t retVal;
	rtk_stat_port_cntr_t pPort_cntrs;

	memset(pTF, 0, sizeof(*pTF));
#ifdef CONFIG_WLAN2_ON_SWITCH_GMAC2
	retVal = rtk_stat_port_getAll(RTK_EXT_2_MAC, &pPort_cntrs);
	if (retVal == RT_ERR_OK) {
		//show_port_stat(&pPort_cntrs);
		pTF->rxByteCount = pPort_cntrs.ifInOctets;
		pTF->txByteCount = pPort_cntrs.ifOutOctets;
	}
	return 0;
#else
	return -1;
#endif
}

int get_traffic_WAN(struct trafficCount_t *pTF)
{
	rtk_api_ret_t retVal;
	rtk_stat_port_cntr_t pPort_cntrs;
	retVal = rtk_stat_port_getAll(WAN_PORT, &pPort_cntrs);
	if (retVal == RT_ERR_OK) {
		//show_port_stat(&pPort_cntrs);
		pTF->rxByteCount = pPort_cntrs.ifInOctets;
		pTF->txByteCount = pPort_cntrs.ifOutOctets;
	}
	return 0;
}

#define RTK_READ(reg,data)											\
		data = 0;											\
		if ((retVal = rtl8367c_getAsicReg(reg, &data)) != RT_ERR_OK) {					\
			printk("### getAsicReg error: retVal(%d) reg(%04x) data(%08x)\n", retVal, reg, data);	\
		} else {											\
			printk("## read  reg(%04x): %08x\n", reg, data);						\
		}
#define RTK_WRITE(reg,data)											\
		if ((retVal = rtl8367c_setAsicReg(reg, data)) != RT_ERR_OK) {					\
			printk("### setAsicReg error: retVal(%d) reg(%04x) data(%04x)\n", retVal, reg, data);	\
		} else {											\
			printk("## write reg(%04x): %08x\n", reg, data);					\
		}

#endif

long rtl8367rb_do_ioctl(struct file *file, unsigned int req, unsigned long arg)
{
        int i, test_t = 1;
        rtk_data_t pDuplex;
        struct ext_port_tbl_s *p;
        rtk_data_t pSpeed;
        int port_user = 0;
        rtk_port_t port;
        rtk_api_ret_t retVal;
        rtk_data_t txDelay_ro, rxDelay_ro;
        rtk_port_linkStatus_t pLinkStatus = 0, portsLinkStatus = 0;
        unsigned int port_nr, mask, lan_port_mask, wan_port_mask, stb_port_mask;
	rtk_stat_port_cntr_t pPort_cntrs;
	rtk_uint32 data = 0, rtk_reg;
	rtk_asic_t asic_reg;
	rtk_port_mac_ability_t pa;
	rtk_enable_t enable;

        /* Handle Realtek switch related ioctl */
        if (get_wan_stb_lan_port_mask(wan_stb_g, &wan_port_mask, &stb_port_mask, &lan_port_mask, 0))
                return -EINVAL;

	switch(req) {
	case INIT_SWITCH:
		initialize_switch();
		break;

	case GET_LANPORTS_LINK_STATUS:					// check LAN ports phy status
		pLinkStatus = 0;
		ENUM_PORT_BEGIN(port_nr, mask, lan_port_mask, 1)
			retVal = rtk_port_linkStatus_get(port_nr, &pLinkStatus);
			if (retVal == RT_ERR_OK) {
				if(pLinkStatus)	
					portsLinkStatus |= 1<<port_nr;
			} else {
				printk("rtk_port_linkStatus_get() fail, return %d\n", retVal);
			}
		ENUM_PORT_END
		printk("portsLinkStaus=%x\n", portsLinkStatus);

		put_user(portsLinkStatus, (unsigned int __user *)arg);
		break;

	case GET_PORT_STAT:
		copy_from_user(&port_user, (int __user *)arg, sizeof(int));
		port = port_user;
		printk("rtk_stat_port_getAll(%d...)\n", port);

		retVal = rtk_stat_port_getAll(port, &pPort_cntrs);

		if (retVal == RT_ERR_OK) {
			show_port_stat(&pPort_cntrs);
		} else {
			printk("rtk_stat_port_getAll() fail, return %d\n", retVal);
		}

		break;

	case RESET_PORT:
		copy_from_user(&port_user, (int __user *)arg, sizeof(int));
		port = port_user;
		printk("port reset(%d)\n", port);
		ENUM_PORT_BEGIN(port_nr, mask, 1U << port, 1)
			rtk_stat_port_reset(port_nr);
		ENUM_PORT_END

		break;

	case SET_EXT_TXDELAY:
		copy_from_user(&txDelay_user, (unsigned int __user *)arg, sizeof(unsigned int));
		printk("txDelay_user: %d\n# txDelay - TX delay value, 1 for delay 2ns and 0 for no-delay\n", txDelay_user);

		for (i = 0, p = &ext_port_tbl[0]; i < ARRAY_SIZE(ext_port_tbl); ++i, ++p) {
			printk("EXT_PORT:%d new txDelay: %d, rxDelay: %d\n", p->id, txDelay_user, rxDelay_user_ary[i]);
			retVal = rtk_port_rgmiiDelayExt_set(p->id, txDelay_user, rxDelay_user_ary[i]);
			if (retVal == RT_ERR_OK)
				txDelay_user_ary[i] = txDelay_user;
			else
				printk("rtk_port_rgmiiDelayExt_set(EXT_PORT:%d): return %d\n", p->id, retVal);
		}
		for (i = 0, p = &ext_port_tbl[0]; i < ARRAY_SIZE(ext_port_tbl); ++i, ++p) {
			rtk_port_rgmiiDelayExt_get(p->id, &txDelay_ro, &rxDelay_ro);
			printk("\ncurrent EXT_PORT:%d txDelay: %d, rxDelay: %d\n", p->id, txDelay_ro, rxDelay_ro);
		}

		break;

	case SET_EXT_RXDELAY:
		copy_from_user(&rxDelay_user, (unsigned int __user *)arg, sizeof(unsigned int));
		printk("rxDelay_user: %d\n# rxDelay - RX delay value, 0~7 for delay setup", rxDelay_user);

		for (i = 0, p = &ext_port_tbl[0]; i < ARRAY_SIZE(ext_port_tbl); ++i, ++p) {
			printk("EXT_PORT:%d new txDelay: %d, rxDelay: %d\n", p->id, txDelay_user_ary[i], rxDelay_user);
			retVal = rtk_port_rgmiiDelayExt_set(p->id, txDelay_user_ary[i], rxDelay_user);
			if (retVal == RT_ERR_OK)
				rxDelay_user_ary[i] = rxDelay_user;
			else
				printk("rtk_port_rgmiiDelayExt_set(EXT_PORT:%d): return %d\n", p->id, retVal);
		}
		for (i = 0, p = &ext_port_tbl[0]; i < ARRAY_SIZE(ext_port_tbl); ++i, ++p) {
			rtk_port_rgmiiDelayExt_get(p->id, &txDelay_ro, &rxDelay_ro);
			printk("\ncurrent EXT_PORT:%d txDelay: %d, rxDelay: %d\n", p->id, txDelay_ro, rxDelay_ro);
		}

		break;

	case GET_PORT_SPEED:	
		copy_from_user(&port_user, (int __user *)arg, sizeof(int));
		port = port_user;
		printk("get port %d speed...", port);

		retVal = rtk_port_phyStatus_get(port, &pLinkStatus, &pSpeed, &pDuplex);
        	if(retVal) printk("rtk_port_phyStatus_get Failed!(%d)\n", retVal);
        	else printk("%d\n", (unsigned int)pSpeed);

		put_user(pSpeed, (unsigned int __user *)arg);

		break;

	case POWERUP_LANPORTS:	
        	retVal = rtk_port_phyEnableAll_set(1);
        	if(retVal) printk("rtk port_phyEnableAll Failed!(%d)\n", retVal);
        	else printk("rtk port_phyEnableAll (on) ok\n");

		break;

	case BAD_ADDR_X:
		printk("something wrong\n");

		break;

	case POWERDOWN_LANPORTS:
        	retVal = rtk_port_phyEnableAll_set(0);
        	if(retVal) printk("rtk port_phyEnableAll Failed!(%d)\n", retVal);
        	else printk("rtk port_phyEnableAll (off) ok\n");

		break;

	case GET_RTK_PHYSTATES:
		{
			phyState pS;

			copy_from_user(&pS, (phyState __user *)arg, sizeof(pS));
			for (port = 0; port < 4; port++) {
				retVal = rtk_port_phyStatus_get(port, &pLinkStatus, &pSpeed, &pDuplex);
				pS.link[port] = pLinkStatus;
				pS.speed[port] = pSpeed;
			}
			copy_to_user((phyState __user *)arg, &pS, sizeof(pS));
		}

		break;

	case SOFT_RESET:
		printk("software reset " NAME "... (not yet)\n");
		msleep(1000);
		/*
		if ((retVal = rtl8367c_setAsicReg(0x1322, 1)) != RT_ERR_OK)
		{
			printk("error retVal(%d)\n", retVal);
			return -1;
		}
		*/
#if 0
		/* clear global variables */
		is_singtel_mio = 0;
		wan_stb_g = 0;
		voip_port_g = 0;
		vlan_vid = 0;
		vlan_prio = 0;
		initialize_switch();
#endif
		break;

	case GET_REG:	/* get rtk switch register */
		copy_from_user(&rtk_reg, (int __user *)arg, sizeof(rtk_uint32));

		if ((retVal = rtl8367c_getAsicReg(rtk_reg, &data)) != RT_ERR_OK)
		{
			printk("error retVal(%d) data(%x)\n", retVal, data);
			return -1;
		}
		printk("Get rtk switch reg[%x] = [%x]\n", rtk_reg, data);
		break;

	case TEST_REG:	/* set rtk switch register */
		test_t = 10000;	
	case SET_REG:	/* get rtk switch register */
		copy_from_user(&asic_reg, (rtk_asic_t __user *)arg, sizeof(rtk_asic_t));

		for(i = 0; i < test_t; ++i) {
			if ((retVal = rtl8367c_setAsicReg(asic_reg.rtk_reg, asic_reg.rtk_val)) != RT_ERR_OK)
			{
				printk("(set)[%d] error retVal(%d)\n", i, retVal);
				return -1;
			}
			if ((retVal = rtl8367c_getAsicReg(asic_reg.rtk_reg, &data)) != RT_ERR_OK)
			{
				printk("(get)[%d] error retVal(%d)\n", i, retVal);
				return -1;
			} else if (data != asic_reg.rtk_val){
				printk("(get)[%d] not match!(%x/%x)\n", i, data, asic_reg.rtk_val);
				return -1;
			}
		}
		printk("Set rtk switch reg[%x] = [%x] ok\n", asic_reg.rtk_reg, asic_reg.rtk_val);
		break;

	case SET_EXT_MODE:	/* 1: MODE_EXT_RGMII; */
		copy_from_user(&port_user, (int __user *)arg, sizeof(int));
		if(port_user < 0 || port_user >= MODE_EXT_END) {
			printk("invalid! \nMODE: MODE_EXT_DISABLE\t(0)\n");
			printk(" MODE_EXT_RGMII\t(1)\n");
			printk(" MODE_EXT_MII_MAC\t(2)\n");
			printk(" MODE_EXT_MII_PHY\t(3)\n");
			printk(" MODE_EXT_TMII_MAC\t(4)\n");
			printk(" MODE_EXT_TMII_PHY\t(5)\n");
			printk(" MODE_EXT_GMII\t(6)\n");
			printk(" MODE_EXT_RMII_MAC\t(7)\n");
			printk(" MODE_EXT_RMII_PHY\t(8)\n");
			printk(" MODE_EXT_SGMII\t(9)\n");
			printk(" MODE_EXT_HSGMII\t(10)\n");
			break;
		}
		port = port_user;
		printk("rtk set ext mode(%d)\n", port);
        	/* configure & set GMAC ports */
        	pa.forcemode       = MAC_FORCE;
        	pa.speed           = SPD_1000M;
        	pa.duplex          = FULL_DUPLEX;
		pa.link            = 1;
        	pa.nway            = 0;
        	pa.rxpause         = 0;
        	pa.txpause         = 0;

        	retVal = rtk_port_macForceLinkExt_set(EXT_PORT0, MODE_EXT_RGMII, &pa);
        	if(retVal) printk("set ext mode Failed!(%d)\n", retVal);
        	else printk("set ext mode ok\n");

		break;

	case GET_CPU:	
        	retVal = rtk_cpu_enable_get(&enable);
        	if(retVal) printk("rtk cpu_enable_get Failed!(%d)\n", retVal);
        	else printk("get cpu port func enable:%d\n", (int)enable);

		break;

	case SET_CPU:	
		copy_from_user(&port_user, (int __user *)arg, sizeof(int));
		enable = (rtk_enable_t)port_user;
		printk("set cpu port func %s ...\n", (int)enable?"on":"off");

        	retVal = rtk_cpu_enable_set(enable);
        	if(retVal) printk("rtk cpu_enable_set Failed!(%d)\n", retVal);
        	else printk("set cpu port func %s done\n", (int)enable?"on":"off");

		break;
		
	default:
		return -ENOIOCTLCMD;
	}

	return 0;
}

long rtl8367rb_ioctl(struct file *file, unsigned int req, unsigned long arg)
{
	long ret = 0;
	mutex_lock(&rtl8367rb_lock);
	ret = rtl8367rb_do_ioctl(file, req, arg);
	mutex_unlock(&rtl8367rb_lock);
	return ret;
}

int rtl8367rb_open(struct inode *inode, struct file *file)
{
	return 0;
}

int rtl8367rb_release(struct inode *inode, struct file *file)
{
	return 0;
}

struct file_operations rtl8367rb_fops =
{
	.owner = THIS_MODULE,
	.unlocked_ioctl = rtl8367rb_ioctl,
	.open = rtl8367rb_open,
	.release = rtl8367rb_release,
};

int __init rtl8367rb_init(void)
{
	int r = 0;
/* 
	unsigned int data;
*/
	msleep(1000);

	r = register_chrdev(RTL8365MB_MAJOR, RTL8367R_DEVNAME,
			&rtl8367rb_fops);
	if (r < 0) {
		printk(KERN_ERR NAME ": unable to register character device\n");
		return r;
	}

	initialize_switch();

	return 0;
}

void __exit rtl8367rb_exit(void)
{
	unregister_chrdev(RTL8365MB_MAJOR, RTL8367R_DEVNAME);

	printk(NAME " driver exited\n");
}

module_init(rtl8367rb_init);
module_exit(rtl8367rb_exit);
