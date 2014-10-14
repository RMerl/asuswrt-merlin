#ifndef I2C_PCA9564_H
#define I2C_PCA9564_H 1

#define I2C_PCA_STA		0x00 /* STATUS  Read Only  */
#define I2C_PCA_TO		0x00 /* TIMEOUT Write Only */
#define I2C_PCA_DAT		0x01 /* DATA    Read/Write */
#define I2C_PCA_ADR		0x02 /* OWN ADR Read/Write */
#define I2C_PCA_CON		0x03 /* CONTROL Read/Write */

#define I2C_PCA_CON_AA		0x80 /* Assert Acknowledge */
#define I2C_PCA_CON_ENSIO	0x40 /* Enable */
#define I2C_PCA_CON_STA		0x20 /* Start */
#define I2C_PCA_CON_STO		0x10 /* Stop */
#define I2C_PCA_CON_SI		0x08 /* Serial Interrupt */
#define I2C_PCA_CON_CR		0x07 /* Clock Rate (MASK) */

#define I2C_PCA_CON_330kHz	0x00
#define I2C_PCA_CON_288kHz	0x01
#define I2C_PCA_CON_217kHz	0x02
#define I2C_PCA_CON_146kHz	0x03
#define I2C_PCA_CON_88kHz	0x04
#define I2C_PCA_CON_59kHz	0x05
#define I2C_PCA_CON_44kHz	0x06
#define I2C_PCA_CON_36kHz	0x07

#endif /* I2C_PCA9564_H */
