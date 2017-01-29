#include <stdio.h>
#include <string.h>
#include "internal.h"

int et131x_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	u8 version = (u8)(regs->version >> 24);
	u32 *reg = (u32 *)regs->data;

	if (version != 1)
		return -1;

	fprintf(stdout, "PHY Registers\n");
	fprintf(stdout, "0x0, Basic Control Reg          = 0x%04X\n", *reg++);
	fprintf(stdout, "0x1, Basic Status Reg           = 0x%04X\n", *reg++);
	fprintf(stdout, "0x2, PHY identifier 1           = 0x%04X\n", *reg++);
	fprintf(stdout, "0x3, PHY identifier 2           = 0x%04X\n", *reg++);
	fprintf(stdout, "0x4, Auto Neg Advertisement     = 0x%04X\n", *reg++);
	fprintf(stdout, "0x5, Auto Neg L Partner Ability = 0x%04X\n", *reg++);
	fprintf(stdout, "0x6, Auto Neg Expansion         = 0x%04X\n", *reg++);
	fprintf(stdout, "0x7, Reserved                   = 0x%04X\n", *reg++);
	fprintf(stdout, "0x8, Reserved                   = 0x%04X\n", *reg++);
	fprintf(stdout, "0x9, 1000T Control              = 0x%04X\n", *reg++);
	fprintf(stdout, "0xA, 1000T Status               = 0x%04X\n", *reg++);
	fprintf(stdout, "0xB, Reserved                   = 0x%04X\n", *reg++);
	fprintf(stdout, "0xC, Reserved                   = 0x%04X\n", *reg++);
	fprintf(stdout, "0xD, MMD Access Control         = 0x%04X\n", *reg++);
	fprintf(stdout, "0xE, MMD access Data            = 0x%04X\n", *reg++);
	fprintf(stdout, "0xF, Extended Status            = 0x%04X\n", *reg++);
	fprintf(stdout, "0x10, Phy Index                 = 0x%04X\n", *reg++);
	fprintf(stdout, "0x11, Phy Data                  = 0x%04X\n", *reg++);
	fprintf(stdout, "0x12, MPhy Control              = 0x%04X\n", *reg++);
	fprintf(stdout, "0x13, Phy Loopback Control1     = 0x%04X\n", *reg++);
	fprintf(stdout, "0x14, Phy Loopback Control2     = 0x%04X\n", *reg++);
	fprintf(stdout, "0x15, Register Management       = 0x%04X\n", *reg++);
	fprintf(stdout, "0x16, Phy Config                = 0x%04X\n", *reg++);
	fprintf(stdout, "0x17, Phy Phy Control           = 0x%04X\n", *reg++);
	fprintf(stdout, "0x18, Phy Interrupt Mask        = 0x%04X\n", *reg++);
	fprintf(stdout, "0x19, Phy Interrupt Status      = 0x%04X\n", *reg++);
	fprintf(stdout, "0x1A, Phy Phy Status            = 0x%04X\n", *reg++);
	fprintf(stdout, "0x1B, Phy LED1                  = 0x%04X\n", *reg++);
	fprintf(stdout, "0x1C, Phy LED2                  = 0x%04X\n", *reg++);
	fprintf(stdout, "\n");

	fprintf(stdout, "JAGCore Global Registers\n");
	fprintf(stdout, "0x0, TXQ Start Address          = 0x%04X\n", *reg++);
	fprintf(stdout, "0x1, TXQ End Address            = 0x%04X\n", *reg++);
	fprintf(stdout, "0x2, RXQ Start Address          = 0x%04X\n", *reg++);
	fprintf(stdout, "0x3, RXQ End Address            = 0x%04X\n", *reg++);
	fprintf(stdout, "0x4, Power Management Status    = 0x%04X\n", *reg++);
	fprintf(stdout, "0x5, Interrupt Status           = 0x%04X\n", *reg++);
	fprintf(stdout, "0x6, Interrupt Mask             = 0x%04X\n", *reg++);
	fprintf(stdout, "0x7, Int Alias Clear Mask       = 0x%04X\n", *reg++);
	fprintf(stdout, "0x8, Int Status Alias           = 0x%04X\n", *reg++);
	fprintf(stdout, "0x9, Software Reset             = 0x%04X\n", *reg++);
	fprintf(stdout, "0xA, SLV Timer                  = 0x%04X\n", *reg++);
	fprintf(stdout, "0xB, MSI Config                 = 0x%04X\n", *reg++);
	fprintf(stdout, "0xC, Loopback                   = 0x%04X\n", *reg++);
	fprintf(stdout, "0xD, Watchdog Timer             = 0x%04X\n", *reg++);
	fprintf(stdout, "\n");

	fprintf(stdout, "TXDMA Registers\n");
	fprintf(stdout, "0x0, Control Status             = 0x%04X\n", *reg++);
	fprintf(stdout, "0x1, Packet Ring Base Addr (Hi) = 0x%04X\n", *reg++);
	fprintf(stdout, "0x2, Packet Ring Base Addr (Lo) = 0x%04X\n", *reg++);
	fprintf(stdout, "0x3, Packet Ring Num Descrs     = 0x%04X\n", *reg++);
	fprintf(stdout, "0x4, TX Queue Write Address     = 0x%04X\n", *reg++);
	fprintf(stdout, "0x5, TX Queue Write Address Ext = 0x%04X\n", *reg++);
	fprintf(stdout, "0x6, TX Queue Read Address      = 0x%04X\n", *reg++);
	fprintf(stdout, "0x7, Status Writeback Addr (Hi) = 0x%04X\n", *reg++);
	fprintf(stdout, "0x8, Status Writeback Addr (Lo) = 0x%04X\n", *reg++);
	fprintf(stdout, "0x9, Service Request            = 0x%04X\n", *reg++);
	fprintf(stdout, "0xA, Service Complete           = 0x%04X\n", *reg++);
	fprintf(stdout, "0xB, Cache Read Index           = 0x%04X\n", *reg++);
	fprintf(stdout, "0xC, Cache Write Index          = 0x%04X\n", *reg++);
	fprintf(stdout, "0xD, TXDMA Error                = 0x%04X\n", *reg++);
	fprintf(stdout, "0xE, Descriptor Abort Count     = 0x%04X\n", *reg++);
	fprintf(stdout, "0xF, Payload Abort Count        = 0x%04X\n", *reg++);
	fprintf(stdout, "0x10, Writeback Abort Count     = 0x%04X\n", *reg++);
	fprintf(stdout, "0x11, Descriptor Timeout Count  = 0x%04X\n", *reg++);
	fprintf(stdout, "0x12, Payload Timeout Count     = 0x%04X\n", *reg++);
	fprintf(stdout, "0x13, Writeback Timeout Count   = 0x%04X\n", *reg++);
	fprintf(stdout, "0x14, Descriptor Error Count    = 0x%04X\n", *reg++);
	fprintf(stdout, "0x15, Payload Error Count       = 0x%04X\n", *reg++);
	fprintf(stdout, "0x16, Writeback Error Count     = 0x%04X\n", *reg++);
	fprintf(stdout, "0x17, Dropped TLP Count         = 0x%04X\n", *reg++);
	fprintf(stdout, "0x18, New service Complete      = 0x%04X\n", *reg++);
	fprintf(stdout, "0x1A, Ethernet Packet Count     = 0x%04X\n", *reg++);
	fprintf(stdout, "\n");

	fprintf(stdout, "RXDMA Registers\n");
	fprintf(stdout, "0x0, Control Status             = 0x%04X\n", *reg++);
	fprintf(stdout, "0x1, Writeback Addr (Hi)        = 0x%04X\n", *reg++);
	fprintf(stdout, "0x2, Writeback Addr (Lo)        = 0x%04X\n", *reg++);
	fprintf(stdout, "0x3, Num Packets Done           = 0x%04X\n", *reg++);
	fprintf(stdout, "0x4, Max Packet Time            = 0x%04X\n", *reg++);
	fprintf(stdout, "0x5, RX Queue Read Addr         = 0x%04X\n", *reg++);
	fprintf(stdout, "0x6, RX Queue Read Address Ext  = 0x%04X\n", *reg++);
	fprintf(stdout, "0x7, RX Queue Write Addr        = 0x%04X\n", *reg++);
	fprintf(stdout, "0x8, Packet Ring Base Addr (Hi) = 0x%04X\n", *reg++);
	fprintf(stdout, "0x9, Packet Ring Base Addr (Lo) = 0x%04X\n", *reg++);
	fprintf(stdout, "0xA, Packet Ring Num Descrs     = 0x%04X\n", *reg++);
	fprintf(stdout, "0xE, Packet Ring Avail Offset   = 0x%04X\n", *reg++);
	fprintf(stdout, "0xF, Packet Ring Full Offset    = 0x%04X\n", *reg++);
	fprintf(stdout, "0x10, Packet Ring Access Index  = 0x%04X\n", *reg++);
	fprintf(stdout, "0x11, Packet Ring Min Descrip   = 0x%04X\n", *reg++);
	fprintf(stdout, "0x12, FBR0 Address (Lo)         = 0x%04X\n", *reg++);
	fprintf(stdout, "0x13, FBR0 Address (Hi)         = 0x%04X\n", *reg++);
	fprintf(stdout, "0x14, FBR0 Num Descriptors      = 0x%04X\n", *reg++);
	fprintf(stdout, "0x15, FBR0 Available Offset     = 0x%04X\n", *reg++);
	fprintf(stdout, "0x16, FBR0 Full Offset          = 0x%04X\n", *reg++);
	fprintf(stdout, "0x17, FBR0 Read Index           = 0x%04X\n", *reg++);
	fprintf(stdout, "0x18, FBR0 Minimum Descriptors  = 0x%04X\n", *reg++);
	fprintf(stdout, "0x19, FBR1 Address (Lo)         = 0x%04X\n", *reg++);
	fprintf(stdout, "0x1A, FBR1 Address (Hi)         = 0x%04X\n", *reg++);
	fprintf(stdout, "0x1B, FBR1 Num Descriptors      = 0x%04X\n", *reg++);
	fprintf(stdout, "0x1C, FBR1 Available Offset     = 0x%04X\n", *reg++);
	fprintf(stdout, "0x1D, FBR1 Full Offset          = 0x%04X\n", *reg++);
	fprintf(stdout, "0x1E, FBR1 Read Index           = 0x%04X\n", *reg++);
	fprintf(stdout, "0x1F, FBR1 Minimum Descriptors  = 0x%04X\n", *reg++);
	fprintf(stdout, "\n");
	return 0;
}
