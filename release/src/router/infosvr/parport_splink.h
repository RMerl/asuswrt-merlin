#include <asm/io.h>

//JY#define PARPORT_IRQ	IRQ_GPIO19
#define PARPORT_IRQ	0x02
//#define PARPORT_GPIO	GPIO_GPIO19
#define PARPORT_GPIO	GPIO_GPIO2

/* --- register definitions ------------------------------- */
//#define CONTROL(p)  ((p)->base    + (0x2<<1))
//#define STATUS(p)   ((p)->base    + (0x1<<1))
//#define DATA(p)     ((p)->base    + 0x0)
#define CONTROL(p)  ((p)->base    + (0x2))//JY
#define STATUS(p)   ((p)->base    + (0x1))//JY
#define DATA(p)     ((p)->base    + 0x0)//JY

#define MAX_CLASS_NAME	16	
#define MAX_MFR		16
#define MAX_MODEL	32
#define MAX_DESCRIPT	64

struct parport_splink_device_info {
	const char class_name[MAX_CLASS_NAME];
        const char mfr[MAX_MFR];
        const char model[MAX_MODEL];
        const char description[MAX_DESCRIPT];
#ifdef PRNSTATUS//JY20021224
	const char status[4];
#endif
};


struct parport_splink_private {
	/* Contents of CTR. */
	unsigned char ctr;

	/* Bitmask of writable CTR bits. */
	unsigned char ctr_writable;

	/* Whether or not there's an ECR. */
	int ecr;

	/* Number of PWords that FIFO will hold. */
	int fifo_depth;

	/* Number of bytes per portword. */
	int pword;

	/* Not used yet. */
	int readIntrThreshold;
	int writeIntrThreshold;

	/* buffer suitable for DMA, if DMA enabled */
	char *dma_buf;
	dma_addr_t dma_handle;
	struct pci_dev *dev;
};

extern __inline__ void parport_splink_write_data(struct parport *p, unsigned char d)
{
	outb(d, DATA(p));
}

extern __inline__ unsigned char parport_splink_read_data(struct parport *p)
{
	return inb (DATA (p));
}

#define dump_parport_state(args...)

/* __parport_splink_frob_control differs from parport_splink_frob_control in that
 * it doesn't do any extra masking. */
static __inline__ unsigned char __parport_splink_frob_control (struct parport *p,
							   unsigned char mask,
							   unsigned char val)
{
	struct parport_splink_private *priv = p->physport->private_data;
	unsigned char ctr = priv->ctr;

	ctr = (ctr & ~mask) ^ val;
	ctr &= priv->ctr_writable; /* only write writable bits. */
	outb (ctr, CONTROL (p));
	priv->ctr = ctr;	/* Update soft copy */
	return ctr;
}

extern __inline__ void parport_splink_data_reverse (struct parport *p)
{
	__parport_splink_frob_control (p, 0x20, 0x20);
}

extern __inline__ void parport_splink_data_forward (struct parport *p)
{
	__parport_splink_frob_control (p, 0x20, 0x00);
}

extern __inline__ void parport_splink_write_control (struct parport *p,
						 unsigned char d)
{
	const unsigned char wm = (PARPORT_CONTROL_STROBE |
				  PARPORT_CONTROL_AUTOFD |
				  PARPORT_CONTROL_INIT |
				  PARPORT_CONTROL_SELECT);
// Neo
printk("write_control: parport_splink.h\n");

	/* Take this out when drivers have adapted to newer interface. */
	if (d & 0x20) {
		printk (KERN_DEBUG "%s (%s): use data_reverse for this!\n",
			p->name, p->cad->name);
		parport_splink_data_reverse (p);
	}

	__parport_splink_frob_control (p, wm, d & wm);
}

extern __inline__ unsigned char parport_splink_read_control(struct parport *p)
{
	const unsigned char rm = (PARPORT_CONTROL_STROBE |
				  PARPORT_CONTROL_AUTOFD |
				  PARPORT_CONTROL_INIT |
				  PARPORT_CONTROL_SELECT);
	const struct parport_splink_private *priv = p->physport->private_data;
	return priv->ctr & rm; /* Use soft copy */
}

extern __inline__ unsigned char parport_splink_frob_control (struct parport *p,
							 unsigned char mask,
							 unsigned char val)
{
	const unsigned char wm = (PARPORT_CONTROL_STROBE |
				  PARPORT_CONTROL_AUTOFD |
				  PARPORT_CONTROL_INIT |
				  PARPORT_CONTROL_SELECT);

	/* Take this out when drivers have adapted to newer interface. */
	if (mask & 0x20) {
		printk (KERN_DEBUG "%s (%s): use data_%s for this!\n",
			p->name, p->cad->name,
			(val & 0x20) ? "reverse" : "forward");
		if (val & 0x20)
			parport_splink_data_reverse (p);
		else
			parport_splink_data_forward (p);
	}

	/* Restrict mask and val to control lines. */
	mask &= wm;
	val &= wm;

	return __parport_splink_frob_control (p, mask, val);
}

extern __inline__ unsigned char parport_splink_read_status(struct parport *p)
{
	return inb(STATUS(p));
}


extern void parport_splink_release_resources(struct parport *p);
extern int parport_splink_claim_resources(struct parport *p);

/* PCMCIA code will want to get us to look at a port.  Provide a mechanism. */
extern struct parport *parport_splink_probe_port (unsigned int base);
extern void parport_splink_unregister_port (struct parport *p);

