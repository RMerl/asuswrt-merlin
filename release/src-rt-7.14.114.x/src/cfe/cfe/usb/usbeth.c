/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  USB Ethernet				File: usbeth.c
    *  
    *  Driver for USB Ethernet devices.
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */

/*  *********************************************************************
    *  USB-Ethernet driver - CFE Network Layer Interfaces
    *  NOTE: Some of the device setup for the Admtek & Realtek devices
    *  was derived from reverse engineering! So these interfaces
    *        assume proper device operation.
    ********************************************************************* */

#include "lib_types.h"
#include "lib_malloc.h"
#include "lib_string.h"
#include "lib_printf.h"

#include "cfe_iocb.h"
#include "cfe_ioctl.h"
#include "cfe_timer.h"
#include "cfe_device.h"
#include "cfe_devfuncs.h"
#include "cfe_error.h"

#include "usbd.h"
#include "usbeth.h"

#define USBETH_TRACE( x, y ... )

#define FAIL				-1

static int Dev_cnt = 0;


/******************************************************************************
  Debug functions
******************************************************************************/

//#define DATA_DUMP
#ifdef DATA_DUMP
static void hexdump( unsigned char * src, int srclen, int rowlen, int rows )
{
	unsigned char * rowptr;
	unsigned char * srcstp;
	unsigned char * byteptr;

	srcstp = src + srclen;

	for( rowptr = src; rowptr < src + rowlen * rows; rowptr += rowlen ) {
		for( byteptr = rowptr; byteptr < rowptr + rowlen && byteptr < srcstp; byteptr++ ) {
			xprintf( "%2X ", *byteptr );
		}
		xprintf( "\n" );
	}
	xprintf( "\n" );
}
#endif


/*  *********************************************************************
    * Interface functions for USB-Ethernet adapters
    ********************************************************************* */

enum { PEGASUS, PEGASUS_II, NETMATE, REALTEK };
enum { VEN_NONE, _3_COM, LINKSYS, LINKSYS_10, LINKSYS_100,
	   CATC_NM, BELKIN_CATC, LINKSYS_100M };
static char * VENDOR_NAMES[] =
{
	"?", "3-COM", "LinkSys", "LinkSys-10TX", "LinkSys-100TX",
	"CATC-Netmate", "Belkin/CATC", "Linksys-100M", "Yikes!"
};

typedef struct usbeth_softc_s
{
	usbdev_t *dev;
	int bulk_inpipe;
	int bulk_outpipe;
	int dev_id;
	int ven_code;
	int embed_tx_len;
	uint8_t mac_addr[6];
	usbreq_t *rx_ur;
	uint8_t rxbuf[1600];	//artbitrary but enough for ethernet packet
} usbeth_softc_t;


/* **************************************
   *  CATC I/F Functions
   ************************************** */


static int catc_set_reg( usbdev_t *dev, int16_t reg, int16_t val )
{
	return usb_std_request( dev, (USBREQ_TYPE_VENDOR | USBREQ_DIR_OUT),
	                        CATC_SET_REG, val, reg, NULL, 0 );
}

static int catc_set_mem( usbdev_t *dev, int16_t addr,
						  uint8_t *data, int16_t len )
{
	return usb_std_request( dev, (USBREQ_TYPE_VENDOR | USBREQ_DIR_OUT),
	                        CATC_SET_MEM, 0, addr, data, len );
}

static int catc_get_mac_addr( usbdev_t *dev, uint8_t *mac_addr )
{
	int status;

	status = usb_std_request( dev, (USBREQ_TYPE_VENDOR | USBREQ_DIR_IN),
	                          CATC_GET_MAC_ADDR, 0, 0, mac_addr, 6 );
	return( status );
}

static void catc_init_device( usbeth_softc_t * softc )
{
	usbdev_t *dev = softc->dev;
	unsigned char *mcast_tbl;

	usb_std_request( dev, (USBREQ_TYPE_STD | USBREQ_REC_INTERFACE),
	                 USB_REQUEST_SET_INTERFACE,
	                 1,         //alt setting 1
	                 0, NULL, 0 );

	catc_set_reg(dev, CATC_TX_BUF_CNT_REG, 0x04 );
	catc_set_reg(dev, CATC_RX_BUF_CNT_REG, 0x10 );
	catc_set_reg(dev, CATC_ADV_OP_MODES_REG, 0x01 );
	catc_set_reg(dev, CATC_LED_CTRL_REG, 0x08 );

	/* Enable broadcast rx via bit in mutlicast table */
	mcast_tbl = KMALLOC(64, 0);
	memset( mcast_tbl, 0, 64 );
	mcast_tbl[31] = 0x80;     //i.e. broadcast bit
	catc_set_mem( dev, CATC_MCAST_TBL_ADDR, mcast_tbl, 64 );
	KFREE(mcast_tbl);

	//Read the adapter's MAC addr
	catc_get_mac_addr( dev, softc->mac_addr );
}

static void catc_close_device( usbdev_t *dev )
{
	// Now disable adapter from receiving packets
	catc_set_reg( dev, CATC_ETH_CTRL_REG, 0 );
}

static void catc_open_device( usbeth_softc_t * softc )
{
	int i;

	for(i = 0; i < 6; ++i)
		catc_set_reg( softc->dev, (CATC_ETH_ADDR_0_REG - i), softc->mac_addr[i] );

	// Now enable adapter to receive packets
	catc_set_reg( softc->dev, CATC_ETH_CTRL_REG, 0x09 );
}

/* **************************************
   *  PEGASUS I/F Functions
   ************************************** */

static int peg_get_reg( usbdev_t *dev, int16_t reg, uint8_t *val, int16_t len )
{
	return usb_std_request( dev, (USBREQ_TYPE_VENDOR | USBREQ_DIR_IN),
	                        PEG_GET_REG, 0, reg, val, len );
}

static int peg_set_reg( usbdev_t *dev, int16_t reg, int16_t val )
{
	unsigned char data = (uint8_t) val & 0xff;

	return usb_std_request( dev, (USBREQ_TYPE_VENDOR | USBREQ_DIR_OUT),
	                        PEG_SET_REG, val, reg, &data, 1 );
}

static int peg_set_regs( usbdev_t *dev, int16_t reg, int8_t *vals, int16_t len )
{
	return usb_std_request( dev, (USBREQ_TYPE_VENDOR | USBREQ_DIR_OUT),
	                        PEG_SET_REG, 0, reg, vals, len );
}

static int peg_get_eep_word( usbdev_t *dev, int16_t ofs, uint8_t *val )
{
	int status=0, tries=20;
	uint8_t data[2];

	if( peg_set_reg( dev, PEG_EEPROM_CTL_REG, 0 ) == FAIL )
		return FAIL;
	if( peg_set_reg( dev, PEG_EEPROM_OFS_REG, ofs ) == FAIL )
		return FAIL;
	if( peg_set_reg( dev, PEG_EEPROM_CTL_REG, 0x02 ) == FAIL )	//read
		return FAIL;
	while( --tries )
	{
		if( peg_get_reg( dev, PEG_EEPROM_CTL_REG, data, 1 ) == FAIL )
			return FAIL;
		if( data[0] & 0x04 )
			break;	//eeprom data ready
	}
	if( !tries )
	{
		xprintf( "Pegasus Eeprom read failed!\n" );
		return FAIL;
	}
	if( peg_get_reg( dev, PEG_EEPROM_DATA_REG, data, 2 ) == FAIL )
		return FAIL;
	val[0] = data[0];
	val[1] = data[1];

	return( status );
}

static int peg_get_mac_addr( usbdev_t *dev, uint8_t *mac_addr )
{
	int i, status;

	for( i = 0; i < 3; ++i )
	{
		status = peg_get_eep_word( dev, i, &mac_addr[i*2] );
	}
	return( status );
}

static void peg_init_phy( usbdev_t *dev )
{	//needed for earlier versions (before Rev B) of the USB-100TX adapters
	static uint8_t phy_magic_wr[] = { 0, 4, 0, 0x1b };
	static uint8_t read_status[] = { 0, 0, 0, 1 };
	uint8_t data[4];

	//reset the MAC ans set up GPIOs
	peg_set_reg( dev, PEG_ETH_CTL1_REG, 0x08 );
	peg_get_reg( dev, PEG_ETH_CTL1_REG, data, 1 );

	//do following steps to enable link activitiy LED
	peg_set_reg( dev, PEG_GPIO1_REG, 0x26 );
	peg_set_reg( dev, PEG_GPIO0_REG, 0x24 );
	peg_set_reg( dev, PEG_GPIO0_REG, 0x26 );

	//do following set of steps to enable LINK LED
	memcpy( data, phy_magic_wr, 4 );
	peg_set_regs( dev, PEG_PHY_ADDR_REG, data, 4);	//set up for magic word
	peg_set_reg( dev, PEG_PHY_CTRL_REG, (0x1b|PHY_WRITE) );
	peg_get_reg( dev, PEG_PHY_CTRL_REG, data, 1 );	//read status of write
	memcpy( data, read_status, 4 );
	peg_set_regs( dev, PEG_PHY_ADDR_REG, data, 4);	//set up for phy status reg
	peg_set_reg( dev, PEG_PHY_CTRL_REG, (1|PHY_READ) );
	peg_get_reg( dev, PEG_PHY_CTRL_REG, data, 1 );	//read status of read
	peg_get_reg( dev, PEG_PHY_DATA_REG, data, 2 );	//read status regs
}

static void peg_init_device( usbeth_softc_t * softc )
{
	usbdev_t *dev = softc->dev;

	if( softc->dev_id == PEGASUS_II )
		peg_set_reg( dev, PEG_INT_PHY_REG, 0x02 );	//enable internal PHY
	else
		peg_init_phy( dev );

	//Read the adapter's MAC addr
	peg_get_mac_addr( dev, softc->mac_addr );
}

static void peg_close_device( usbdev_t *dev )
{
	//Now disable adapter from receiving or transmitting packets
	peg_set_reg( dev, PEG_ETH_CTL1_REG, 0 );
}

static void peg_open_device( usbeth_softc_t * softc )
{
	usbdev_t *dev = softc->dev;

	//Now setup adapter's receiver with MAC address
	peg_set_regs( dev, PEG_MAC_ADDR_0_REG, softc->mac_addr, 6 );

	//Now enable adapter to receive and transmit packets
	peg_set_reg( dev, PEG_ETH_CTL0_REG, 0xc1 );
	peg_set_reg( dev, PEG_ETH_CTL1_REG, 0x30 );
}

/* **************************************
   *  REALTEK I/F Functions
   ************************************** */

//
// ********** NOT FULLY WORKING YET!!!!!!!!!! ***************
//

static int rtek_get_reg( usbdev_t *dev, int16_t reg, uint8_t *val, int16_t len )
{
	return usb_std_request( dev, (USBREQ_TYPE_VENDOR | USBREQ_DIR_IN),
	                        RTEK_REG_ACCESS, reg, 0, val, len );
}

static int rtek_set_reg( usbdev_t *dev, int16_t reg, int16_t val )
{
	unsigned char data = (uint8_t) val & 0xff;

	return usb_std_request( dev, (USBREQ_TYPE_VENDOR | USBREQ_DIR_OUT),
	                        RTEK_REG_ACCESS, reg, 0, &data, 1 );
}

static int rtek_get_mac_addr( usbdev_t *dev, uint8_t *mac_addr )
{
	int status;

	status = rtek_get_reg( dev, RTEK_MAC_REG, mac_addr, 6 );

	return( status );
}

static void rtek_init_device( usbeth_softc_t * softc )
{
	int i;
	usbdev_t *dev = softc->dev;
	uint8_t val;

	//Reset the adapter
	rtek_set_reg( dev, RTEK_CMD_REG, RTEK_RESET );
	for( i = 0; i < 10; ++i )
	{
		rtek_get_reg( dev, RTEK_CMD_REG, &val, 1 );
		if( !(val & RTEK_RESET) )
			break;
		usb_delay_ms( NULL, 1 );
	}

	//autoload the internal registers
	rtek_set_reg( dev, RTEK_CMD_REG, RTEK_AUTOLOAD );
	for( i = 0; i < 50; ++i )
	{
		rtek_get_reg( dev, RTEK_CMD_REG, &val, 1 );
		if( !(val & RTEK_AUTOLOAD) )
			break;
		usb_delay_ms( NULL, 1 );
	}

	//Read the adapter's MAC addr
	rtek_get_mac_addr( dev, softc->mac_addr );
}

static void rtek_close_device( usbdev_t *dev )
{
	//Now disable adapter from receiving or transmitting packets
	rtek_set_reg( dev, RTEK_CMD_REG, 0 );
}

static void rtek_open_device( usbeth_softc_t * softc )
{
	//accept broadcast & own packets
	rtek_set_reg( softc->dev, RTEK_RXCFG_REG, 0x0c );

	//Now enable adapter to receive and transmit packets
	rtek_set_reg( softc->dev, RTEK_CMD_REG, 0x0c );
}

//*********************** USB-ETH I/F Functions ****************************

static const int ID_TBL[] =
{
	0x0506, 0x4601, PEGASUS_II, _3_COM,			//3-Com
	0x066b, 0x2202, PEGASUS_II, LINKSYS_10,		//LinkSys
	0x066b, 0x2203, PEGASUS,    LINKSYS_100,
	0x066b, 0x2204, PEGASUS,    LINKSYS_100,
	0x066b, 0x2206, PEGASUS,    LINKSYS,
	0x066b, 0x400b, PEGASUS_II, LINKSYS_10,
	0x066b, 0x200c, PEGASUS_II, LINKSYS_10,
	0x0bda, 0x8150, REALTEK,    LINKSYS_100M,
	0x0423, 0x000a, NETMATE,    CATC_NM,		//CATC (Netmate I)
	0x0423, 0x000c, NETMATE,    BELKIN_CATC,	//Belkin & CATC (Netmate II)
	-1
};

static int usbeth_init_device( usbeth_softc_t * softc )
{
	int i;
	usb_device_descr_t dev_desc;
	uint16_t vendor_id, device_id;
	const int *ptr=ID_TBL;

	//find out which device is connected
	usb_get_device_descriptor( softc->dev, &dev_desc, 0 );
	vendor_id = (dev_desc.idVendorHigh  << 8) + dev_desc.idVendorLow;
	device_id = (dev_desc.idProductHigh << 8) + dev_desc.idProductLow;
	xprintf( "USB device:  vendor id %04x, device id %04x\n",
	         vendor_id, device_id );

	while( *ptr != -1 )
	{
		if( (vendor_id == ptr[0]) && (device_id == ptr[1]) )
		{
			softc->dev_id = ptr[2];
			softc->ven_code = ptr[3];
			break;
		}
		ptr += 4;
	}
	if( *ptr == -1 )
	{
		xprintf( "Unrecognized USB-Ethernet device\n" );
		return -1;
	}

	//init the adapter
	if( softc->dev_id == NETMATE )
	{
		catc_init_device( softc );
		softc->embed_tx_len = 1;
	}
	else
	{
		if( softc->dev_id == REALTEK )
		{
			rtek_init_device( softc );
			softc->embed_tx_len = 0;
		}
		else
		{
			peg_init_device( softc );
			softc->embed_tx_len = 1;
		}
	}

	//display adapter info
	xprintf( "%s USB-Ethernet Adapter  (", VENDOR_NAMES[softc->ven_code] );
	for( i = 0; i < 6; ++i )
		xprintf( "%02x%s", softc->mac_addr[i], (i == 5) ? ")\n" : ":" );

	return 0;
}

static int usbeth_get_dev_addr( usbeth_softc_t * softc, uint8_t *mac_addr )
{
	memcpy( mac_addr, softc->mac_addr, 6 );
	return 0;
}

static void usbeth_queue_rx( usbeth_softc_t * softc )
{
	softc->rx_ur = usb_make_request(softc->dev, softc->bulk_inpipe,
	                                softc->rxbuf, sizeof(softc->rxbuf),
	                                (UR_FLAG_IN | UR_FLAG_SHORTOK));
	usb_queue_request(softc->rx_ur);
}

static void usbeth_close_device( usbeth_softc_t * softc )
{
	if( softc->dev_id == NETMATE )
		catc_close_device( softc->dev );
	else if( softc->dev_id == REALTEK )
		rtek_close_device( softc->dev );
	else
		peg_close_device( softc->dev );
}

static void usbeth_open_device( usbeth_softc_t * softc )
{
	if( softc->dev_id == NETMATE )
		catc_open_device( softc );
	else if( softc->dev_id == REALTEK )
		rtek_open_device( softc );
	else
		peg_open_device( softc );

	//kick start the receive
	usbeth_queue_rx( softc );
}

static int usbeth_data_rx( usbeth_softc_t * softc )
{
	usb_poll(softc->dev->ud_bus);
	return( !softc->rx_ur->ur_inprogress );
}

static int usbeth_get_eth_frame( usbeth_softc_t * softc, unsigned char * buf )
{
	int len = 0;

	if( !softc->rx_ur->ur_inprogress )
	{
		len = softc->rx_ur->ur_xferred;
		memcpy( buf, softc->rxbuf, len );
		usb_free_request(softc->rx_ur);
		usbeth_queue_rx( softc );
	}
	else
		xprintf( "Bulk data is not available yet!\n" );

	return( len );
}

static int usbeth_send_eth_frame( usbeth_softc_t * softc, unsigned char * buf, int len )
{
	usbreq_t *ur;
	int txlen = len;
	unsigned char * txbuf;

	if(softc->embed_tx_len)
	{
		txbuf = KMALLOC((len+2), 0);
		txbuf[0] = txlen & 0xff;
		txbuf[1] = (txlen >> 8) & 0xff;		//1st two bytes...little endian
		memcpy( &txbuf[2], buf, txlen );
		txlen += 2;
	}
	else
	{
		if( softc->dev_id == REALTEK )
		{
			//Now for some Realtek chip workarounds
			if( txlen < 60 )			//some strange limitation
				txlen = 60;
			else if( !(txlen % 64) )	//to handle module 64 packets
				++txlen;
		}
		txbuf = KMALLOC(txlen, 0);
		memcpy( txbuf, buf, txlen );
	}
	ur = usb_make_request(softc->dev, softc->bulk_outpipe,
	                      txbuf, txlen, UR_FLAG_OUT);
	usb_sync_request(ur);
	usb_free_request(ur);
	KFREE(txbuf);

	return( len );
}


/*  *********************************************************************
    * CFE-USB interfaces
    ********************************************************************* */

/*  *********************************************************************
    *  usbeth_attach(dev,drv)
    *
    *  This routine is called when the bus scan stuff finds a usb-ethernet
    *  device.  We finish up the initialization by configuring the
    *  device and allocating our softc here.
    *
    *  Input parameters:
    *      dev - usb device, in the "addressed" state.
    *      drv - the driver table entry that matched
    *
    *  Return value:
    *      0
    ********************************************************************* */

const cfe_driver_t usbethdrv;		//forward declaration

static int usbeth_attach(usbdev_t *dev,usb_driver_t *drv)
{
	usb_config_descr_t *cfgdscr = dev->ud_cfgdescr;
	usb_endpoint_descr_t *epdscr;
	usb_endpoint_descr_t *indscr = NULL;
	usb_endpoint_descr_t *outdscr = NULL;
	usb_interface_descr_t *ifdscr;
	usbeth_softc_t *softc;
	int idx;

	dev->ud_drv = drv;

	softc = (usbeth_softc_t *) KMALLOC( sizeof(usbeth_softc_t), 0 );
	if( softc == NULL )
	{
		xprintf( "Failed to allocate softc memory.\n" );
		return -1;
	}
	memset( softc, 0, sizeof(usbeth_softc_t) );
	dev->ud_private = softc;
	softc->dev = dev;

	ifdscr = usb_find_cfg_descr(dev,USB_INTERFACE_DESCRIPTOR_TYPE,0);
	if (ifdscr == NULL)
	{
		xprintf("USBETH: ERROR...no interace descriptor\n");
		return -1;
	}


	for (idx = 0; idx < 2; idx++)
	{
		epdscr = usb_find_cfg_descr(dev,USB_ENDPOINT_DESCRIPTOR_TYPE,idx);
		if (USB_ENDPOINT_DIR_OUT(epdscr->bEndpointAddress))
			outdscr = epdscr;
		else
			indscr = epdscr;
	}

	if (!indscr || !outdscr)
	{
		/*
		 * Could not get descriptors, something is very wrong.
		 * Leave device addressed but not configured.
		 */
		xprintf("USBETH: ERROR...no endpoint descriptors\n");
		return -1;
	}

	/*
	 * Choose the standard configuration.
	 */

	usb_set_configuration(dev,cfgdscr->bConfigurationValue);

	// Quit if not able to initialize the device
	if (usbeth_init_device(softc) < 0)
		return -1;

	/*
	 * Open the pipes.
	 */

	softc->bulk_inpipe     = usb_open_pipe(dev,indscr);
	softc->bulk_outpipe    = usb_open_pipe(dev,outdscr);

	//Now attach this device as a CFE Ethernet device
	cfe_attach( (cfe_driver_t *) &usbethdrv, softc, NULL,
	            usbethdrv.drv_description );

	++Dev_cnt;

	return 0;
}

/*  *********************************************************************
    *  usbeth_detach(dev)
    *
    *  This routine is called when the bus scanner notices that
    *  this device has been removed from the system.  We should
    *  do any cleanup that is required.  The pending requests
    *  will be cancelled automagically.
    *
    *  Input parameters:
    *      dev - usb device
    *
    *  Return value:
    *      0
    ********************************************************************* */

static int usbeth_detach(usbdev_t *dev)
{
	usbeth_softc_t *softc = (usbeth_softc_t *) dev->ud_private;

	--Dev_cnt;
	KFREE(softc);

	//*** SHOULD DETACH THE ETHERNET DEVICE TOO...LATER

	return 0;
}

// CFE USB device interface structure
usb_driver_t usbeth_driver =
{
	"Ethernet Device",
	usbeth_attach,
	usbeth_detach
};



/*  *********************************************************************
    * CFE-Ethernet device interfaces
    ********************************************************************* */


static int usbeth_ether_open(cfe_devctx_t *ctx)
{
	if( !Dev_cnt )
		return CFE_ERR_NOTREADY;

	USBETH_TRACE( "%s called.\n", __FUNCTION__ );
	usbeth_open_device( (usbeth_softc_t *) ctx->dev_softc );

	return 0;
}

static int usbeth_ether_read( cfe_devctx_t * ctx, iocb_buffer_t * buffer )
{
	if( !Dev_cnt )
		return CFE_ERR_NOTREADY;

	buffer->buf_retlen = usbeth_get_eth_frame( (usbeth_softc_t *)ctx->dev_softc,
	                                           buffer->buf_ptr );

#ifdef DATA_DUMP
	xprintf( "Incoming packet :\n" );
	hexdump( buffer->buf_ptr, buffer->buf_retlen, 16,
	         buffer->buf_retlen / 16 + 1 );
#endif

	return 0;
}


static int usbeth_ether_inpstat( cfe_devctx_t * ctx, iocb_inpstat_t * inpstat )
{
	if( !Dev_cnt )
		return CFE_ERR_NOTREADY;

	inpstat->inp_status = usbeth_data_rx( (usbeth_softc_t *) ctx->dev_softc );

	return 0;
}


static int usbeth_ether_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
	if( !Dev_cnt )
		return CFE_ERR_NOTREADY;

	// Block until hw notifies you data is sent.
	usbeth_send_eth_frame( (usbeth_softc_t *) ctx->dev_softc, buffer->buf_ptr,
	                       buffer->buf_length );

	return 0;
}


static int usbeth_ether_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
	int retval = 0;

	if( !Dev_cnt )
		return CFE_ERR_NOTREADY;

	switch( (int)buffer->buf_ioctlcmd ) {
		case IOCTL_ETHER_GETHWADDR:
			USBETH_TRACE( "IOCTL_ETHER_GETHWADDR called.\n" );
			usbeth_get_dev_addr( (usbeth_softc_t *) ctx->dev_softc,
								 buffer->buf_ptr );
			break;
		case IOCTL_ETHER_SETHWADDR:
			xprintf( "IOCTL_ETHER_SETHWADDR not implemented.\n" );
			break;
		default:
			xprintf( "Invalid IOCTL to usbeth_ether_ioctl.\n" );
			retval = -1;
	}

	return retval;
}


static int usbeth_ether_close(cfe_devctx_t *ctx)
{
	if( !Dev_cnt )
		return CFE_ERR_NOTREADY;

	USBETH_TRACE( "%s called.\n", __FUNCTION__ );
	usbeth_close_device( (usbeth_softc_t *) ctx->dev_softc );

	return 0;
}


// CFE ethernet device interface structures
const static cfe_devdisp_t usbeth_ether_dispatch =
{
	usbeth_ether_open,
	usbeth_ether_read,
	usbeth_ether_inpstat,
	usbeth_ether_write,
	usbeth_ether_ioctl,
	usbeth_ether_close,
	NULL,
	NULL
};

const cfe_driver_t usbethdrv =
{
	"USB-Ethernet Device",
	"eth",
	CFE_DEV_NETWORK,
	&usbeth_ether_dispatch,
	NULL,						//probe...not needed
};
