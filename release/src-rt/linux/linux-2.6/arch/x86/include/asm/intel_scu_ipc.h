#ifndef _ASM_X86_INTEL_SCU_IPC_H_
#define  _ASM_X86_INTEL_SCU_IPC_H_

#define IPCMSG_VRTC	0xFA	 /* Set vRTC device */

/* Command id associated with message IPCMSG_VRTC */
#define IPC_CMD_VRTC_SETTIME      1 /* Set time */
#define IPC_CMD_VRTC_SETALARM     2 /* Set alarm */

/* Read single register */
int intel_scu_ipc_ioread8(u16 addr, u8 *data);

/* Read two sequential registers */
int intel_scu_ipc_ioread16(u16 addr, u16 *data);

/* Read four sequential registers */
int intel_scu_ipc_ioread32(u16 addr, u32 *data);

/* Read a vector */
int intel_scu_ipc_readv(u16 *addr, u8 *data, int len);

/* Write single register */
int intel_scu_ipc_iowrite8(u16 addr, u8 data);

/* Write two sequential registers */
int intel_scu_ipc_iowrite16(u16 addr, u16 data);

/* Write four sequential registers */
int intel_scu_ipc_iowrite32(u16 addr, u32 data);

/* Write a vector */
int intel_scu_ipc_writev(u16 *addr, u8 *data, int len);

/* Update single register based on the mask */
int intel_scu_ipc_update_register(u16 addr, u8 data, u8 mask);

/* Issue commands to the SCU with or without data */
int intel_scu_ipc_simple_command(int cmd, int sub);
int intel_scu_ipc_command(int cmd, int sub, u32 *in, int inlen,
							u32 *out, int outlen);
/* I2C control api */
int intel_scu_ipc_i2c_cntrl(u32 addr, u32 *data);

/* Update FW version */
int intel_scu_ipc_fw_update(u8 *buffer, u32 length);

#endif
