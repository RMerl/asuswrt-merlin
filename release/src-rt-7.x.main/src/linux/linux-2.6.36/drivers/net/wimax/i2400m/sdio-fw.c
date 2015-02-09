/*
 * Intel Wireless WiMAX Connection 2400m
 * Firmware uploader's SDIO specifics
 *
 *
 * Copyright (C) 2007-2008 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Intel Corporation <linux-wimax@intel.com>
 * Yanir Lubetkin <yanirx.lubetkin@intel.com>
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *  - Initial implementation
 *
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *  - Bus generic/specific split for USB
 *
 * Dirk Brandewie <dirk.j.brandewie@intel.com>
 *  - Initial implementation for SDIO
 *
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *  - SDIO rehash for changes in the bus-driver model
 *
 * Dirk Brandewie <dirk.j.brandewie@intel.com>
 *  - Make it IRQ based, not polling
 *
 * THE PROCEDURE
 *
 * See fw.c for the generic description of this procedure.
 *
 * This file implements only the SDIO specifics. It boils down to how
 * to send a command and waiting for an acknowledgement from the
 * device.
 *
 * All this code is sequential -- all i2400ms_bus_bm_*() functions are
 * executed in the same thread, except i2400ms_bm_irq() [on its own by
 * the SDIO driver]. This makes it possible to avoid locking.
 *
 * COMMAND EXECUTION
 *
 * The generic firmware upload code will call i2400m_bus_bm_cmd_send()
 * to send commands.
 *
 * The SDIO devices expects things in 256 byte blocks, so it will pad
 * it, compute the checksum (if needed) and pass it to SDIO.
 *
 * ACK RECEPTION
 *
 * This works in IRQ mode -- the fw loader says when to wait for data
 * and for that it calls i2400ms_bus_bm_wait_for_ack().
 *
 * This checks if there is any data available (RX size > 0); if not,
 * waits for the IRQ handler to notify about it. Once there is data,
 * it is read and passed to the caller. Doing it this way we don't
 * need much coordination/locking, and it makes it much more difficult
 * for an interrupt to be lost and the wait_for_ack() function getting
 * stuck even when data is pending.
 */
#include <linux/mmc/sdio_func.h>
#include "i2400m-sdio.h"


#define D_SUBMODULE fw
#include "sdio-debug-levels.h"


/*
 * Send a boot-mode command to the SDIO function
 *
 * We use a bounce buffer (i2400m->bm_cmd_buf) because we need to
 * touch the header if the RAW flag is not set.
 *
 * @flags: pass thru from i2400m_bm_cmd()
 * @return: cmd_size if ok, < 0 errno code on error.
 *
 * Note the command is padded to the SDIO block size for the device.
 */
ssize_t i2400ms_bus_bm_cmd_send(struct i2400m *i2400m,
				const struct i2400m_bootrom_header *_cmd,
				size_t cmd_size, int flags)
{
	ssize_t result;
	struct device *dev = i2400m_dev(i2400m);
	struct i2400ms *i2400ms = container_of(i2400m, struct i2400ms, i2400m);
	int opcode = _cmd == NULL ? -1 : i2400m_brh_get_opcode(_cmd);
	struct i2400m_bootrom_header *cmd;
	/* SDIO restriction */
	size_t cmd_size_a = ALIGN(cmd_size, I2400MS_BLK_SIZE);

	d_fnstart(5, dev, "(i2400m %p cmd %p size %zu)\n",
		  i2400m, _cmd, cmd_size);
	result = -E2BIG;
	if (cmd_size > I2400M_BM_CMD_BUF_SIZE)
		goto error_too_big;

	if (_cmd != i2400m->bm_cmd_buf)
		memmove(i2400m->bm_cmd_buf, _cmd, cmd_size);
	cmd = i2400m->bm_cmd_buf;
	if (cmd_size_a > cmd_size)			/* Zero pad space */
		memset(i2400m->bm_cmd_buf + cmd_size, 0, cmd_size_a - cmd_size);
	if ((flags & I2400M_BM_CMD_RAW) == 0) {
		if (WARN_ON(i2400m_brh_get_response_required(cmd) == 0))
			dev_warn(dev, "SW BUG: response_required == 0\n");
		i2400m_bm_cmd_prepare(cmd);
	}
	d_printf(4, dev, "BM cmd %d: %zu bytes (%zu padded)\n",
		 opcode, cmd_size, cmd_size_a);
	d_dump(5, dev, cmd, cmd_size);

	sdio_claim_host(i2400ms->func);			/* Send & check */
	result = sdio_memcpy_toio(i2400ms->func, I2400MS_DATA_ADDR,
				  i2400m->bm_cmd_buf, cmd_size_a);
	sdio_release_host(i2400ms->func);
	if (result < 0) {
		dev_err(dev, "BM cmd %d: cannot send: %ld\n",
			opcode, (long) result);
		goto error_cmd_send;
	}
	result = cmd_size;
error_cmd_send:
error_too_big:
	d_fnend(5, dev, "(i2400m %p cmd %p size %zu) = %d\n",
		i2400m, _cmd, cmd_size, (int) result);
	return result;
}


/*
 * Read an ack from the device's boot-mode
 *
 * @i2400m:
 * @_ack: pointer to where to store the read data
 * @ack_size: how many bytes we should read
 *
 * Returns: < 0 errno code on error; otherwise, amount of received bytes.
 *
 * The ACK for a BM command is always at least sizeof(*ack) bytes, so
 * check for that. We don't need to check for device reboots
 *
 */
ssize_t i2400ms_bus_bm_wait_for_ack(struct i2400m *i2400m,
				    struct i2400m_bootrom_header *ack,
				    size_t ack_size)
{
	ssize_t result;
	struct i2400ms *i2400ms = container_of(i2400m, struct i2400ms, i2400m);
	struct sdio_func *func = i2400ms->func;
	struct device *dev = &func->dev;
	int size;

	BUG_ON(sizeof(*ack) > ack_size);

	d_fnstart(5, dev, "(i2400m %p ack %p size %zu)\n",
		  i2400m, ack, ack_size);

	result = wait_event_timeout(i2400ms->bm_wfa_wq,
				    i2400ms->bm_ack_size != -EINPROGRESS,
				    2 * HZ);
	if (result == 0) {
		result = -ETIMEDOUT;
		dev_err(dev, "BM: error waiting for an ack\n");
		goto error_timeout;
	}

	spin_lock(&i2400m->rx_lock);
	result = i2400ms->bm_ack_size;
	BUG_ON(result == -EINPROGRESS);
	if (result < 0)        /* so we exit when rx_release() is called */
		dev_err(dev, "BM: %s failed: %zd\n", __func__, result);
	else {
		size = min(ack_size, i2400ms->bm_ack_size);
		memcpy(ack, i2400m->bm_ack_buf, size);
	}
	/*
	 * Remember always to clear the bm_ack_size to -EINPROGRESS
	 * after the RX data is processed
	 */
	i2400ms->bm_ack_size = -EINPROGRESS;
	spin_unlock(&i2400m->rx_lock);

error_timeout:
	d_fnend(5, dev, "(i2400m %p ack %p size %zu) = %zd\n",
		i2400m, ack, ack_size, result);
	return result;
}
