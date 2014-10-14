#ifndef IRQ_HANDLED
typedef void irqreturn_t;
#define IRQ_NONE
#define IRQ_HANDLED
#endif

#ifndef MODULE_LICENSE
#define MODULE_LICENSE(x)
#endif

#ifndef __iomem
#define __iomem
#endif

#ifndef __attribute_used__
#define __attribute_used__	__devinitdata
#endif

#ifndef __user
#define __user
#endif

#ifndef SERVICE_ACTION_IN
#define SERVICE_ACTION_IN	0x9e
#endif
#ifndef READ_16
#define READ_16			0x88
#endif
#ifndef WRITE_16
#define WRITE_16		0x8a
#endif
