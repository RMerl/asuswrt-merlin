/*
 * Code to dump Marvell SysKonnect registers for skge and sky2 drivers.
 *
 * Copyright (C) 2004, 2006
 *  Stephen Hemminger <shemminger@osdl.org>
 */

#include <stdio.h>

#include "internal.h"

static void dump_addr(int n, const u8 *a)
{
	int i;

	printf("Addr %d            ", n);
	for (i = 0; i < 6; i++)
		printf("%02X%c", a[i], i == 5 ? '\n' : ' ');
}

static void dump_timer(const char *name, const void *p)
{
	const u8 *a = p;
	const u32 *r = p;

	printf("%s\n", name);
	printf("\tInit 0x%08X Value 0x%08X\n", r[0], r[1]);
	printf("\tTest 0x%02X       Control 0x%02X\n", a[8], a[9]);
}

static void dump_queue(const char *name, const void *a, int rx)
{
	struct desc {
		u_int32_t		ctl;
		u_int32_t		next;
		u_int32_t		data_lo;
		u_int32_t		data_hi;
		u_int32_t		status;
		u_int32_t		timestamp;
		u_int16_t		csum2;
		u_int16_t		csum1;
		u_int16_t		csum2_start;
		u_int16_t		csum1_start;
		u_int32_t		addr_lo;
		u_int32_t		addr_hi;
		u_int32_t		count_lo;
		u_int32_t		count_hi;
		u_int32_t               byte_count;
		u_int32_t               csr;
		u_int32_t               flag;
	};
	const struct desc *d = a;

	/* is reset bit set? */
	if (!(d->ctl & 2)) {
		printf("\n%s (disabled)\n", name);
		return;
	}

	printf("\n%s\n", name);
	printf("---------------\n");
	printf("Descriptor Address       0x%08X%08X\n",
	       d->addr_hi, d->addr_lo);
	printf("Address Counter          0x%08X%08X\n",
	       d->count_hi, d->count_lo);
	printf("Current Byte Counter             %d\n", d->byte_count);
	printf("BMU Control/Status               0x%08X\n", d->csr);
	printf("Flag & FIFO Address              0x%08X\n", d->flag);
	printf("\n");
	printf("Control                          0x%08X\n", d->ctl);
	printf("Next                             0x%08X\n", d->next);
	printf("Data                     0x%08X%08X\n",
	       d->data_hi, d->data_lo);
	printf("Status                           0x%08X\n", d->status);
	printf("Timestamp                        0x%08X\n", d->timestamp);
	if (rx) {
		printf("Csum1      Offset %4d Position  %d\n",
		       d->csum1, d->csum1_start);
		printf("Csum2      Offset %4d Position  %d\n",
		       d->csum2, d->csum2_start);
	} else
		printf("Csum Start 0x%04X Pos %4d Write %d\n",
		       d->csum1, d->csum2_start, d->csum1_start);

}

static void dump_ram(const char *name, const void *p)
{
	const u32 *r = p;

	if (!(r[10] & 2)) {
		printf("\n%s (disabled)\n", name);
		return;
	}

	printf("\n%s\n", name);
	printf("---------------\n");
	printf("Start Address                    0x%08X\n", r[0]);
	printf("End Address                      0x%08X\n", r[1]);
	printf("Write Pointer                    0x%08X\n", r[2]);
	printf("Read Pointer                     0x%08X\n", r[3]);

	if (*name == 'R') { /* Receive only */
		printf("Upper Threshold/Pause Packets    0x%08X\n", r[4]);
		printf("Lower Threshold/Pause Packets    0x%08X\n", r[5]);
		printf("Upper Threshold/High Priority    0x%08X\n", r[6]);
		printf("Lower Threshold/High Priority    0x%08X\n", r[7]);
	}
	printf("Packet Counter                   0x%08X\n", r[8]);
	printf("Level                            0x%08X\n", r[9]);
	printf("Control                          0x%08X\n", r[10]);
}

static void dump_fifo(const char *name, const void *p)
{
	const u32 *r = p;

	printf("\n%s\n", name);
	printf("---------------\n");
	printf("End Address                      0x%08X\n", r[0]);
  	printf("Write Pointer                    0x%08X\n", r[1]);
  	printf("Read Pointer                     0x%08X\n", r[2]);
  	printf("Packet Counter                   0x%08X\n", r[3]);
  	printf("Level                            0x%08X\n", r[4]);
  	printf("Control                          0x%08X\n", r[5]);
  	printf("Control/Test                     0x%08X\n", r[6]);
	dump_timer("LED", p + 0x20);
}

static void dump_gmac_fifo(const char *name, const void *p)
{
	const u32 *r = p;
	int i;
	static const char *regs[] = {
		"End Address",
		"Almost Full Thresh",
		"Control/Test",
		"FIFO Flush Mask",
		"FIFO Flush Threshold",
		"Truncation Threshold",
		"Upper Pause Threshold",
		"Lower Pause Threshold",
		"VLAN Tag",
		"FIFO Write Pointer",
		"FIFO Write Level",
		"FIFO Read Pointer",
		"FIFO Read Level",
	};

	printf("\n%s\n", name);
	for (i = 0; i < sizeof(regs)/sizeof(regs[0]); ++i)
		printf("%-32s 0x%08X\n", regs[i], r[i]);

}

static void dump_mac(const u8 *r)
{
	u8 id;

	printf("\nMAC Addresses\n");
	printf("---------------\n");
	dump_addr(1, r + 0x100);
	dump_addr(2, r + 0x108);
	dump_addr(3, r + 0x110);
	printf("\n");

	printf("Connector type               0x%02X (%c)\n",
	       r[0x118], (char)r[0x118]);
	printf("PMD type                     0x%02X (%c)\n",
	       r[0x119], (char)r[0x119]);
	printf("PHY type                     0x%02X\n", r[0x11d]);

	id = r[0x11b];
	printf("Chip Id                      0x%02X ", id);

	switch (id) {
	case 0x0a:	printf("Genesis");	break;
	case 0xb0:	printf("Yukon");	break;
	case 0xb1:	printf("Yukon-Lite");	break;
	case 0xb2:	printf("Yukon-LP");	break;
	case 0xb3:	printf("Yukon-2 XL");	break;
	case 0xb5:	printf("Yukon Extreme"); break;
	case 0xb4:	printf("Yukon-2 EC Ultra");	break;
	case 0xb6:	printf("Yukon-2 EC");	break;
 	case 0xb7:	printf("Yukon-2 FE");	break;
	case 0xb8:	printf("Yukon-2 FE Plus"); break;
	case 0xb9:	printf("Yukon Supreme"); break;
	case 0xba:	printf("Yukon Ultra 2"); break;
	case 0xbc:	printf("Yukon Optima"); break;
	default:	printf("(Unknown)");	break;
	}

	printf(" (rev %d)\n", (r[0x11a] & 0xf0) >> 4);

	printf("Ram Buffer                   0x%02X\n", r[0x11c]);

}

static void dump_gma(const char *name, const u8 *r)
{
	int i;

	printf("%12s address: ", name);
	for (i = 0; i < 3; i++) {
		u16 a = *(u16 *)(r + i * 4);
		printf(" %02X %02X", a & 0xff, (a >> 8) & 0xff);
	}
	printf("\n");
}

static void dump_gmac(const char *name, const u8 *data)
{
	printf("\n%s\n", name);

	printf("Status                       0x%04X\n", *(u16 *) data);
	printf("Control                      0x%04X\n", *(u16 *) (data + 4));
	printf("Transmit                     0x%04X\n", *(u16 *) (data + 8));
	printf("Receive                      0x%04X\n", *(u16 *) (data + 0xc));
	printf("Transmit flow control        0x%04X\n", *(u16 *) (data + 0x10));
	printf("Transmit parameter           0x%04X\n", *(u16 *) (data + 0x14));
	printf("Serial mode                  0x%04X\n", *(u16 *) (data + 0x18));

	dump_gma("Source", data + 0x1c);
	dump_gma("Physical", data + 0x28);
}

static void dump_pci(const u8 *cfg)
{
	int i;

	printf("\nPCI config\n----------\n");
	for(i = 0; i < 0x80; i++) {
		if (!(i & 15))
			printf("%02x:", i);
		printf(" %02x", cfg[i]);
		if ((i & 15) == 15)
			putchar('\n');
	}
	putchar('\n');
}

static void dump_control(u8 *r)
{
	printf("Control Registers\n");
	printf("-----------------\n");

	printf("Register Access Port             0x%02X\n", *r);
	printf("LED Control/Status               0x%08X\n", *(u32 *) (r + 4));

	printf("Interrupt Source                 0x%08X\n", *(u32 *) (r + 8));
	printf("Interrupt Mask                   0x%08X\n", *(u32 *) (r + 0xc));
	printf("Interrupt Hardware Error Source  0x%08X\n", *(u32 *) (r + 0x10));
	printf("Interrupt Hardware Error Mask    0x%08X\n", *(u32 *) (r + 0x14));
	printf("Interrupt Control                0x%08X\n", *(u32 *) (r + 0x2c));
	printf("Interrupt Moderation Mask        0x%08X\n", *(u32 *) (r + 0x14c));
	printf("Hardware Moderation Mask         0x%08X\n", *(u32 *) (r + 0x150));
	dump_timer("Moderation Timer", r + 0x140);

	printf("General Purpose  I/O             0x%08X\n", *(u32 *) (r + 0x15c));
}

int skge_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	const u32 *r = (const u32 *) regs->data;
	int dual = !(regs->data[0x11a] & 1);

	dump_pci(regs->data + 0x380);

	dump_control(regs->data);

	printf("\nBus Management Unit\n");
	printf("-------------------\n");
	printf("CSR Receive Queue 1              0x%08X\n", r[24]);
	printf("CSR Sync Queue 1                 0x%08X\n", r[26]);
	printf("CSR Async Queue 1                0x%08X\n", r[27]);
	if (dual) {
		printf("CSR Receive Queue 2              0x%08X\n", r[25]);
		printf("CSR Async Queue 2                0x%08X\n", r[29]);
		printf("CSR Sync Queue 2                 0x%08X\n", r[28]);
	}

	dump_mac(regs->data);
	dump_gmac("GMAC 1", regs->data + 0x2800);

	dump_timer("Timer", regs->data + 0x130);
	dump_timer("Blink Source", regs->data +0x170);

	dump_queue("Receive Queue 1", regs->data +0x400, 1);
	dump_queue("Sync Transmit Queue 1", regs->data +0x600, 0);
	dump_queue("Async Transmit Queue 1", regs->data +0x680, 0);

	dump_ram("Receive RAMbuffer 1", regs->data+0x800);
	dump_ram("Sync Transmit RAMbuffer 1", regs->data+0xa00);
	dump_ram("Async Transmit RAMbuffer 1", regs->data+0xa80);

	dump_fifo("Receive MAC FIFO 1", regs->data+0xc00);
	dump_fifo("Transmit MAC FIFO 1", regs->data+0xd00);
	if (dual) {
		dump_gmac("GMAC 1", regs->data + 0x2800);

		dump_queue("Receive Queue 2", regs->data +0x480, 1);
		dump_queue("Async Transmit Queue 2", regs->data +0x780, 0);
		dump_queue("Sync Transmit Queue 2", regs->data +0x700, 0);

		dump_ram("Receive RAMbuffer 2", regs->data+0x880);
		dump_ram("Sync Transmit RAMbuffer 2", regs->data+0xb00);
		dump_ram("Async Transmit RAMbuffer 21", regs->data+0xb80);

		dump_fifo("Receive MAC FIFO 2", regs->data+0xc80);
		dump_fifo("Transmit MAC FIFO 2", regs->data+0xd80);
	}

	dump_timer("Descriptor Poll", regs->data+0xe00);
	return 0;

}

static void dump_queue2(const char *name, void *a, int rx)
{
	struct sky2_queue {
		u16	buf_control;
		u16	byte_count;
		u32	rss;
		u32	addr_lo, addr_hi;
		u32	status;
		u32	timestamp;
		u16	csum1, csum2;
		u16	csum1_start, csum2_start;
		u16	length;
		u16	vlan;
		u16	rsvd1;
		u16	done;
		u32	req_lo, req_hi;
		u16	rsvd2;
		u16	req_count;
		u32	csr;
	} *d = a;

	printf("\n%s\n", name);
	printf("---------------\n");

	printf("Buffer control                   0x%04X\n", d->buf_control);

	printf("Byte Counter                     %d\n", d->byte_count);
	printf("Descriptor Address               0x%08X%08X\n",
	       d->addr_hi, d->addr_lo);
	printf("Status                           0x%08X\n", d->status);
	printf("Timestamp                        0x%08X\n", d->timestamp);
	printf("BMU Control/Status               0x%08X\n", d->csr);
	printf("Done                             0x%04X\n", d->done);
	printf("Request                          0x%08X%08X\n",
	       d->req_hi, d->req_lo);
	if (rx) {
		printf("Csum1      Offset %4d Position  %d\n",
		       d->csum1, d->csum1_start);
		printf("Csum2      Offset %4d Position  %d\n",
		       d->csum2, d->csum2_start);
	} else
		printf("Csum Start 0x%04X Pos %4d Write %d\n",
		       d->csum1, d->csum2_start, d->csum1_start);
}

static void dump_prefetch(const char *name, const void *r)
{
	const u32 *reg = r;

	printf("\n%s Prefetch\n", name);
	printf("Control               0x%08X\n", reg[0]);
	printf("Last Index            %u\n", reg[1]);
	printf("Start Address         0x%08x%08x\n", reg[3], reg[2]);
	if (*name == 'S') { /* Status unit */
		printf("TX1 report            %u\n", reg[4]);
		printf("TX2 report            %u\n", reg[5]);
		printf("TX threshold          %u\n", reg[6]);
		printf("Put Index             %u\n", reg[7]);
	} else {
		printf("Get Index             %u\n", reg[4]);
		printf("Put Index             %u\n", reg[5]);
	}
}

int sky2_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	const u16 *r16 = (const u16 *) regs->data;
	const u32 *r32 = (const u32 *) regs->data;
	int dual;

	dump_pci(regs->data + 0x1c00);

	dump_control(regs->data);

	printf("\nBus Management Unit\n");
	printf("-------------------\n");
	printf("CSR Receive Queue 1              0x%08X\n", r32[24]);
	printf("CSR Sync Queue 1                 0x%08X\n", r32[26]);
	printf("CSR Async Queue 1                0x%08X\n", r32[27]);

	dual = (regs->data[0x11e] & 2) != 0;
	if (dual) {
		printf("CSR Receive Queue 2              0x%08X\n", r32[25]);
		printf("CSR Async Queue 2                0x%08X\n", r32[29]);
		printf("CSR Sync Queue 2                 0x%08X\n", r32[28]);
	}

	dump_mac(regs->data);

	dump_prefetch("Status", regs->data + 0xe80);
	dump_prefetch("Receive 1", regs->data + 0x450);
	dump_prefetch("Transmit 1", regs->data + 0x450 + 0x280);

	if (dual) {
		dump_prefetch("Receive 2", regs->data + 0x450 + 0x80);
		dump_prefetch("Transmit 2", regs->data + 0x450 + 0x380);
	}

	printf("\nStatus FIFO\n");
  	printf("\tWrite Pointer            0x%02X\n", regs->data[0xea0]);
  	printf("\tRead Pointer             0x%02X\n", regs->data[0xea4]);
  	printf("\tLevel                    0x%02X\n", regs->data[0xea8]);
  	printf("\tWatermark                0x%02X\n", regs->data[0xeac]);
  	printf("\tISR Watermark            0x%02X\n", regs->data[0xead]);

	dump_timer("Status level", regs->data + 0xeb0);
	dump_timer("TX status", regs->data + 0xec0);
	dump_timer("ISR", regs->data + 0xed0);

	printf("\nGMAC control             0x%04X\n", r32[0xf00 >> 2]);
	printf("GPHY control             0x%04X\n", r32[0xf04 >> 2]);
	printf("LINK control             0x%02hX\n", r16[0xf10 >> 1]);

	dump_gmac("GMAC 1", regs->data + 0x2800);
	dump_gmac_fifo("Rx GMAC 1", regs->data + 0xc40);
	dump_gmac_fifo("Tx GMAC 1", regs->data + 0xd40);

	dump_queue2("Receive Queue 1", regs->data +0x400, 1);
	dump_queue("Sync Transmit Queue 1", regs->data +0x600, 0);
	dump_queue2("Async Transmit Queue 1", regs->data +0x680, 0);

	dump_ram("Receive RAMbuffer 1", regs->data+0x800);
	dump_ram("Sync Transmit RAMbuffer 1", regs->data+0xa00);
	dump_ram("Async Transmit RAMbuffer 1", regs->data+0xa80);

	if (dual) {
		dump_ram("Receive RAMbuffer 2", regs->data+0x880);
		dump_ram("Sync Transmit RAMbuffer 2", regs->data+0xb00);
		dump_ram("Async Transmit RAMbuffer 21", regs->data+0xb80);
		dump_gmac("GMAC 2", regs->data + 0x3800);
		dump_gmac_fifo("Rx GMAC 2", regs->data + 0xc40 + 128);
		dump_gmac_fifo("Tx GMAC 2", regs->data + 0xd40 + 128);
	}

	return 0;
}
