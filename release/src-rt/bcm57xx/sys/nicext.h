/****************************************************************************
 * Copyright(c) 2000-2004 Broadcom Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Name:        nicext.h
 *
 * Description: Broadcom Network Interface Card Extension (NICE) is an
 *              extension to Linux NET device kernel mode drivers.
 *              NICE is designed to provide additional functionalities,
 *              such as receive packet intercept. To support Broadcom NICE,
 *              the network device driver can be modified by adding an
 *              device ioctl handler and by indicating receiving packets
 *              to the NICE receive handler. Broadcom NICE will only be
 *              enabled by a NICE-aware intermediate driver, such as
 *              Broadcom Advanced Server Program Driver (BASP). When NICE
 *              is not enabled, the modified network device drivers
 *              functions exactly as other non-NICE aware drivers.
 *
 * Author:      Frankie Fan
 *
 * Created:     September 17, 2000
 *
 ****************************************************************************/
#ifndef _nicext_h_
#define _nicext_h_

/*
 * ioctl for NICE
 */
#define SIOCNICE                    SIOCDEVPRIVATE+7

/*
 * SIOCNICE:
 *
 * The following structure needs to be less than IFNAMSIZ (16 bytes) because
 * we're overloading ifreq.ifr_ifru.
 *
 * If 16 bytes is not enough, we should consider relaxing this because
 * this is no field after ifr_ifru in the ifreq structure. But we may
 * run into future compatiability problem in case of changing struct ifreq.
 */
struct nice_req
{
    __u32 cmd;

    union
    {
#ifdef __KERNEL__
        /* cmd = NICE_CMD_SET_RX or NICE_CMD_GET_RX */
        /* cmd = NICE_CMD_SET_RX_NAPI or NICE_CMD_GET_RX_NAPI */
        struct
        {
            void (*nrqus1_rx)( struct sk_buff*, void* );
            void* nrqus1_ctx;
        } nrqu_nrqus1;

        /* cmd = NICE_CMD_QUERY_SUPPORT */
        struct
        {
            __u32 nrqus2_magic;
            __u32 nrqus2_support_rx:1;
            __u32 nrqus2_support_vlan:1;
            __u32 nrqus2_support_get_speed:1;
            __u32 nrqus2_support_rx_napi:1;
        } nrqu_nrqus2;
#endif

        void *align_ptr;  /* this field is not used, it is to align the union */
                          /* in 64-bit user mode */

        /* cmd = NICE_CMD_GET_SPEED - in Mbps or 0 if link down */
        /* cmd = NICE_CMD_ENABLE_EXT_LOOPBACK  - in Mbps */
        struct
        {
            unsigned int nrqus3_speed;   /* 1000 = Gb, 100 = 100mbs, 10 = 10mbs */
        } nrqu_nrqus3;

        /* cmd = NICE_CMD_BLINK_LED */
        struct
        {
            unsigned int nrqus4_blink_time; /* blink duration in seconds */
        } nrqu_nrqus4;

        /* cmd = NICE_CMD_REG_READ */
        /* cmd = NICE_CMD_REG_WRITE */
        /* cmd = NICE_CMD_MEM_READ */
        /* cmd = NICE_CMD_MEM_WRITE */
        /* cmd = NICE_CMD_REG_READ_DIRECT */
        /* cmd = NICE_CMD_REG_WRITE_DIRECT */
        /* cmd = NICE_CMD_CFG_READ32 */
        /* cmd = NICE_CMD_CFG_READ16 */
        /* cmd = NICE_CMD_CFG_READ8 */
        /* cmd = NICE_CMD_CFG_WRITE32 */
        /* cmd = NICE_CMD_CFG_WRITE16 */
        /* cmd = NICE_CMD_CFG_WRITE8 */
        struct
        {
            unsigned int nrqus5_offset; /* offset */
            unsigned int nrqus5_data; /* value */
        } nrqu_nrqus5;

        /* cmd = NICE_CMD_INTERRUPT_TEST */
        struct
        {
            unsigned int nrqus6_intr_test_result; /* 1 == pass */
        } nrqu_nrqus6;

        /* cmd = NICE_CMD_KMALLOC_PHYS */
        /* cmd = NICE_CMD_KFREE_PHYS */
        /* These commands allow the diagnostics app. to allocate and free */
        /* PCI consistent memory for DMA tests */
       struct
        {
            unsigned int nrqus7_size; /* size(bytes) to allocate, not used    */
                                      /* when cmd is NICE_CMD_KFREE_PHYS.     */
            __u32 nrqus7_phys_addr_lo;/* CPU physical address allocated or    */
            __u32 nrqus7_phys_addr_hi;/* to be freed.                         */
                                      /* PCI physical address is contained in */
                                      /* the 1st 64 bit of the allocated      */
                                      /* buffer. Use open("/dev/mem")/lseek() */
                                      /* and read()/write() to a access       */
                                      /* buffer in user space.                */
                                      /* mmap() only works on x86.            */
        } nrqu_nrqus7;

        /* cmd = NICE_CMD_SET_WRITE_PROTECT */
        struct
        {
            unsigned int nrqus8_data; /* 1 == set write protect, 0 == clear write protect */
        } nrqu_nrqus8;

        /* cmd = NICE_CMD_GET_STATS_BLOCK */
        struct
        {
            void *nrqus9_useraddr; /* user space address for the stats block */
            unsigned int nrqus9_size;/* size (in bytes)                      */
                                     /* (0x6c0 for the whole block)          */
        } nrqu_nrqus9;

        /* cmd = NICE_CMD_LOOPBACK_TEST */
        struct
        {
            unsigned char nrqus10_looptype;
            unsigned char nrqus10_loopspeed;
        } nrqu_nrqus10;

         /* cmd = NICE_CMD_KMAP_PHYS/KUNMAP_PHYS */
        struct
        {
            int nrqus11_rw;	      /* direction                            */
	    void *nrqus11_uaddr;      /* ptr to mem allocated in user space   */
                                      /* when cmd is NICE_CMD_KFREE_PHYS.     */
            __u32 nrqus11_phys_addr_lo;/* CPU physical address allocated or    */
            __u32 nrqus11_phys_addr_hi;/* to be freed.                         */
                                      /* PCI physical address is contained in */
                                      /* the 1st 64 bit of the allocated      */
                                      /* buffer. Use open("/dev/mem")/lseek() */
                                      /* and read()/write() to a access       */
                                      /* buffer in user space.                */
                                      /* mmap() only works on x86.            */
        } nrqu_nrqus11;
   } nrq_nrqu;
};

#define nrq_rx           nrq_nrqu.nrqu_nrqus1.nrqus1_rx
#define nrq_ctx          nrq_nrqu.nrqu_nrqus1.nrqus1_ctx
#define nrq_support_rx   nrq_nrqu.nrqu_nrqus2.nrqus2_support_rx
#define nrq_magic        nrq_nrqu.nrqu_nrqus2.nrqus2_magic
#define nrq_support_vlan nrq_nrqu.nrqu_nrqus2.nrqus2_support_vlan
#define nrq_support_get_speed nrq_nrqu.nrqu_nrqus2.nrqus2_support_get_speed
#define nrq_support_rx_napi nrq_nrqu.nrqu_nrqus2.nrqus2_support_rx_napi
#define nrq_speed        nrq_nrqu.nrqu_nrqus3.nrqus3_speed
#define nrq_blink_time   nrq_nrqu.nrqu_nrqus4.nrqus4_blink_time
#define nrq_offset       nrq_nrqu.nrqu_nrqus5.nrqus5_offset
#define nrq_data         nrq_nrqu.nrqu_nrqus5.nrqus5_data
#define nrq_intr_test_result  nrq_nrqu.nrqu_nrqus6.nrqus6_intr_test_result

#define nrq_size         nrq_nrqu.nrqu_nrqus7.nrqus7_size
#define nrq_phys_addr_lo nrq_nrqu.nrqu_nrqus7.nrqus7_phys_addr_lo
#define nrq_phys_addr_hi nrq_nrqu.nrqu_nrqus7.nrqus7_phys_addr_hi

#define nrq_rw           nrq_nrqu.nrqu_nrqus11.nrqus11_rw
#define nrq_puaddr	 nrq_nrqu.nrqu_nrqus11.nrqus11_uaddr
#define nrq_phys_add_lo nrq_nrqu.nrqu_nrqus11.nrqus11_phys_addr_lo
#define nrq_phys_add_hi nrq_nrqu.nrqu_nrqus11.nrqus11_phys_addr_hi

#define nrq_write_protect nrq_nrqu.nrqu_nrqus8.nrqus8_data
#define nrq_stats_useraddr nrq_nrqu.nrqu_nrqus9.nrqus9_useraddr
#define nrq_stats_size    nrq_nrqu.nrqu_nrqus9.nrqus9_size

#define nrq_looptype    nrq_nrqu.nrqu_nrqus10.nrqus10_looptype
#define nrq_loopspeed   nrq_nrqu.nrqu_nrqus10.nrqus10_loopspeed

/*
 * magic constants
 */
#define NICE_REQUESTOR_MAGIC            0x4543494E // NICE in ascii
#define NICE_DEVICE_MAGIC               0x4E494345 // ECIN in ascii

#define NICE_LOOPBACK_TESTTYPE_MAC      0x1
#define NICE_LOOPBACK_TESTTYPE_PHY      0x2
#define NICE_LOOPBACK_TESTTYPE_EXT      0x4

#define NICE_LOOPBACK_TEST_SPEEDMASK    0x3
#define NICE_LOOPBACK_TEST_10MBPS       0x1
#define NICE_LOOPBACK_TEST_100MBPS      0x2
#define NICE_LOOPBACK_TEST_1000MBPS     0x3


/*
 * command field
 */
typedef enum {
    NICE_CMD_QUERY_SUPPORT         = 0x00000001,
    NICE_CMD_SET_RX                = 0x00000002,
    NICE_CMD_GET_RX                = 0x00000003,
    NICE_CMD_GET_SPEED             = 0x00000004,
    NICE_CMD_BLINK_LED             = 0x00000005,
    NICE_CMD_DIAG_SUSPEND          = 0x00000006,
    NICE_CMD_DIAG_RESUME           = 0x00000007,
    NICE_CMD_REG_READ              = 0x00000008,
    NICE_CMD_REG_WRITE             = 0x00000009,
    NICE_CMD_MEM_READ              = 0x0000000a,
    NICE_CMD_MEM_WRITE             = 0x0000000b,
    NICE_CMD_ENABLE_MAC_LOOPBACK   = 0x0000000c,
    NICE_CMD_DISABLE_MAC_LOOPBACK  = 0x0000000d,
    NICE_CMD_ENABLE_PHY_LOOPBACK   = 0x0000000e,
    NICE_CMD_DISABLE_PHY_LOOPBACK  = 0x0000000f,
    NICE_CMD_INTERRUPT_TEST        = 0x00000010,
    NICE_CMD_SET_WRITE_PROTECT     = 0x00000011,
    NICE_CMD_SET_RX_NAPI           = 0x00000012,
    NICE_CMD_GET_RX_NAPI           = 0x00000013,
    NICE_CMD_ENABLE_EXT_LOOPBACK   = 0x00000014,
    NICE_CMD_DISABLE_EXT_LOOPBACK  = 0x00000015,
    NICE_CMD_CFG_READ32            = 0x00000016,
    NICE_CMD_CFG_READ16            = 0x00000017,
    NICE_CMD_CFG_READ8             = 0x00000018,
    NICE_CMD_CFG_WRITE32           = 0x00000019,
    NICE_CMD_CFG_WRITE16           = 0x0000001a,
    NICE_CMD_CFG_WRITE8            = 0x0000001b,

    NICE_CMD_REG_READ_DIRECT       = 0x0000001e,
    NICE_CMD_REG_WRITE_DIRECT      = 0x0000001f,
    NICE_CMD_RESET                 = 0x00000020,
    NICE_CMD_KMALLOC_PHYS          = 0x00000021,
    NICE_CMD_KFREE_PHYS            = 0x00000022,
    NICE_CMD_GET_STATS_BLOCK       = 0x00000023,
    NICE_CMD_CLR_STATS_BLOCK       = 0x00000024,
    NICE_CMD_LOOPBACK_TEST         = 0x00000025,
    NICE_CMD_KMAP_PHYS    	   = 0x00000026,
    NICE_CMD_KUNMAP_PHYS           = 0x00000027,
    NICE_CMD_MAX
} nice_cmds;

#endif  // _nicext_h_
