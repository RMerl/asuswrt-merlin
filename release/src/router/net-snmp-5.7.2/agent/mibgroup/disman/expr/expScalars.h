#ifndef EXPSCALARS_H
#define EXPSCALARS_H

void            init_expScalars(void);
Netsnmp_Node_Handler handle_expResourceGroup;

#define	EXP_RESOURCE_MIN_DELTA			1
#define	EXP_RESOURCE_SAMPLE_MAX			2
#define	EXP_RESOURCE_SAMPLE_INSTANCES		3
#define	EXP_RESOURCE_SAMPLE_HIGH		4
#define	EXP_RESOURCE_SAMPLE_LACKS		5

#endif                          /* EXPSCALARS_H */
