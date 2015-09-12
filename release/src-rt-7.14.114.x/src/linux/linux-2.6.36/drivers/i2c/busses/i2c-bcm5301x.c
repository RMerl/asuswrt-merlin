/*
 * I2C master mode driver for 5301x chip.
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id:  $
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

#include <osl.h>
#include <siutils.h>
#include <hndsoc.h>
#include <chipcommonb.h>

/* Configuration */
#define BCM5301X_CFG_DRV_NAME					"i2c-bcm5301x"
#define BCM5301X_CFG_BB_DRV_NAME				"i2c-bcm5301x-bb"
#define BCM5301X_CFG_DBG					0
#define BCM5301X_CFG_INFO					0
#define BCM5301X_CFG_ERR					1
#define BCM5301X_CFG_SMBUS_RETRY_CNT				4
#define BCM5301X_CFG_SMBUS_IDLE_TIME				100
#define BCM5301X_CFG_TIMEOUT_MS(len)				(200 + (len))
#define BCM5301X_CFG_TIMEOUT(len)				msecs_to_jiffies(BCM5301X_CFG_TIMEOUT_MS(len))
#define BCM5301X_CFG_MIN_RETRY_CNT				3
#define BCM5301X_CFG_MIN_RETRY_MS				20
#define BCM5301X_CFG_RX_THRESH					(BCM5301X_SMBUS_FIFO_DEPTH / 2)
/***************************************************/

/*
 * Debug
 *
 * Please be careful with printing debug messages over serial line.
 * Printing messages in ISR cost me several hours of debugging.
 * Print is finished quickly, but later I2C controller ISR postponed for ~20 milliseconds because of UART activities.
 * In couple milliseconds I2C RX FIFO became full and triggered RX FIFO full interrupt - which is normal.
 * But UART acitivities hold off I2C ISR for much longer. When finally I2C ISR begin execution it found that
 * RX FIFO is actually empty (seems somehow self-resetted) and I2C device is not acting sane anymore.
 * Theory that relatively long time keeping device paused in the middle of transaction make device
 * fail and send some signals and reaction to them was I2C controller cleared RX FIFO.
 */
#define BCM5301X_2STR_1(x)					BCM5301X_2STR_2(x)
#define BCM5301X_2STR_2(x)					#x
#define BCM5301X_LINE						BCM5301X_2STR_1(__LINE__)
#define BCM5301X_MSG(level, fmt, args...)			printk(level __FILE__ " : " BCM5301X_LINE " : " fmt "\n", ## args)
#if BCM5301X_CFG_DBG
# define BCM5301X_MSG_DBG(...)					BCM5301X_MSG(KERN_DEBUG, __VA_ARGS__)
#else
# define BCM5301X_MSG_DBG(...)
#endif
#if BCM5301X_CFG_INFO
# define BCM5301X_MSG_INFO(...)					BCM5301X_MSG(KERN_INFO, __VA_ARGS__)
#else
# define BCM5301X_MSG_INFO(...)
#endif
#if BCM5301X_CFG_ERR
# define BCM5301X_MSG_ERR(...)					BCM5301X_MSG(KERN_ERR, __VA_ARGS__)
#else
# define BCM5301X_MSG_ERR(...)
#endif
#define BCM5301X_MSG_MAX_LEN					16
/***************************************************/

/* Hardware */

#define BCM5301X_BIT(shift)					(1 << (shift))
#define BCM5301X_MASK_S(val, mask, shift)			(((val) & (mask)) << (shift))
#define BCM5301X_MASK_G(val, mask, shift)			(((val) >> (shift)) & (mask))

#define BCM5301X_SMBUS_CFG_RESET_B				BCM5301X_BIT(31)
#define BCM5301X_SMBUS_CFG_SMB_EN_B				BCM5301X_BIT(30)
#define BCM5301X_SMBUS_CFG_BITBANG_EN_B				BCM5301X_BIT(29)
#define BCM5301X_SMBUS_CFG_EN_NIC_SMB_ADDR_0_B			BCM5301X_BIT(28)
#define BCM5301X_SMBUS_CFG_PROMISCOUS_MODE_B			BCM5301X_BIT(27)
#define BCM5301X_SMBUS_CFG_TIMESTAMP_CNT_EN_B			BCM5301X_BIT(26)
#define BCM5301X_SMBUS_CFG_MASTER_RETRY_CNT_S(v)		BCM5301X_MASK_S(v, 0xF, 16)

#define BCM5301X_SMBUS_TIMING_CFG_MODE_400_B			BCM5301X_BIT(31)
#define BCM5301X_SMBUS_TIMING_CFG_RANDOM_SLAVE_STRETCH_S(v)	BCM5301X_MASK_S(v, 0x7F, 24)
#define BCM5301X_SMBUS_TIMING_CFG_PERIODIC_SLAVE_STRETCH_S(v)	BCM5301X_MASK_S(v, 0xFF, 16)
#define BCM5301X_SMBUS_TIMING_CFG_SMBUS_IDLE_TIME_S(v)		BCM5301X_MASK_S(v, 0xFF, 8)

#define BCM5301X_SMBUS_MASTER_FIFO_CTRL_RX_FIFO_FLUSH_B		BCM5301X_BIT(31)
#define BCM5301X_SMBUS_MASTER_FIFO_CTRL_TX_FIFO_FLUSH_B		BCM5301X_BIT(30)
#define BCM5301X_SMBUS_MASTER_FIFO_CTRL_RX_PKT_CNT_G(v)		BCM5301X_MASK_G(v, 0x7F, 16)
#define BCM5301X_SMBUS_MASTER_FIFO_CTRL_RX_FIFO_THRESH_S(v)	BCM5301X_MASK_S(v, 0x3F, 8)

#define BCM5301X_SMBUS_BB_CLK_IN				BCM5301X_BIT(31)
#define BCM5301X_SMBUS_BB_CLK_OUT_EN				BCM5301X_BIT(30)
#define BCM5301X_SMBUS_BB_DAT_IN				BCM5301X_BIT(29)
#define BCM5301X_SMBUS_BB_DAT_OUT_EN				BCM5301X_BIT(28)

#define BCM5301X_SMBUS_MASTER_CMD_START_BUSY_COMM_B		BCM5301X_BIT(31)
#define BCM5301X_SMBUS_MASTER_CMD_ABORT_B			BCM5301X_BIT(30)
#define BCM5301X_SMBUS_MASTER_CMD_STATUS_G(v)			BCM5301X_MASK_G(v, 0x7, 25)
#define BCM5301X_SMBUS_MASTER_CMD_SMBUS_PROTOCOL_S(v)		BCM5301X_MASK_S(v, 0xF, 9)
#define BCM5301X_SMBUS_MASTER_CMD_PEC_B				BCM5301X_BIT(8)
#define BCM5301X_SMBUS_MASTER_CMD_RD_BYTE_CNT_S(v)		BCM5301X_MASK_S(v, 0xFF, 0)
#define BCM5301X_SMBUS_MASTER_CMD_RD_BYTE_CNT_G(v)		BCM5301X_MASK_G(v, 0xFF, 0)

#define BCM5301X_SMBUS_MASTER_CMD_STATUS_SUCCESS		0x0
#define BCM5301X_SMBUS_MASTER_CMD_STATUS_LOST_ARB		0x1
#define BCM5301X_SMBUS_MASTER_CMD_STATUS_OFFLINE		0x2
#define BCM5301X_SMBUS_MASTER_CMD_STATUS_NACK			0x3
#define BCM5301X_SMBUS_MASTER_CMD_STATUS_DEV_TIMEOUT		0x4
#define BCM5301X_SMBUS_MASTER_CMD_STATUS_TX_UNDERRUN_TIMEOUT	0x5
#define BCM5301X_SMBUS_MASTER_CMD_STATUS_RX_FULL_TIMEOUT	0x6

#define BCM5301X_SMBUS_MASTER_CMD_PROTOCOL_QUICK_CMD_B		BCM5301X_SMBUS_MASTER_CMD_SMBUS_PROTOCOL_S(0x0)
#define BCM5301X_SMBUS_MASTER_CMD_PROTOCOL_SEND_BYTE_B		BCM5301X_SMBUS_MASTER_CMD_SMBUS_PROTOCOL_S(0x1)
#define BCM5301X_SMBUS_MASTER_CMD_PROTOCOL_RECEIVE_BYTE_B	BCM5301X_SMBUS_MASTER_CMD_SMBUS_PROTOCOL_S(0x2)
#define BCM5301X_SMBUS_MASTER_CMD_PROTOCOL_WRITE_BYTE_B		BCM5301X_SMBUS_MASTER_CMD_SMBUS_PROTOCOL_S(0x3)
#define BCM5301X_SMBUS_MASTER_CMD_PROTOCOL_READ_BYTE_B		BCM5301X_SMBUS_MASTER_CMD_SMBUS_PROTOCOL_S(0x4)
#define BCM5301X_SMBUS_MASTER_CMD_PROTOCOL_WRITE_WORD_B		BCM5301X_SMBUS_MASTER_CMD_SMBUS_PROTOCOL_S(0x5)
#define BCM5301X_SMBUS_MASTER_CMD_PROTOCOL_READ_WORD_B		BCM5301X_SMBUS_MASTER_CMD_SMBUS_PROTOCOL_S(0x6)
#define BCM5301X_SMBUS_MASTER_CMD_PROTOCOL_BLOCK_WRITE_B	BCM5301X_SMBUS_MASTER_CMD_SMBUS_PROTOCOL_S(0x7)
#define BCM5301X_SMBUS_MASTER_CMD_PROTOCOL_BLOCK_READ_B		BCM5301X_SMBUS_MASTER_CMD_SMBUS_PROTOCOL_S(0x8)
#define BCM5301X_SMBUS_MASTER_CMD_PROTOCOL_PROC_CALL_B		BCM5301X_SMBUS_MASTER_CMD_SMBUS_PROTOCOL_S(0x9)
#define BCM5301X_SMBUS_MASTER_CMD_PROTOCOL_BLOCK_PROC_CALL_B	BCM5301X_SMBUS_MASTER_CMD_SMBUS_PROTOCOL_S(0xA)
#define BCM5301X_SMBUS_MASTER_CMD_PROTOCOL_HOST_NOTIFY_B	BCM5301X_SMBUS_MASTER_CMD_SMBUS_PROTOCOL_S(0xB)

#define BCM5301X_SMBUS_EVENT_ENABLE_MASTER_RX_FIFO_FULL_B	BCM5301X_BIT(31)
#define BCM5301X_SMBUS_EVENT_ENABLE_MASTER_RX_THRESH_HIT_B	BCM5301X_BIT(30)
#define BCM5301X_SMBUS_EVENT_ENABLE_MASTER_RX_EVENT_B		BCM5301X_BIT(29)
#define BCM5301X_SMBUS_EVENT_ENABLE_MASTER_START_BUSY_B		BCM5301X_BIT(28)
#define BCM5301X_SMBUS_EVENT_ENABLE_MASTER_TX_UNDERRUN_B	BCM5301X_BIT(27)
#define BCM5301X_SMBUS_EVENT_ENABLE_SLAVE_RX_FIFO_FULL_B	BCM5301X_BIT(26)
#define BCM5301X_SMBUS_EVENT_ENABLE_SLAVE_RX_THRESH_HIT_B	BCM5301X_BIT(25)
#define BCM5301X_SMBUS_EVENT_ENABLE_SLAVE_RX_EVENT_B		BCM5301X_BIT(24)
#define BCM5301X_SMBUS_EVENT_ENABLE_SLAVE_START_BUSY_B		BCM5301X_BIT(23)
#define BCM5301X_SMBUS_EVENT_ENABLE_SLAVE_TX_UNDERRUN_B		BCM5301X_BIT(22)
#define BCM5301X_SMBUS_EVENT_ENABLE_SLAVE_RD_EVENT_B		BCM5301X_BIT(21)

#define BCM5301X_SMBUS_EVENT_STATUS_MASTER_RX_FIFO_FULL_B	BCM5301X_BIT(31)
#define BCM5301X_SMBUS_EVENT_STATUS_MASTER_RX_THRESH_HIT_B	BCM5301X_BIT(30)
#define BCM5301X_SMBUS_EVENT_STATUS_MASTER_RX_EVENT_B		BCM5301X_BIT(29)
#define BCM5301X_SMBUS_EVENT_STATUS_MASTER_START_BUSY_B		BCM5301X_BIT(28)
#define BCM5301X_SMBUS_EVENT_STATUS_MASTER_TX_UNDERRUN_B	BCM5301X_BIT(27)
#define BCM5301X_SMBUS_EVENT_STATUS_SLAVE_RX_FIFO_FULL_B	BCM5301X_BIT(26)
#define BCM5301X_SMBUS_EVENT_STATUS_SLAVE_RX_THRESH_HIT_B	BCM5301X_BIT(25)
#define BCM5301X_SMBUS_EVENT_STATUS_SLAVE_RX_EVENT_B		BCM5301X_BIT(24)
#define BCM5301X_SMBUS_EVENT_STATUS_SLAVE_START_BUSY_B		BCM5301X_BIT(23)
#define BCM5301X_SMBUS_EVENT_STATUS_SLAVE_TX_UNDERRUN_B		BCM5301X_BIT(22)
#define BCM5301X_SMBUS_EVENT_STATUS_SLAVE_RD_EVENT_B		BCM5301X_BIT(21)

#define BCM5301X_SMBUS_MASTER_DATA_WRITE_STATUS_B		BCM5301X_BIT(31)
#define BCM5301X_SMBUS_MASTER_DATA_WRITE_DATA_S(v)		BCM5301X_MASK_S(v, 0xFF, 0)

#define BCM5301X_SMBUS_MASTER_DATA_READ_STATUS_G(v)		BCM5301X_MASK_G(v, 0x3, 30)
#define BCM5301X_SMBUS_MASTER_DATA_READ_PEC_ERR_B		BCM5301X_BIT(29)
#define BCM5301X_SMBUS_MASTER_DATA_READ_DATA_G(v)		BCM5301X_MASK_G(v, 0xFF, 0)

#define BCM5301X_SMBUS_MASTER_DATA_READ_STATUS_EMPTY		0x0
#define BCM5301X_SMBUS_MASTER_DATA_READ_STATUS_START		0x1
#define BCM5301X_SMBUS_MASTER_DATA_READ_STATUS_MIDDLE		0x2
#define BCM5301X_SMBUS_MASTER_DATA_READ_STATUS_END		0x3

#define BCM5301X_SMBUS_FIFO_DEPTH				64

#define BCM5301X_SMBUS_RX_MAX_CNT				BCM5301X_SMBUS_MASTER_CMD_RD_BYTE_CNT_G(~((uint32)0))

#define BCM5301X_SMBUS_TX_EVENT_MASK				BCM5301X_SMBUS_EVENT_ENABLE_MASTER_TX_UNDERRUN_B
#define BCM5301X_SMBUS_RX_EVENT_MASK \
	(BCM5301X_SMBUS_EVENT_ENABLE_MASTER_RX_FIFO_FULL_B | \
	BCM5301X_SMBUS_EVENT_ENABLE_MASTER_RX_THRESH_HIT_B | \
	BCM5301X_SMBUS_EVENT_ENABLE_MASTER_RX_EVENT_B)

#define BCM5301X_SMBUS_TX_STATUS_MASK				BCM5301X_SMBUS_TX_EVENT_MASK
#define BCM5301X_SMBUS_RX_STATUS_MASK				BCM5301X_SMBUS_RX_EVENT_MASK

/***************************************************/

struct bcm5301x_i2c_data
{
	struct i2c_adapter adap;
	int irq;
	chipcommonbregs_t *ccb;
	bool fast;
	uint16 consumed;
	struct i2c_msg *pmsg;
	struct completion done;
	struct i2c_adapter bb_adap;
	struct i2c_algo_bit_data bb_data;
};

static void bcm5301x_bb_setsda(void *data, int state)
{
	struct bcm5301x_i2c_data *pdata = data;
	chipcommonbregs_t *ccb = pdata->ccb;
	uint32 bb_reg = R_REG(SI_OSH, &ccb->smbus_bit_bang_control);

	if (state) {
		bb_reg |= BCM5301X_SMBUS_BB_DAT_OUT_EN;
	} else {
		bb_reg &= ~BCM5301X_SMBUS_BB_DAT_OUT_EN;
	}

	W_REG(SI_OSH, &ccb->smbus_bit_bang_control, bb_reg);
}

static void bcm5301x_bb_setscl(void *data, int state)
{
	struct bcm5301x_i2c_data *pdata = data;
	chipcommonbregs_t *ccb = pdata->ccb;
	uint32 bb_reg = R_REG(SI_OSH, &ccb->smbus_bit_bang_control);

	if (state) {
		bb_reg |= BCM5301X_SMBUS_BB_CLK_OUT_EN;
	} else {
		bb_reg &= ~BCM5301X_SMBUS_BB_CLK_OUT_EN;
	}

	W_REG(SI_OSH, &ccb->smbus_bit_bang_control, bb_reg);
}

static int bcm5301x_bb_getsda(void *data)
{
	struct bcm5301x_i2c_data *pdata = data;
	chipcommonbregs_t *ccb = pdata->ccb;

	return (R_REG(SI_OSH, &ccb->smbus_bit_bang_control) & BCM5301X_SMBUS_BB_DAT_IN) ? 1 : 0;
}

static int bcm5301x_bb_getscl(void *data)
{
	struct bcm5301x_i2c_data *pdata = data;
	chipcommonbregs_t *ccb = pdata->ccb;

	return (R_REG(SI_OSH, &ccb->smbus_bit_bang_control) & BCM5301X_SMBUS_BB_CLK_IN) ? 1 : 0;
}

static int bcm5301x_bb_pre_xfer(struct i2c_adapter *adap)
{
	struct i2c_algo_bit_data *bit_data = adap->algo_data;
	struct bcm5301x_i2c_data *pdata = bit_data->data;
	chipcommonbregs_t *ccb = pdata->ccb;

	W_REG(SI_OSH, &ccb->smbus_config,
		R_REG(SI_OSH, &ccb->smbus_config) | BCM5301X_SMBUS_CFG_BITBANG_EN_B);

	return 0;
}

static void bcm5301x_bb_post_xfer(struct i2c_adapter *adap)
{
	struct i2c_algo_bit_data *bit_data = adap->algo_data;
	struct bcm5301x_i2c_data *pdata = bit_data->data;
	chipcommonbregs_t *ccb = pdata->ccb;

	W_REG(SI_OSH, &ccb->smbus_config,
		R_REG(SI_OSH, &ccb->smbus_config) & ~BCM5301X_SMBUS_CFG_BITBANG_EN_B);
}

static int __devinit bcm5301x_bb_init(struct platform_device *pdev, struct bcm5301x_i2c_data *pdata)
{
	struct i2c_algo_bit_data *bb_data = &pdata->bb_data;
	struct i2c_adapter *bb_adap = &pdata->bb_adap;

	bb_data->setsda = bcm5301x_bb_setsda;
	bb_data->setscl = bcm5301x_bb_setscl;
	bb_data->getsda = bcm5301x_bb_getsda;
	bb_data->getscl = bcm5301x_bb_getscl;
	bb_data->pre_xfer = bcm5301x_bb_pre_xfer;
	bb_data->post_xfer = bcm5301x_bb_post_xfer;
	bb_data->udelay = 5; /* 100 KHz; borrowed from i2c-gpio.c */
	bb_data->timeout = BCM5301X_CFG_TIMEOUT(0);
	bb_data->data = pdata;

	bb_adap->owner = THIS_MODULE;
	snprintf(bb_adap->name, sizeof(bb_adap->name), BCM5301X_CFG_BB_DRV_NAME);
	bb_adap->algo_data = bb_data;
	bb_adap->dev.parent = &pdev->dev;
	bb_adap->nr = pdev->id + 1;

	return i2c_bit_add_numbered_bus(bb_adap);
}

static bool bcm5301x_i2c_read(struct i2c_msg *pmsg)
{
	return ((pmsg->flags & I2C_M_RD) != 0);
}

const char* bcm5301x_i2c_dirstr(struct i2c_msg *pmsg)
{
	return bcm5301x_i2c_read(pmsg) ? "read" : "write";
}

static void bcm5301x_dump_before(struct i2c_msg *pmsg)
{
#if BCM5301X_CFG_DBG
	int i;

	BCM5301X_MSG_DBG("+++ xfer : %s addr 0x%x flags 0x%x len %u",
		bcm5301x_i2c_dirstr(pmsg),
		pmsg->addr,
		pmsg->flags,
		pmsg->len);

	if (!bcm5301x_i2c_read(pmsg)) {
		for (i = 0; i < min(pmsg->len, BCM5301X_MSG_MAX_LEN); ++i) {
			BCM5301X_MSG_DBG("\t%d : 0x%x", i, pmsg->buf[i]);
		}
	}
#endif /* BCM5301X_CFG_DBG */
}

static void bcm5301x_dump_after(struct i2c_msg *pmsg, int rc)
{
#if BCM5301X_CFG_DBG
	bool failure = (rc < 0);
	int i;

	if (!failure && bcm5301x_i2c_read(pmsg)) {
		for (i = 0; i < min(pmsg->len, BCM5301X_MSG_MAX_LEN); ++i) {
			BCM5301X_MSG_DBG("\t%d : 0x%x", i, pmsg->buf[i]);
		}
	}

	BCM5301X_MSG_DBG("--- xfer : %s addr 0x%x flags 0x%x len %u rc %d %s",
		bcm5301x_i2c_dirstr(pmsg),
		pmsg->addr,
		pmsg->flags,
		pmsg->len,
		rc,
		failure ? "FAILURE" : "SUCCESS");
#endif /* BCM5301X_CFG_DBG */
}

static int bcm5301x_check_before(struct bcm5301x_i2c_data *pdata, struct i2c_msg *pmsg)
{
	chipcommonbregs_t *ccb = pdata->ccb;
	uint32 cmd_reg = R_REG(SI_OSH, &ccb->smbus_master_command);

	if (cmd_reg & BCM5301X_SMBUS_MASTER_CMD_START_BUSY_COMM_B) {
		BCM5301X_MSG_DBG("not ready reg 0x%x", cmd_reg);
		return -EBUSY;
	}

	return 0;
}

static int bcm5301x_check_after(struct bcm5301x_i2c_data *pdata, struct i2c_msg *pmsg)
{
	chipcommonbregs_t *ccb = pdata->ccb;
	uint32 cmd_reg = R_REG(SI_OSH, &ccb->smbus_master_command);
	uint32 cmd_status = BCM5301X_SMBUS_MASTER_CMD_STATUS_G(cmd_reg);

	if (cmd_reg & BCM5301X_SMBUS_MASTER_CMD_START_BUSY_COMM_B) {
		BCM5301X_MSG_DBG("not ready reg 0x%x", cmd_reg);
		return -EBUSY;
	} else if (cmd_status == BCM5301X_SMBUS_MASTER_CMD_STATUS_OFFLINE) {
		BCM5301X_MSG_DBG("dev offline");
		return -EAGAIN;
	} else if (cmd_status != BCM5301X_SMBUS_MASTER_CMD_STATUS_SUCCESS) {
		BCM5301X_MSG_DBG("failed status 0x%x", cmd_status);
		return -EIO;
	} else if (pdata->consumed != pmsg->len) {
		BCM5301X_MSG_DBG("failed len %d of %d", pdata->consumed, pmsg->len);
		return -EIO;
	}

	return 0;
}

static void bcm5301x_reset(struct bcm5301x_i2c_data *pdata)
{
	chipcommonbregs_t *ccb = pdata->ccb;

	W_REG(SI_OSH, &ccb->smbus_config, BCM5301X_SMBUS_CFG_RESET_B);
	udelay(10);
}

static void bcm5301x_reset_and_en(struct bcm5301x_i2c_data *pdata)
{
	chipcommonbregs_t *ccb = pdata->ccb;

	bcm5301x_reset(pdata);

	W_REG(SI_OSH, &ccb->smbus_master_fifo_control,
		BCM5301X_SMBUS_MASTER_FIFO_CTRL_RX_FIFO_THRESH_S(BCM5301X_CFG_RX_THRESH));
	W_REG(SI_OSH, &ccb->smbus_timing_config,
		(pdata->fast ? BCM5301X_SMBUS_TIMING_CFG_MODE_400_B : 0) |
		BCM5301X_SMBUS_TIMING_CFG_SMBUS_IDLE_TIME_S(BCM5301X_CFG_SMBUS_IDLE_TIME));
	W_REG(SI_OSH, &ccb->smbus_config,
		BCM5301X_SMBUS_CFG_SMB_EN_B |
		BCM5301X_SMBUS_CFG_MASTER_RETRY_CNT_S(BCM5301X_CFG_SMBUS_RETRY_CNT));

	BCM5301X_MSG_INFO("config 0x%x timing 0x%x",
		R_REG(SI_OSH, &ccb->smbus_config),
		R_REG(SI_OSH, &ccb->smbus_timing_config));
}

static void bcm5301x_xfer_trigger(struct bcm5301x_i2c_data *pdata, uint32 irq, uint32 cmd)
{
	chipcommonbregs_t *ccb = pdata->ccb;

	irq |= BCM5301X_SMBUS_EVENT_ENABLE_MASTER_START_BUSY_B;
	cmd |= BCM5301X_SMBUS_MASTER_CMD_START_BUSY_COMM_B;
	BCM5301X_MSG_DBG("trigger cmd 0x%x irq 0x%x js %lu", cmd, irq, jiffies);

	/* Cleanup, for safety */
	while (try_wait_for_completion(&pdata->done)) {
		BCM5301X_MSG_ERR("spurious irq");
	}

	/* Enable interrupt */
	W_REG(SI_OSH, &ccb->smbus_event_enable, irq);

	/* Start transaction */
	W_REG(SI_OSH, &ccb->smbus_master_command, cmd);
}

static void bcm5301x_tx_data(struct bcm5301x_i2c_data *pdata, uint16 max_write_len)
{
	chipcommonbregs_t *ccb = pdata->ccb;
	struct i2c_msg *pmsg = pdata->pmsg;
	uint16 max_offset = pdata->consumed + max_write_len;
	uint16 last_offset = min(pmsg->len, max_offset);

	while (pdata->consumed < last_offset) {
		uint32 data = BCM5301X_SMBUS_MASTER_DATA_WRITE_DATA_S(pmsg->buf[pdata->consumed++]);

		if (pdata->consumed == pmsg->len) {
			/* indicate this is last byte */
			data |= BCM5301X_SMBUS_MASTER_DATA_WRITE_STATUS_B;
		}

		W_REG(SI_OSH, &ccb->smbus_master_data_write, data);
	}
}

static void bcm5301x_rx_data(struct bcm5301x_i2c_data *pdata)
{
	chipcommonbregs_t *ccb = pdata->ccb;
	struct i2c_msg *pmsg = pdata->pmsg;

	while (pdata->consumed < pmsg->len) {
		uint32 reg = R_REG(SI_OSH, &ccb->smbus_master_data_read);
		uint32 status = BCM5301X_SMBUS_MASTER_DATA_READ_STATUS_G(reg);

		if (status == BCM5301X_SMBUS_MASTER_DATA_READ_STATUS_EMPTY) {
			break;
		}

		pmsg->buf[pdata->consumed++] = (uint8)BCM5301X_SMBUS_MASTER_DATA_READ_DATA_G(reg);
	}
}

static void bcm5301x_tx_start(struct bcm5301x_i2c_data *pdata, struct i2c_msg *pmsg)
{
	chipcommonbregs_t *ccb = pdata->ccb;

	/* Write 7-bit address and keep LSB zero for write operations */
	W_REG(SI_OSH, &ccb->smbus_master_data_write,
		BCM5301X_SMBUS_MASTER_DATA_WRITE_DATA_S(pmsg->addr << 1));

	/* Write data to FIFO */
	bcm5301x_tx_data(pdata, BCM5301X_SMBUS_FIFO_DEPTH - 1 /* addr */);

	/* Trigger transaction */
	bcm5301x_xfer_trigger(pdata,
		BCM5301X_SMBUS_TX_EVENT_MASK,
		BCM5301X_SMBUS_MASTER_CMD_PROTOCOL_BLOCK_WRITE_B);
}

static void bcm5301x_rx_start(struct bcm5301x_i2c_data *pdata, struct i2c_msg *pmsg)
{
	chipcommonbregs_t *ccb = pdata->ccb;

	/* Write 7-bit address and set LSB for read operations */
	W_REG(SI_OSH, &ccb->smbus_master_data_write,
		BCM5301X_SMBUS_MASTER_DATA_WRITE_DATA_S((pmsg->addr << 1) | 0x1));

	/* Trigger transaction */
	bcm5301x_xfer_trigger(pdata,
		BCM5301X_SMBUS_RX_EVENT_MASK,
		BCM5301X_SMBUS_MASTER_CMD_PROTOCOL_BLOCK_READ_B |
			BCM5301X_SMBUS_MASTER_CMD_RD_BYTE_CNT_S(pmsg->len));
}

static void bcm5301x_xfer_cleanup(struct bcm5301x_i2c_data *pdata)
{
	chipcommonbregs_t *ccb = pdata->ccb;

	/* Disable and ack interrupts */
	W_REG(SI_OSH, &ccb->smbus_event_enable, 0);
	W_REG(SI_OSH, &ccb->smbus_event_status, R_REG(SI_OSH, &ccb->smbus_event_status));

	/* Flush FIFOs */
	W_REG(SI_OSH, &ccb->smbus_master_fifo_control,
		R_REG(SI_OSH, &ccb->smbus_master_fifo_control) |
		BCM5301X_SMBUS_MASTER_FIFO_CTRL_RX_FIFO_FLUSH_B |
		BCM5301X_SMBUS_MASTER_FIFO_CTRL_TX_FIFO_FLUSH_B);
}

static int bcm5301x_xfer_msg(struct bcm5301x_i2c_data *pdata, struct i2c_msg *pmsg)
{
	int rc = bcm5301x_check_before(pdata, pmsg);
	if (rc) {
		return rc;
	}

	pdata->consumed = 0;
	pdata->pmsg = pmsg;

	if (bcm5301x_i2c_read(pmsg)) {
		bcm5301x_rx_start(pdata, pmsg);
	} else {
		bcm5301x_tx_start(pdata, pmsg);
	}

	if (!wait_for_completion_timeout(&pdata->done, BCM5301X_CFG_TIMEOUT(pmsg->len))) {
		BCM5301X_MSG_DBG("waiting timed out js %lu status 0x%x",
			jiffies, R_REG(SI_OSH, &pdata->ccb->smbus_event_status));
		rc = -ETIMEDOUT;
	} else {
		rc = bcm5301x_check_after(pdata, pmsg);
	}

	return rc;
}

static int bcm5301x_xfer_hw(struct bcm5301x_i2c_data *pdata, struct i2c_msg *pmsg)
{
	unsigned long ts = jiffies + max(1UL, msecs_to_jiffies(BCM5301X_CFG_MIN_RETRY_MS));
	int i = 0;
	int rc;

	bcm5301x_dump_before(pmsg);

	while (1) {
		rc = bcm5301x_xfer_msg(pdata, pmsg);
		if (!rc) {
			bcm5301x_xfer_cleanup(pdata);
			break;
		} else if (rc != -EAGAIN) {
			break;
		} else if ((++i >= BCM5301X_CFG_MIN_RETRY_CNT) && (time_after(jiffies, ts))) {
			BCM5301X_MSG_DBG("%s give up on #%d retry", bcm5301x_i2c_dirstr(pmsg), i);
			rc = -EIO;
			break;
		} else {
			BCM5301X_MSG_DBG("%s retry #%d", bcm5301x_i2c_dirstr(pmsg), i);
			bcm5301x_xfer_cleanup(pdata);
			yield();
		}
	}

	bcm5301x_dump_after(pmsg, rc);

	if (rc) {
		bcm5301x_reset_and_en(pdata);
	} else {
		rc = 1;
	}

	return rc;
}

static bool bcm5301x_xfer_hw_supp(struct i2c_msg *pmsg_arr, int num)
{
	struct i2c_msg *pmsg = &pmsg_arr[0];

	if (num > 1) {
		/* Support only single message as can't make no STOP between them */
		return false;
	}

	if ((pmsg->flags & ~I2C_M_RD) != 0) {
		/* Support only simple read and write */
		return false;
	}

	if ((pmsg->flags & I2C_M_RD) && (pmsg->len > BCM5301X_SMBUS_RX_MAX_CNT)) {
		/* Read size is limited */
		return false;
	}

	return true;
}

static int bcm5301x_xfer_bb(struct i2c_adapter *adap, struct i2c_msg *pmsg_arr, int num)
{
	int rc;
	int i;

	for (i = 0; i < num; ++i) {
		bcm5301x_dump_before(&pmsg_arr[i]);
	}

	rc = adap->algo->master_xfer(adap, pmsg_arr, num);

	for (i = 0; i < num; ++i) {
		bcm5301x_dump_after(&pmsg_arr[i], rc);
	}

	return rc;
}

static int bcm5301x_xfer(struct i2c_adapter *adap, struct i2c_msg *pmsg_arr, int num)
{
	struct bcm5301x_i2c_data *pdata = adap->algo_data;
	struct i2c_msg *pmsg = &pmsg_arr[0];
	int rc = -ENOSYS;

	if (num < 1) {
		return -EINVAL;
	}

	i2c_lock_adapter(&pdata->bb_adap);

	if (bcm5301x_xfer_hw_supp(pmsg_arr, num)) {
		rc = bcm5301x_xfer_hw(pdata, pmsg);
	}

	if (rc < 0) {
		rc = bcm5301x_xfer_bb(&pdata->bb_adap, pmsg_arr, num);
	}

	i2c_unlock_adapter(&pdata->bb_adap);

	return rc;
}

static u32 bcm5301x_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static irqreturn_t bcm5301x_i2c_irq(int irqno, void *dev_id)
{
	struct bcm5301x_i2c_data *pdata = dev_id;
	chipcommonbregs_t *ccb = pdata->ccb;
	uint32 status = R_REG(SI_OSH, &ccb->smbus_event_status);

	BCM5301X_MSG_DBG("enter irq %d status 0x%x js %lu consumed %d",
		irqno, status, jiffies, pdata->consumed);

	W_REG(SI_OSH, &ccb->smbus_event_status, status);

	if (status & BCM5301X_SMBUS_RX_STATUS_MASK) {
		bcm5301x_rx_data(pdata);
	}
	if (status & BCM5301X_SMBUS_TX_STATUS_MASK) {
		bcm5301x_tx_data(pdata, BCM5301X_SMBUS_FIFO_DEPTH);
	}
	if (status & BCM5301X_SMBUS_EVENT_STATUS_MASTER_START_BUSY_B) {
		complete(&pdata->done);
	}

	BCM5301X_MSG_DBG("leave irq %d status 0x%x js %lu consumed %d",
		irqno, status, jiffies, pdata->consumed);

	return IRQ_HANDLED;
}

static int __devinit bcm5301x_hwinit(struct bcm5301x_i2c_data *pdata)
{
	si_t *sih;
	chipcommonbregs_t *ccb;

	sih = si_kattach(SI_OSH);
	if (!sih) {
		BCM5301X_MSG_ERR("can't get sih");
		return -ENXIO;
	}

	ccb = si_setcore(sih, NS_CCB_CORE_ID, 0);
	if (!ccb) {
		BCM5301X_MSG_ERR("can't get ccb");
		return -ENXIO;
	}
	pdata->ccb = ccb;

	bcm5301x_reset_and_en(pdata);

	return 0;
}

static struct i2c_algorithm bcm5301x_algorithm = {
	.master_xfer	= bcm5301x_xfer,
	.functionality	= bcm5301x_func,
};

static int __devinit bcm5301x_i2c_probe(struct platform_device *pdev)
{
	struct bcm5301x_i2c_data *pdata;
	struct i2c_adapter *adap;
	struct resource *res;
	int irq;
	bool fast;
	int rc;

	BCM5301X_MSG_INFO("probe");

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		BCM5301X_MSG_ERR("no resources");
		rc = -EINVAL;
		goto fail_get_res;
	}
	irq = res->start;
	fast = res->end;
	if (irq <= 0) {
		BCM5301X_MSG_ERR("bad irqno %d", irq);
		rc = -EINVAL;
		goto fail_get_res;
	}
	BCM5301X_MSG_INFO("irqno %d fast %d", irq, fast);

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (pdata == NULL) {
		BCM5301X_MSG_ERR("failed to allocate data");
		rc = -ENOMEM;
		goto fail_alloc;
	}
	pdata->irq = irq;
	pdata->fast = fast;
	init_completion(&pdata->done);
	adap = &pdata->adap;
	snprintf(adap->name, sizeof(adap->name), BCM5301X_CFG_DRV_NAME);
	adap->algo = &bcm5301x_algorithm;
	adap->algo_data = pdata;
	adap->dev.parent = &pdev->dev;
	adap->nr = pdev->id;
	platform_set_drvdata(pdev, pdata);

	rc = bcm5301x_hwinit(pdata);
	if (rc) {
		BCM5301X_MSG_ERR("hw init failed");
		goto fail_hwinit;
	}

	rc = request_irq(irq, bcm5301x_i2c_irq, 0, BCM5301X_CFG_DRV_NAME, pdata);
	if (rc) {
		BCM5301X_MSG_ERR("failed to request irqno %d", irq);
		goto fail_request_irq;
	}

	rc = i2c_add_numbered_adapter(adap);
	if (rc) {
		BCM5301X_MSG_ERR("adapter registration failed");
		goto fail_add_adapter;
	}

	rc = bcm5301x_bb_init(pdev, pdata);
	if (rc) {
		BCM5301X_MSG_ERR("bb init failed");
		goto fail_bb_init;
	}

	printk(KERN_INFO"%s : adapter %d created\n", BCM5301X_CFG_DRV_NAME, adap->id);

	return 0;

fail_bb_init:
	i2c_del_adapter(adap);
fail_add_adapter:
	free_irq(irq, pdata);
fail_request_irq:
fail_hwinit:
	platform_set_drvdata(pdev, NULL);
	kfree(pdata);
fail_alloc:
fail_get_res:
	return rc;
}

static int __devexit bcm5301x_i2c_remove(struct platform_device *pdev)
{
	struct bcm5301x_i2c_data *pdata = platform_get_drvdata(pdev);

	BCM5301X_MSG_INFO("remove");

	i2c_del_adapter(&pdata->adap);
	i2c_del_adapter(&pdata->bb_adap);

	bcm5301x_reset(pdata);

	free_irq(pdata->irq, pdata);

	platform_set_drvdata(pdev, NULL);
	kfree(pdata);

	return 0;
}

static struct platform_driver bcm5301x_i2c_driver = {
	.probe		= bcm5301x_i2c_probe,
	.remove		= __devexit_p(bcm5301x_i2c_remove),
	.driver		= {
		.name	= BCM5301X_CFG_DRV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init bcm5301x_i2c_init(void)
{
	return platform_driver_register(&bcm5301x_i2c_driver);
}

static void __exit bcm5301x_i2c_exit(void)
{
	platform_driver_unregister(&bcm5301x_i2c_driver);
}

module_init(bcm5301x_i2c_init);
module_exit(bcm5301x_i2c_exit);
