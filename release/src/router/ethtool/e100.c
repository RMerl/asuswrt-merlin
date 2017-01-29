/* Copyright (c) 2002 Intel Corporation */
#include <stdio.h>
#include "internal.h"

#define D102_REV_ID		12

#define MDI_MDIX_CONFIG_IS_OK	0x0010
#define MDI_MDIX_STATUS		0x0020

#define SOFT_INT            	0x0200		/* Generate a S/W interrupt */

/* Interrupt masks */
#define ALL_INT_MASK        	0x0100		/* Mask interrupts */
#define FCP_INT_MASK        	0x0400		/* Flow Control Pause */
#define ER_INT_MASK         	0x0800		/* Early Receive */
#define RNR_INT_MASK        	0x1000		/* RU Not Ready */
#define CNA_INT_MASK        	0x2000		/* CU Not Active */
#define FR_INT_MASK         	0x4000		/* Frame Received */
#define CX_INT_MASK         	0x8000		/* CU eXecution w/ I-bit done */

/* Interrupts pending */
#define FCP_INT_PENDING         0x0100		/* Flow Control Pause */
#define ER_INT_PENDING          0x0200		/* Early Receive */
#define SWI_INT_PENDING         0x0400		/* S/W generated interrupt */
#define MDI_INT_PENDING         0x0800		/* MDI read or write done */
#define RNR_INT_PENDING         0x1000		/* RU Became Not Ready */
#define CNA_INT_PENDING         0x2000		/* CU Became Inactive (IDLE) */
#define FR_INT_PENDING          0x4000		/* RU Received A Frame */
#define CX_INT_PENDING          0x8000		/* CU Completed Action Cmd */

/* Status */
#define CU_STATUS		0x00C0
#define RU_STATUS		0x003C

/* Commands */
#define CU_CMD			0x00F0
#define RU_CMD			0x0007

int
e100_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	u32 *regs_buff = (u32 *)regs->data;
	u8 version = (u8)(regs->version >> 24);
	u8 rev_id = (u8)(regs->version);
	u8 regs_len = regs->len / sizeof(u32);
	u32 reg;
	u16 scb_status, scb_cmd;

	if (version != 1)
		return -1;

	reg = regs_buff[0];
	scb_status = reg & 0x0000ffff;
	scb_cmd = reg >> 16;
	fprintf(stdout,
		"SCB Status Word (Lower Word)             0x%04X\n",
		scb_status);

	switch ((scb_status & RU_STATUS) >> 2) {
	case 0:
		fprintf(stdout,
		"      RU Status:               Idle\n");
		break;
	case 1:
		fprintf(stdout,
		"      RU Status:               Suspended\n");
		break;
	case 2:
		fprintf(stdout,
		"      RU Status:               No Resources\n");
		break;
	case 4:
		fprintf(stdout,
		"      RU Status:               Ready\n");
		break;
	case 9:
		fprintf(stdout,
		"      RU Status:               Suspended with no more RBDs\n");
		break;
	case 10:
		fprintf(stdout,
		"      RU Status:               No Resources due to no more RBDs\n");
		break;
	case 12:
		fprintf(stdout,
		"      RU Status:               Ready with no RBDs present\n");
		break;
	default:
		fprintf(stdout,
		"      RU Status:               Unknown State\n");
		break;
	}

	switch ((scb_status & CU_STATUS) >> 6) {
	case 0:
		fprintf(stdout,
		"      CU Status:               Idle\n");
		break;
	case 1:
		fprintf(stdout,
		"      CU Status:               Suspended\n");
		break;
        case 2:
                fprintf(stdout,
                "      CU Status:              Active\n");
                break;
	default:
		fprintf(stdout,
		"      CU Status:               Unknown State\n");
		break;
	}

	fprintf(stdout,
	        "      ---- Interrupts Pending ----\n"
		"      Flow Control Pause:                %s\n"
		"      Early Receive:                     %s\n"
		"      Software Generated Interrupt:      %s\n"
		"      MDI Done:                          %s\n"
		"      RU Not In Ready State:             %s\n"
		"      CU Not in Active State:            %s\n"
		"      RU Received Frame:                 %s\n"
		"      CU Completed Command:              %s\n",
		scb_status & FCP_INT_PENDING     ? "yes"     : "no",
		scb_status & ER_INT_PENDING      ? "yes"     : "no",
		scb_status & SWI_INT_PENDING     ? "yes"     : "no",
		scb_status & MDI_INT_PENDING     ? "yes"     : "no",
		scb_status & RNR_INT_PENDING     ? "yes"     : "no",
		scb_status & CNA_INT_PENDING     ? "yes"     : "no",
		scb_status & FR_INT_PENDING      ? "yes"     : "no",
		scb_status & CX_INT_PENDING      ? "yes"     : "no");

	fprintf(stdout,
		"SCB Command Word (Upper Word)            0x%04X\n",
		scb_cmd);

	switch (scb_cmd & RU_CMD) {
	case 0:
		fprintf(stdout,
		"      RU Command:              No Command\n");
		break;
	case 1:
		fprintf(stdout,
		"      RU Command:              RU Start\n");
		break;
	case 2:
		fprintf(stdout,
		"      RU Command:              RU Resume\n");
		break;
	case 4:
		fprintf(stdout,
		"      RU Command:              RU Abort\n");
		break;
	case 6:
		fprintf(stdout,
		"      RU Command:              Load RU Base\n");
		break;
	default:
		fprintf(stdout,
		"      RU Command:              Unknown\n");
		break;
	}

	switch ((scb_cmd & CU_CMD) >> 4) {
	case 0:
		fprintf(stdout,
		"      CU Command:              No Command\n");
		break;
	case 1:
		fprintf(stdout,
		"      CU Command:              CU Start\n");
		break;
	case 2:
		fprintf(stdout,
		"      CU Command:              CU Resume\n");
		break;
	case 4:
		fprintf(stdout,
		"      CU Command:              Load Dump Counters Address\n");
		break;
	case 5:
		fprintf(stdout,
		"      CU Command:              Dump Counters\n");
		break;
	case 6:
		fprintf(stdout,
		"      CU Command:              Load CU Base\n");
		break;
	case 7:
		fprintf(stdout,
		"      CU Command:              Dump & Reset Counters\n");
		break;
	default:
		fprintf(stdout,
		"      CU Command:              Unknown\n");
		break;
	}

	fprintf(stdout,
		"      Software Generated Interrupt:      %s\n",
		scb_cmd & SOFT_INT     ? "yes"     : "no");

	fprintf(stdout,
	        "      ---- Interrupts Masked ----\n"
		"      ALL Interrupts:                    %s\n"
		"      Flow Control Pause:                %s\n"
		"      Early Receive:                     %s\n"
		"      RU Not In Ready State:             %s\n"
		"      CU Not in Active State:            %s\n"
		"      RU Received Frame:                 %s\n"
		"      CU Completed Command:              %s\n",
		scb_cmd & ALL_INT_MASK     ? "yes"     : "no",
		scb_cmd & FCP_INT_MASK     ? "yes"     : "no",
		scb_cmd & ER_INT_MASK      ? "yes"     : "no",
		scb_cmd & RNR_INT_MASK     ? "yes"     : "no",
		scb_cmd & CNA_INT_MASK     ? "yes"     : "no",
		scb_cmd & FR_INT_MASK      ? "yes"     : "no",
		scb_cmd & CX_INT_MASK      ? "yes"     : "no");

	if(regs_len > 1) {
		fprintf(stdout, "MDI/MDI-X Status:                        ");
		if(rev_id < D102_REV_ID)
			fprintf(stdout, "MDI\n");
		else {
			u16 ctrl_reg = regs_buff[1];

			if(ctrl_reg & MDI_MDIX_CONFIG_IS_OK) {
				if(ctrl_reg & MDI_MDIX_STATUS)
					fprintf(stdout, "MDI-X\n");
				else
					fprintf(stdout, "MDI\n");
			} else
				fprintf(stdout, "Unknown\n");
		}
	}

	return 0;
}

