#ifndef LM_SENSORS_MIB_H
#define LM_SENSORS_MIB_H

config_require(hardware/sensors)
config_add_mib(LM-SENSORS-MIB)

/* function declarations */
void init_lmsensorsMib(void);

/*
 * Handler and Column definitions for lmXxxxSensorsTable
 *
 * Note that the same handler (and hence the same
 *  column identifiers) are used for all four tables.
 * This is possible because all the table share the
 *  same structure and behaviour.
 */
Netsnmp_Node_Handler lmSensorsTables_handler;
#define COLUMN_LMSENSORS_INDEX		1
#define COLUMN_LMSENSORS_DEVICE		2
#define COLUMN_LMSENSORS_VALUE		3

#endif /* LM_SENSORS_MIB_H */
