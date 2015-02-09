/*
 * Copyright (c) 2006 Cisco Systems, Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef MLX4_CMD_H
#define MLX4_CMD_H

#include <linux/dma-mapping.h>

enum {
	/* initialization and general commands */
	MLX4_CMD_SYS_EN		 = 0x1,
	MLX4_CMD_SYS_DIS	 = 0x2,
	MLX4_CMD_MAP_FA		 = 0xfff,
	MLX4_CMD_UNMAP_FA	 = 0xffe,
	MLX4_CMD_RUN_FW		 = 0xff6,
	MLX4_CMD_MOD_STAT_CFG	 = 0x34,
	MLX4_CMD_QUERY_DEV_CAP	 = 0x3,
	MLX4_CMD_QUERY_FW	 = 0x4,
	MLX4_CMD_ENABLE_LAM	 = 0xff8,
	MLX4_CMD_DISABLE_LAM	 = 0xff7,
	MLX4_CMD_QUERY_DDR	 = 0x5,
	MLX4_CMD_QUERY_ADAPTER	 = 0x6,
	MLX4_CMD_INIT_HCA	 = 0x7,
	MLX4_CMD_CLOSE_HCA	 = 0x8,
	MLX4_CMD_INIT_PORT	 = 0x9,
	MLX4_CMD_CLOSE_PORT	 = 0xa,
	MLX4_CMD_QUERY_HCA	 = 0xb,
	MLX4_CMD_QUERY_PORT	 = 0x43,
	MLX4_CMD_SENSE_PORT	 = 0x4d,
	MLX4_CMD_SET_PORT	 = 0xc,
	MLX4_CMD_ACCESS_DDR	 = 0x2e,
	MLX4_CMD_MAP_ICM	 = 0xffa,
	MLX4_CMD_UNMAP_ICM	 = 0xff9,
	MLX4_CMD_MAP_ICM_AUX	 = 0xffc,
	MLX4_CMD_UNMAP_ICM_AUX	 = 0xffb,
	MLX4_CMD_SET_ICM_SIZE	 = 0xffd,

	/* TPT commands */
	MLX4_CMD_SW2HW_MPT	 = 0xd,
	MLX4_CMD_QUERY_MPT	 = 0xe,
	MLX4_CMD_HW2SW_MPT	 = 0xf,
	MLX4_CMD_READ_MTT	 = 0x10,
	MLX4_CMD_WRITE_MTT	 = 0x11,
	MLX4_CMD_SYNC_TPT	 = 0x2f,

	/* EQ commands */
	MLX4_CMD_MAP_EQ		 = 0x12,
	MLX4_CMD_SW2HW_EQ	 = 0x13,
	MLX4_CMD_HW2SW_EQ	 = 0x14,
	MLX4_CMD_QUERY_EQ	 = 0x15,

	/* CQ commands */
	MLX4_CMD_SW2HW_CQ	 = 0x16,
	MLX4_CMD_HW2SW_CQ	 = 0x17,
	MLX4_CMD_QUERY_CQ	 = 0x18,
	MLX4_CMD_MODIFY_CQ	 = 0x2c,

	/* SRQ commands */
	MLX4_CMD_SW2HW_SRQ	 = 0x35,
	MLX4_CMD_HW2SW_SRQ	 = 0x36,
	MLX4_CMD_QUERY_SRQ	 = 0x37,
	MLX4_CMD_ARM_SRQ	 = 0x40,

	/* QP/EE commands */
	MLX4_CMD_RST2INIT_QP	 = 0x19,
	MLX4_CMD_INIT2RTR_QP	 = 0x1a,
	MLX4_CMD_RTR2RTS_QP	 = 0x1b,
	MLX4_CMD_RTS2RTS_QP	 = 0x1c,
	MLX4_CMD_SQERR2RTS_QP	 = 0x1d,
	MLX4_CMD_2ERR_QP	 = 0x1e,
	MLX4_CMD_RTS2SQD_QP	 = 0x1f,
	MLX4_CMD_SQD2SQD_QP	 = 0x38,
	MLX4_CMD_SQD2RTS_QP	 = 0x20,
	MLX4_CMD_2RST_QP	 = 0x21,
	MLX4_CMD_QUERY_QP	 = 0x22,
	MLX4_CMD_INIT2INIT_QP	 = 0x2d,
	MLX4_CMD_SUSPEND_QP	 = 0x32,
	MLX4_CMD_UNSUSPEND_QP	 = 0x33,
	/* special QP and management commands */
	MLX4_CMD_CONF_SPECIAL_QP = 0x23,
	MLX4_CMD_MAD_IFC	 = 0x24,

	/* multicast commands */
	MLX4_CMD_READ_MCG	 = 0x25,
	MLX4_CMD_WRITE_MCG	 = 0x26,
	MLX4_CMD_MGID_HASH	 = 0x27,

	/* miscellaneous commands */
	MLX4_CMD_DIAG_RPRT	 = 0x30,
	MLX4_CMD_NOP		 = 0x31,

	/* debug commands */
	MLX4_CMD_QUERY_DEBUG_MSG = 0x2a,
	MLX4_CMD_SET_DEBUG_MSG	 = 0x2b,
};

enum {
	MLX4_CMD_TIME_CLASS_A	= 10000,
	MLX4_CMD_TIME_CLASS_B	= 10000,
	MLX4_CMD_TIME_CLASS_C	= 10000,
};

enum {
	MLX4_MAILBOX_SIZE	=  4096
};

enum {
	/* set port opcode modifiers */
	MLX4_SET_PORT_GENERAL   = 0x0,
	MLX4_SET_PORT_RQP_CALC  = 0x1,
	MLX4_SET_PORT_MAC_TABLE = 0x2,
	MLX4_SET_PORT_VLAN_TABLE = 0x3,
	MLX4_SET_PORT_PRIO_MAP  = 0x4,
};

struct mlx4_dev;

struct mlx4_cmd_mailbox {
	void		       *buf;
	dma_addr_t		dma;
};

int __mlx4_cmd(struct mlx4_dev *dev, u64 in_param, u64 *out_param,
	       int out_is_imm, u32 in_modifier, u8 op_modifier,
	       u16 op, unsigned long timeout);

/* Invoke a command with no output parameter */
static inline int mlx4_cmd(struct mlx4_dev *dev, u64 in_param, u32 in_modifier,
			   u8 op_modifier, u16 op, unsigned long timeout)
{
	return __mlx4_cmd(dev, in_param, NULL, 0, in_modifier,
			  op_modifier, op, timeout);
}

/* Invoke a command with an output mailbox */
static inline int mlx4_cmd_box(struct mlx4_dev *dev, u64 in_param, u64 out_param,
			       u32 in_modifier, u8 op_modifier, u16 op,
			       unsigned long timeout)
{
	return __mlx4_cmd(dev, in_param, &out_param, 0, in_modifier,
			  op_modifier, op, timeout);
}

/*
 * Invoke a command with an immediate output parameter (and copy the
 * output into the caller's out_param pointer after the command
 * executes).
 */
static inline int mlx4_cmd_imm(struct mlx4_dev *dev, u64 in_param, u64 *out_param,
			       u32 in_modifier, u8 op_modifier, u16 op,
			       unsigned long timeout)
{
	return __mlx4_cmd(dev, in_param, out_param, 1, in_modifier,
			  op_modifier, op, timeout);
}

struct mlx4_cmd_mailbox *mlx4_alloc_cmd_mailbox(struct mlx4_dev *dev);
void mlx4_free_cmd_mailbox(struct mlx4_dev *dev, struct mlx4_cmd_mailbox *mailbox);

#endif /* MLX4_CMD_H */
