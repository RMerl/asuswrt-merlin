/*
 */

#ifndef AUDIOCHIP_H
#define AUDIOCHIP_H

enum audiochip {
	AUDIO_CHIP_NONE,
	AUDIO_CHIP_UNKNOWN,
	/* Provided by video chip */
	AUDIO_CHIP_INTERNAL,
	/* Provided by tvaudio.c */
	AUDIO_CHIP_TDA8425,
	AUDIO_CHIP_TEA6300,
	AUDIO_CHIP_TEA6420,
	AUDIO_CHIP_TDA9840,
	AUDIO_CHIP_TDA985X,
	AUDIO_CHIP_TDA9874,
	AUDIO_CHIP_PIC16C54,
	/* Provided by msp3400.c */
	AUDIO_CHIP_MSP34XX,
	/* Provided by wm8775.c */
	AUDIO_CHIP_WM8775
};

#endif /* AUDIOCHIP_H */
