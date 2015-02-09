/*
 * Definitions for the KLSI KL5KUSB105 serial port adapter
 */

/* vendor/product pairs that are known to contain this chipset */
#define PALMCONNECT_VID		0x0830
#define PALMCONNECT_PID		0x0080

#define KLSI_VID		0x05e9
#define KLSI_KL5KUSB105D_PID	0x00c0

/* Vendor commands: */


/* port table -- the chip supports up to 4 channels */

/* baud rates */

enum {
	kl5kusb105a_sio_b115200 = 0,
	kl5kusb105a_sio_b57600  = 1,
	kl5kusb105a_sio_b38400  = 2,
	kl5kusb105a_sio_b19200  = 4,
	kl5kusb105a_sio_b14400  = 5,
	kl5kusb105a_sio_b9600   = 6,
	kl5kusb105a_sio_b4800   = 8,	/* unchecked */
	kl5kusb105a_sio_b2400   = 9,	/* unchecked */
	kl5kusb105a_sio_b1200   = 0xa,	/* unchecked */
	kl5kusb105a_sio_b600    = 0xb	/* unchecked */
};

/* data bits */
#define kl5kusb105a_dtb_7   7
#define kl5kusb105a_dtb_8   8



/* requests: */
#define KL5KUSB105A_SIO_SET_DATA  1
#define KL5KUSB105A_SIO_POLL      2
#define KL5KUSB105A_SIO_CONFIGURE      3
/* values used for request KL5KUSB105A_SIO_CONFIGURE */
#define KL5KUSB105A_SIO_CONFIGURE_READ_ON      3
#define KL5KUSB105A_SIO_CONFIGURE_READ_OFF     2

/* Interpretation of modem status lines */
#define KL5KUSB105A_DSR			((1<<4) | (1<<5))
#define KL5KUSB105A_CTS			((1<<5) | (1<<4))

#define KL5KUSB105A_WANTS_TO_SEND	0x30
