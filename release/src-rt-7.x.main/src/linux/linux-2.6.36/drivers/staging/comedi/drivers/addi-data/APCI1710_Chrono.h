/*
 * Copyright (C) 2004,2005  ADDI-DATA GmbH for the source code of this module.
 *
 *	ADDI-DATA GmbH
 *	Dieselstrasse 3
 *	D-77833 Ottersweier
 *	Tel: +19(0)7223/9493-0
 *	Fax: +49(0)7223/9493-92
 *	http://www.addi-data-com
 *	info@addi-data.com
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#define APCI1710_30MHZ			30
#define APCI1710_33MHZ			33
#define APCI1710_40MHZ			40

#define APCI1710_SINGLE			0
#define APCI1710_CONTINUOUS		1

#define APCI1710_CHRONO_PROGRESS_STATUS	0
#define APCI1710_CHRONO_READVALUE	1
#define APCI1710_CHRONO_CONVERTVALUE	2
#define APCI1710_CHRONO_READINTERRUPT	3

#define APCI1710_CHRONO_SET_CHANNELON	0
#define APCI1710_CHRONO_SET_CHANNELOFF	1
#define APCI1710_CHRONO_READ_CHANNEL	2
#define APCI1710_CHRONO_READ_PORT	3

/*
 * CHRONOMETER INISIALISATION FUNCTION
 */
int i_APCI1710_InsnConfigInitChrono(struct comedi_device *dev, struct comedi_subdevice *s,
				    struct comedi_insn *insn, unsigned int *data);

int i_APCI1710_InsnWriteEnableDisableChrono(struct comedi_device *dev,
					    struct comedi_subdevice *s,
					    struct comedi_insn *insn,
					    unsigned int *data);

/*
 * CHRONOMETER READ FUNCTION
 */
int i_APCI1710_InsnReadChrono(struct comedi_device *dev, struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data);

int i_APCI1710_GetChronoProgressStatus(struct comedi_device *dev,
				       unsigned char b_ModulNbr, unsigned char *pb_ChronoStatus);

int i_APCI1710_ReadChronoValue(struct comedi_device *dev,
			       unsigned char b_ModulNbr,
			       unsigned int ui_TimeOut, unsigned char *pb_ChronoStatus,
			       unsigned int *pul_ChronoValue);

int i_APCI1710_ConvertChronoValue(struct comedi_device *dev,
				  unsigned char b_ModulNbr,
				  unsigned int ul_ChronoValue,
				  unsigned int *pul_Hour,
				  unsigned char *pb_Minute,
				  unsigned char *pb_Second,
				  unsigned int *pui_MilliSecond, unsigned int *pui_MicroSecond,
				  unsigned int *pui_NanoSecond);

/*
 * CHRONOMETER DIGITAL INPUT OUTPUT FUNCTION
 */
int i_APCI1710_InsnBitsChronoDigitalIO(struct comedi_device *dev,
				       struct comedi_subdevice *s, struct comedi_insn *insn,
				       unsigned int *data);
