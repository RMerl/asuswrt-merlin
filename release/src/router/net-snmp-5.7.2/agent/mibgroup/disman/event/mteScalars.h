#ifndef MTESCALARS_H
#define MTESCALARS_H

void            init_mteScalars(void);
Netsnmp_Node_Handler handle_mteResourceGroup;
Netsnmp_Node_Handler handle_mteTriggerFailures;

#define	MTE_RESOURCE_SAMPLE_MINFREQ		1
#define	MTE_RESOURCE_SAMPLE_MAX_INST		2
#define	MTE_RESOURCE_SAMPLE_INSTANCES		3
#define	MTE_RESOURCE_SAMPLE_HIGH		4
#define	MTE_RESOURCE_SAMPLE_LACKS		5

#endif                          /* MTESCALARS_H */
