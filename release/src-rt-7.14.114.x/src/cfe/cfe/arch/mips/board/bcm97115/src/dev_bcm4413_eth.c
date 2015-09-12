#include "lib_types.h"
#include "lib_malloc.h"
#include "lib_string.h"
#include "lib_printf.h"

#include "cfe_iocb.h"
#include "cfe_ioctl.h"
#include "cfe_timer.h"
#include "cfe_device.h"
#include "cfe_devfuncs.h"

#include "dev_bcm4413.h"
#include "dev_bcm4413_mii.h"

#ifdef EBIDMA
	#include "bcmdma.h"
#endif

#ifdef __MIPSEB
	#define SWAP_ENDIAN
#endif

#ifdef SWAP_ENDIAN
#define SWAP16(x) ( ((x & 0xFF00) >> 8) | ((x & 0x00FF) << 8) )
#define SWAP32(x) ( (x<<24)|((x<<8)&0x00FF0000)|((x>>8)&0x0000FF00)|(x>>24) )
#endif


#define BCM4413ERROR( x, y ... ) xprintf( "BCM4413 ERROR : " x, ##y )


#ifdef DEBUG

	#define BCMDEBUG( x, y ... )            xprintf( "BCM4413 : " x, ##y )
	#define BCM4413TRACE( x, y ... )
	//#define BCM4413TRACE( x, y ... )      xprintf( "BCM4413 : " x, ##y )

	#define BCM4413INFO( x, y ... )
	//#define BCM4413INFO( x, y ... ) xprintf( "BCM4413 ERROR : " x, ##y )

#else

	#define BCM4413TRACE( x, y ... )

#endif


#define K0_TO_PHYS(p)   ((uint32_t)(p) & ((uint32_t)0x1fffffff))

#define	BCM4412_DEVICE_ID	0x4412		/* bcm44xx family pci enet */

#define PHYS_BCM44XX_BASE  0x1B400000
#define BCM44XX_BASE       (0xA0000000 | PHYS_BCM44XX_BASE)
#define BCM44XX_ENET_BASE  (BCM44XX_BASE + 0x1800)

/*
 * The number of bytes in an ethernet (MAC) address.
 */
#define	ETHER_ADDR_LEN		6

/*
 * The number of bytes in the type field.
 */
#define	ETHER_TYPE_LEN		2

/*
 * The number of bytes in the trailing CRC field.
 */
#define	ETHER_CRC_LEN		4

/*
 * The length of the combined header.
 */
#define	ETHER_HDR_LEN		(ETHER_ADDR_LEN*2+ETHER_TYPE_LEN)


/* TODO -- This value probably isn't right */
#define	ETHER_MAX_DATA		1500

/*
 * The maximum packet length.
 */
//#define	ETHER_MAX_LEN		1518
#define	ETHER_MAX_LEN		1514

/* Broadcom's rxhdr */
#define HWRXOFF       30

#define DST_BUF_SIZE        1514
#define BRCM_PACKET_SIZE    DST_BUF_SIZE + HWRXOFF + ETHER_CRC_LEN



static void bcm4413_ether_probe( cfe_driver_t * drv,    unsigned long probe_a,
                                 unsigned long probe_b, void * probe_ptr );
static int bcm4413_ether_open(cfe_devctx_t *ctx);
static int bcm4413_ether_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm4413_ether_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
static int bcm4413_ether_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm4413_ether_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm4413_ether_close(cfe_devctx_t *ctx);


const static cfe_devdisp_t bcm4413_ether_dispatch = {
	bcm4413_ether_open,
	bcm4413_ether_read,
	bcm4413_ether_inpstat,
	bcm4413_ether_write,
	bcm4413_ether_ioctl,
	bcm4413_ether_close,
	NULL,
	NULL
};

const cfe_driver_t bcm4413drv = {
	"BCM4413 Ethernet",
	"eth",
	CFE_DEV_NETWORK,
	&bcm4413_ether_dispatch,
	bcm4413_ether_probe
};


typedef struct bcm4413_softc {
	bcmenetregs_t * regs;

	unsigned char * txbuffer;
	unsigned char * rxbuffer;

	uint8_t  hwaddr[ETHER_ADDR_LEN];
} bcm4413_softc;


/*
 * Structure of a 10Mb/s Ethernet header.
 */
typedef struct ether_header {
	uint8_t	 ether_dhost[ETHER_ADDR_LEN];
	uint8_t	 ether_shost[ETHER_ADDR_LEN];
	uint16_t ether_type;
} ether_header __attribute__((packed));

/*
 * Structure of a 48-bit Ethernet address.
 */
struct	ether_addr {
	uint8_t octet[ETHER_ADDR_LEN];
} __attribute__((packed));



static int bcm7115_getmacaddr( char * macaddr )
{
	int        retval;
	uint8_t    chksum;
	uint32_t   brdtype;
	uint16_t * src;

	BCM4413TRACE( "getmacaddr entered\n" );

	/* Check checksum */
	chksum = 0;
	for( src = (uint16_t *)(0xBAFFF800); src <= (uint16_t *)(0xBAFFF88A); ) {
		chksum += ((*src) & 0xFF00) >> 8;
		chksum += (*src) & 0x00FF;
		src++;
	}

#ifdef __MIPSEB

	if( chksum != ((*src) & 0x00FF) ) {
		BCM4413ERROR( "Flash checksum failed.\n" );
		return 0;
	}

	brdtype = (((*((uint16_t *)(0xBAFFF800))) & 0x00FF) << 16)
	        |  ((*((uint16_t *)(0xBAFFF800))) & 0xFF00)
	        |  ((*((uint16_t *)(0xBAFFF802))) & 0x00FF);

#else

	if( chksum != (((*src) & 0xFF00) >> 8) ) {
		BCM4413ERROR( "Flash checksum failed.\n" );
		return 0;
	}

	brdtype = ( (*((uint16_t *)(0xBAFFF800))) << 8 )
	        | (((*((uint16_t *)(0xBAFFF802))) & 0xFF00) >> 8);

#endif

	if( brdtype == 0x00097115 ) {
		for( src = (uint16_t *)(0xBAFFF80C); src < (uint16_t *)(0xBAFFF812); ) {
			*((uint16_t *)macaddr) = (((*src) & 0xFF00) >> 8)
			                       | (((*src) & 0x00FF) << 8);
			macaddr += 2;
			src++;
		}
		retval = 1;
	} else {
		BCM4413ERROR( "Wrong board type (0x%X).\n", brdtype );
		retval = 0;
	}

	BCM4413TRACE( "getmacaddr exited\n" );

	return retval;
}


static void bcm4413_writecam(bcmenetregs_t * regs, struct ether_addr *ea,
                             unsigned int camindex);
static void bcm4413_writecam(bcmenetregs_t * regs, struct ether_addr *ea,
                             unsigned int camindex)
{
	uint32_t w;
	uint16_t *camcontrol;

	BCM4413TRACE( "bcm4413_writecam entered\n" );

#ifdef DEBUG
	if( (R_REG(NULL, &regs->camcontrol) & (CC_CB | CC_CE)) != 0 ) {
		BCM4413ERROR( "Cannot write MAC address to CAM.\n" );
	}
#endif

	w = (ea->octet[2] << 24) | (ea->octet[3] << 16) | (ea->octet[4] << 8)
	  | ea->octet[5];
	W_REG(NULL,  &regs->camdatalo, w );

	w = CD_V | (ea->octet[0] << 8) | ea->octet[1];
	W_REG(NULL,  &regs->camdatahi, w );

	w = (camindex << CC_INDEX_SHIFT) | CC_WR;

	camcontrol = (unsigned short *) (&regs->camcontrol);
	W_REG(NULL,  &camcontrol[1], (w >> 16) & 0xffff );
	W_REG(NULL,  &camcontrol[0],    w      & 0xffff );

	/* Return when done */
	while( R_REG(NULL, &regs->camcontrol) & CC_CB ) {
		cfe_usleep( 10 );
	}

	BCM4413TRACE( "bcm4413_writecam exited\n" );
}


static void bcm4413_reset( bcmenetregs_t * regs );
static void bcm4413_reset( bcmenetregs_t * regs )
{
	BCM4413TRACE( "bcm4413_reset entered\n" );

	/* Put the chip in a reset state */
	W_REG(NULL,  &regs->devcontrol, DC_RS );

	/* We have to wait 10 msec before we can reaquire. */
	/* Update : cached mode needs to wait longer.      */
	cfe_usleep( 100 );

	/* Bring it back out to a dormant state. */
	W_REG(NULL,  &regs->devcontrol, 0 );

	cfe_usleep( 100 );

	/*
	 * We want the phy registers to be accessible even when
	 * the driver is "downed" so initialize enough for this here.
	 */
	W_REG(NULL,  ((unsigned short *) (&regs->mdiocontrol)), (MC_PE | 0x1f) );

	BCM4413TRACE( "bcm4413_reset exited\n" );
}


static void bcm4413_init( bcm4413_softc * softc )
{
	bcmenetregs_t * regs;

	BCM4413TRACE( "bcm4413_init entered\n" );

	regs = softc->regs;

	mii_init( regs );

	/* enable crc32 generation */
	OR_REG(NULL,  &regs->emaccontrol, EMC_CG );

	/* enable 802.3x tx flow control (honor received PAUSE frames) */
	W_REG(NULL,  &regs->rxcontrol, ERC_FE | ERC_UF );

#ifdef PROMISC
	OR_REG(NULL,  &regs->rxcontrol, ERC_PE );
#else
	/* our local address */
	bcm4413_writecam(regs, (struct ether_addr *)(softc->hwaddr), 0 );

	/* Accept multicast frames */
	OR_REG(NULL,  &regs->rxcontrol, ERC_AM );

	/* Enable CAM */
	OR_REG(NULL,  &regs->camcontrol, CC_CE );
#endif

	/* set max frame lengths - account for possible vlan tag */
	W_REG(NULL,  &regs->rxmaxlength, (ETHER_MAX_LEN + 32) );
	W_REG(NULL,  &regs->txmaxlength, (ETHER_MAX_LEN + 32) );

	/* set tx watermark */
	W_REG(NULL,  (unsigned short *)(&regs->txwatermark), 56 );

	/* Set TX to full duplex */
	W_REG(NULL,  &regs->txcontrol, EXC_FD );

	/* turn on the emac */
	OR_REG(NULL,  &regs->enetcontrol, EC_EE );

	mii_setspeed( regs );

	/* Enable recieves in Fifo Mode */
	W_REG(NULL,  &regs->rcvcontrol, RC_FM );

#ifdef __MIPSEB
	OR_REG(NULL,  &regs->devcontrol, DC_BS );
#endif

	/* Enable transmit and recieve */
	OR_REG(NULL,  &regs->devcontrol, DC_XE | DC_RE );

	BCM4413TRACE( "bcm4413_init exited\n" );
}


#define	I_ERRORS	(I_PC | I_PD | I_DE | I_RU | I_RO | I_XU)
static void chiperrors( bcmenetregs_t * regs, bcm4413_softc * softc );
static void chiperrors( bcmenetregs_t * regs, bcm4413_softc * softc )
{
	uint32_t intstatus;

	intstatus = R_REG(NULL,  &regs->intstatus );

	BCM4413INFO( "chiperrors: intstatus 0x%x\n", intstatus );

	if (intstatus & I_PC) {
		BCM4413INFO( "\tpci descriptor error\n");
	}

	if (intstatus & I_PD) {
		BCM4413INFO( "\tpci data error\n");
	}

	if (intstatus & I_DE) {
		BCM4413INFO("\tdescriptor protocol error\n");
	}

	if (intstatus & I_RU) {
		BCM4413INFO("\treceive descriptor underflow\n");
	}

	if (intstatus & I_RO) {
		BCM4413INFO("\treceive fifo overflow\n");

		/* in pio mode, just bang the rx fifo */
		AND_REG(NULL, &regs->devcontrol, ~DC_RE);
		cfe_usleep( 10 );
		OR_REG(NULL, &regs->devcontrol, DC_RE);
		return;
	}

	if (intstatus & I_XU) {
		BCM4413INFO("\ttransmit fifo underflow\n");
	}

	/* big hammer */
	bcm4413_reset( regs );
	bcm4413_init( softc );
}


static void bcm4413_ether_probe( cfe_driver_t * drv,    unsigned long probe_a,
                                 unsigned long probe_b, void * probe_ptr )
{
	bcmenetregs_t * regs;
	bcm4413_softc * softc;
	char descr[100];

	BCM4413TRACE( "probe entered\n" );

	softc = (bcm4413_softc *) KMALLOC( sizeof(bcm4413_softc), 0 );
	if( softc == NULL ) {
		BCM4413ERROR( "BCM4413 : Failed to allocate softc memory.\n" );
		return;
	}

	memset( softc, 0, sizeof(bcm4413_softc) );

	softc->regs = (bcmenetregs_t *)BCM44XX_ENET_BASE;
	regs = softc->regs;

	bcm4413_reset( regs );

	bcm7115_getmacaddr( softc->hwaddr );

#ifdef DEBUG
	xprintf( "BCM4413 :\n" );
	xprintf( "\tvendorID = 0x%X\n", R_REG(NULL, &regs->pcicfg[0]) );
	xprintf( "\tdeviceID = 0x%X\n", R_REG(NULL, &regs->pcicfg[1]) );
	xprintf( "\tchiprev  = 0x%X\n", R_REG(NULL, &regs->devstatus) & DS_RI_MASK );
#endif

	xsprintf( descr, "%s at 0x%X", drv->drv_description, probe_a );
	cfe_attach( drv, softc, NULL, descr );

	BCM4413TRACE( "probe exited\n" );
}


static int bcm4413_ether_open(cfe_devctx_t *ctx)
{
	bcmenetregs_t * regs;
	bcm4413_softc * softc = (bcm4413_softc *) ctx->dev_softc;

	BCM4413TRACE( "open entered\n" );

	/* THESE BUFFERS MUST BE 4-BYTE ALIGNED */
	softc->txbuffer = (unsigned char *)KMALLOC( BRCM_PACKET_SIZE, 4 );
	if( softc->txbuffer == NULL ) {
		BCM4413ERROR( "BCM4413 : Failed to allocate TX buffer memory.\n" );
		return 0;
	}

	softc->rxbuffer = (unsigned char *)KMALLOC( BRCM_PACKET_SIZE, 4 );
	if( softc->rxbuffer == NULL ) {
		BCM4413ERROR( "BCM4413 : Failed to allocate RX buffer memory.\n" );
		KFREE( softc->txbuffer );
		return 0;
	}

#ifdef DEBUG
	memset( softc->txbuffer, 0, BRCM_PACKET_SIZE );
	memset( softc->rxbuffer, 0, BRCM_PACKET_SIZE );
#endif

	regs = softc->regs;

#ifdef DEBUG
	if( (R_REG(NULL, &regs->emaccontrol) & EMC_PD) != 0 ||
	    (R_REG(NULL, &regs->devcontrol)  &  DC_PD) != 0 ) {
		BCM4413ERROR( "ERROR : Internal ephy has no power or clock.\n" );
	}
#endif

	mii_init( regs );

	/* enable crc32 generation */
	OR_REG(NULL,  &regs->emaccontrol, EMC_CG );

	/* enable 802.3x tx flow control (honor received PAUSE frames) */
	W_REG(NULL,  &regs->rxcontrol, ERC_FE | ERC_UF );

#ifdef PROMISC
	OR_REG(NULL,  &regs->rxcontrol, ERC_PE );
#else

	/* our local address */
	bcm4413_writecam(regs, (struct ether_addr *)(softc->hwaddr), 0 );

	/* Accept multicast frames */
	OR_REG(NULL,  &regs->rxcontrol, ERC_AM );

	/* enable cam */
	OR_REG(NULL,  &regs->camcontrol, CC_CE );
#endif

	/* set max frame lengths - account for possible vlan tag */
	W_REG(NULL,  &regs->rxmaxlength, (ETHER_MAX_LEN + 32) );
	W_REG(NULL,  &regs->txmaxlength, (ETHER_MAX_LEN + 32) );

	/* set tx watermark */
	W_REG(NULL,  (unsigned short *)(&regs->txwatermark), 56 );

	/* Set TX to full duplex */
	W_REG(NULL,  &regs->txcontrol, EXC_FD );

	/* turn on the emac */
	OR_REG(NULL,  &regs->enetcontrol, EC_EE );

	mii_setspeed( regs );

	/* Enable recieves in Fifo Mode */
	W_REG(NULL,  &regs->rcvcontrol, RC_FM );

#ifdef __MIPSEB
	OR_REG(NULL,  &regs->devcontrol, DC_BS );
#endif

	/* Enable transmit and recieve */
	OR_REG(NULL,  &regs->devcontrol, DC_XE | DC_RE );

	BCM4413TRACE( "open exited\n" );

	return 0;
}


static int bcm4413_ether_read( cfe_devctx_t * ctx, iocb_buffer_t * buffer )
{
	int i;
	uint16_t type;
	uint16_t flags;
	uint16_t rawlen;
	uint16_t length;
	uint16_t * dst16;
	bcmenetregs_t * regs;
	bcmenetrxhdr_t * hdr;
	bcm4413_softc  * softc = (bcm4413_softc *) ctx->dev_softc;
	ether_header   * ethhdr;

	BCM4413TRACE( "read entered\n" );

#ifdef DEBUG
	/* ============================= ASSERTIONS ============================= */

	if( ctx == NULL ) {
		xprintf( "No context\n" );
		return -1;
	}

	if( buffer == NULL ) {
		xprintf( "No dst buffer\n" );
		return -1;
	}

	if( buffer->buf_length != DST_BUF_SIZE ) {
		xprintf( "dst buffer too small.\n" );
		xprintf( "actual size is %d\n", buffer->buf_length );
		return -1;
	}

	if( softc == NULL ) {
		xprintf( "softc has not been initialized.\n" );
		return -1;
	}

	/* ====================================================================== */
#endif

	regs = softc->regs;

#ifdef DEBUG
	if( !(R_REG(NULL, &regs->rcvfifocontrol) & RFC_FR) ) {
		BCM4413ERROR( "4413 NOT READY TO RECEIVE!\n" );
		return -1;
	}
#endif

	/* Clear frame ready */
	W_REG(NULL,  &regs->rcvfifocontrol, RFC_FR );

	/* Wait for the hardware to make the data available */
	while( !(R_REG(NULL, &regs->rcvfifocontrol) & RFC_DR) ) {}

	rawlen = R_REG(NULL,  &regs->rcvfifodata );

#ifdef SWAP_ENDIAN
	length = SWAP16( rawlen );
#else
	length = rawlen;
#endif

	/* TODO -- Check this test. */
	if( length < ETHER_HDR_LEN || length > (BRCM_PACKET_SIZE - HWRXOFF) ) {
		BCM4413INFO( "Extreme incoming packet size.\n" );
		BCM4413INFO( "length = %d, rawlen = %d\n", length, rawlen );

		buffer->buf_retlen = 0;

		W_REG(NULL,  &regs->rcvfifocontrol, RFC_DR );

		return 0;
	}

	/* read the rxheader from the fifo first */
	dst16 = (uint16_t *)(softc->rxbuffer);
	*dst16++ = rawlen;
	i = (RXHDR_LEN / 2) - 1;
	while (i--) {
		*dst16++ = R_REG(NULL, &regs->rcvfifodata);
	}

	/* read the frame */
	dst16 = (uint16_t *)(softc->rxbuffer + HWRXOFF);
	i = length / 2;
	while (i--) {
		*dst16++ = R_REG(NULL, &regs->rcvfifodata);
	}

	/* Read the odd byte */
	if( length & 0x1 ) {
		*dst16 = R_REG(NULL, &regs->rcvfifodata) & 0xFF;
	}

	/* Notify the hardware that we're done. */
	W_REG(NULL,  &regs->rcvfifocontrol, RFC_DR );

	hdr    = (bcmenetrxhdr_t *)(softc->rxbuffer);
	ethhdr = (struct ether_header *)(softc->rxbuffer + HWRXOFF);

#ifdef SWAP_ENDIAN
	flags = SWAP16( hdr->flags );
#else
	flags = hdr->flags;
#endif

	/* TODO -- Should we reset the chip here? */
	if( flags & (RXF_NO | RXF_RXER | RXF_CRC | RXF_OV) ) {
		BCM4413INFO( "Packet recieve error (0x%X)\n", flags );
		buffer->buf_retlen = 0;
		return -1;
	}

	/* 802.3 4.3.2: discard frames with inconsistent length field value */
#ifdef SWAP_ENDIAN
	type = SWAP32( ethhdr->ether_type );
#else
	type = ethhdr->ether_type;
#endif
	if( (type <= ETHER_MAX_DATA) && (type > (length - ETHER_HDR_LEN)) ) {
		BCM4413INFO( "Inconsistent length field value.\n" );
		buffer->buf_retlen = 0;
		return -1;
	}

	/* Strip off the CRC */
	length -= ETHER_CRC_LEN;

	/* TODO -- This check shouldn't be necessary. */
	if( length >= DST_BUF_SIZE ) {
		BCM4413INFO( "\nIncoming packet too big : length = %d\n", length );
		buffer->buf_retlen = 0;
		return -1;
	}

	memcpy( buffer->buf_ptr, softc->rxbuffer + HWRXOFF, length );

	buffer->buf_retlen = length;

	/* Check for any error conditions */
	if( R_REG(NULL, &regs->intstatus) & I_ERRORS ) {
		chiperrors( regs, softc );
	}

	BCM4413TRACE( "read exited\n" );

	return 0;
}


static int bcm4413_ether_inpstat( cfe_devctx_t * ctx, iocb_inpstat_t * inpstat )
{
	cfe_uint_t stat;
	bcm4413_softc * softc = (bcm4413_softc *) ctx->dev_softc;

	/* Check whenever a full frame is ready. */
	stat = (R_REG(NULL, &softc->regs->rcvfifocontrol) & RFC_FR) ? 1 : 0;
	inpstat->inp_status = stat;

	return 0;
}


static int bcm4413_ether_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
	uint16_t length;
	uint16_t * src16;
	bcm4413_softc * softc;
	bcmenetregs_t * regs;

	BCM4413TRACE( "write entered\n", ctx );

	softc = (bcm4413_softc *) ctx->dev_softc;
	regs  = softc->regs;

#ifdef DEBUG
	/* TODO -- Should we wait until the hardware _is_ ready? */
	if( !(R_REG(NULL, &regs->xmtfifocontrol) & XFC_FR) ) {
		BCM4413ERROR( "4413 NOT READY TO SEND!\n" );
		return -1;
	}
#endif

	/* Clear frameready */
	W_REG(NULL,  &regs->xmtfifocontrol, XFC_FR );

	/* When writing 16 bit values, both 8-bit data bytes are valid. */
	W_REG(NULL,  &regs->xmtfifocontrol, XFC_BOTH );

	length = (buffer->buf_length) & ~1;

	/* SEND IT! */
	src16 = (uint16_t *)(buffer->buf_ptr);
	length /= 2;
	while( length-- ) {
		W_REG(NULL, &regs->xmtfifodata, *src16++ );
	}

	/* Write last odd byte. */
	if( buffer->buf_length & 1 ) {
		W_REG(NULL,  &regs->xmtfifocontrol, XFC_LO );
		W_REG(NULL,  &regs->xmtfifodata, *src16 );
	}

	/* Tell the hardware we're done. */
	W_REG(NULL,  &regs->xmtfifocontrol, XFC_EF );

	/* Check for any error conditions */
	if( R_REG(NULL, &regs->intstatus) & I_ERRORS ) {
		chiperrors( regs, softc );
	}

	BCM4413TRACE( "write exited\n" );

	return 0;
}


static int bcm4413_ether_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
	int retval = 0;
	bcm4413_softc * softc = ctx->dev_softc;

	switch( (int)buffer->buf_ioctlcmd ) {
		case IOCTL_ETHER_GETHWADDR:
			BCM4413TRACE( "IOCTL_ETHER_GETHWADDR called.\n" );
			memcpy( buffer->buf_ptr, softc->hwaddr, sizeof(softc->hwaddr) );
			break;
		case IOCTL_ETHER_SETHWADDR:
			BCM4413TRACE( "IOCTL_ETHER_SETHWADDR called.\n" );
			bcm4413_writecam( softc->regs, (struct ether_addr *)buffer->buf_ptr, 0 );
			memcpy( softc->hwaddr, buffer->buf_ptr, sizeof(softc->hwaddr)  );
			break;
		default:
			xprintf( "Invalid IOCTL to bcm4413_ether_ioctl.\n" );
			retval = -1;
	}

	return retval;
}


static int bcm4413_ether_close(cfe_devctx_t *ctx)
{
	bcm4413_softc * softc = (bcm4413_softc *) ctx->dev_softc;

	bcm4413_reset( softc->regs );

	KFREE( softc->rxbuffer );
	softc->rxbuffer = NULL;

	KFREE( softc->txbuffer );
	softc->txbuffer = NULL;

	return 0;
}
