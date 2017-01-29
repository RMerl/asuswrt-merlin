/* Copyright 2001 Sun Microsystems (thockin@sun.com) */
#include <stdio.h>
#include "internal.h"

static const char * const csr0_tap[4] = {
	"No transmit automatic polling",
	"Transmit automatic polling every 200 seconds",
	"Transmit automatic polling every 800 seconds",
	"Transmit automatic polling every 1.6 milliseconds",
};

static const char * const csr0_cache_al[4] = {
	"not used",
	"8-longword boundary alignment",
	"16-longword boundary alignment",
	"32-longword boundary alignment",
};

static const char * const csr5_buserr[8] = {
	"      Bus error: parity",
	"      Bus error: master abort",
	"      Bus error: target abort",
	"      Bus error: (unknown code, reserved)",
	"      Bus error: (unknown code, reserved)",
	"      Bus error: (unknown code, reserved)",
	"      Bus error: (unknown code, reserved)",
	"      Bus error: (unknown code, reserved)",
};

static const int csr6_tx_thresh[4] = {
	72,
	96,
	128,
	160,
};

static const char * const csr6_om[4] = {
	"normal",
	"internal loopback",
	"external loopback",
	"unknown (not used)",
};

static const char * const csr5_tx_state[8] = {
	"stopped",
	"running: fetch desc",
	"running: wait xmit end",
	"running: read buf",
	"unknown (reserved)",
	"running: setup packet",
	"suspended",
	"running: close desc",
};

static const char * const csr5_rx_state[8] = {
	"stopped",
	"running: fetch desc",
	"running: chk pkt end",
	"running: wait for pkt",
	"suspended",
	"running: close",
	"running: flush",
	"running: queue",
};

static const char * const csr12_nway_state[8] = {
	"Autonegotiation disable",
	"Transmit disable",
	"Ability detect",
	"Acknowledge detect",
	"Complete acknowledge",
	"FLP link good, nway complete",
	"Link check",
	"unknown (reserved)",
};

static const char * const csr14_tp_comp[4] = {
	"Compensation Disabled Mode",
	"Compensation Disabled Mode",
	"High Power Mode",
	"Normal Compensation Mode",
};

static void
print_ring_addresses(u32 csr3, u32 csr4)
{
	fprintf(stdout,
		"0x18: CSR3 (Rx Ring Base Address)        0x%08x\n"
		"0x20: CSR4 (Tx Ring Base Address)        0x%08x\n"
		,
		csr3,
		csr4);
}

static void
print_rx_missed(u32 csr8)
{
	fprintf(stdout,
		"0x40: CSR8 (Missed Frames Counter)       0x%08x\n", csr8);
	if (csr8 & (1 << 16))
		fprintf(stdout,
		"      Counter overflow\n");
	else {
		unsigned int rx_missed = csr8 & 0xffff;
		if (!rx_missed)
			fprintf(stdout,
			"      No missed frames\n");
		else
			fprintf(stdout,
			"      %u missed frames\n", rx_missed);
	}
}

static void
de21040_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	u32 tmp, v, *data = (u32 *)regs->data;

	fprintf(stdout, "21040 Registers\n");
	fprintf(stdout, "---------------\n");

	/*
	 * CSR0
	 */
	v = data[0];
	fprintf(stdout,
		"0x00: CSR0 (Bus Mode)                    0x%08x\n"
		"      %s\n"
		"      %s address space\n"
		"      Cache alignment: %s\n"
		,
		v,
		csr0_tap[(v >> 17) & 3],
		v & (1 << 16) ? "Diagnostic" : "Standard",
		csr0_cache_al[(v >> 14) & 3]);
	tmp = (v >> 8) & 0x3f;
	if (tmp == 0)
		fprintf(stdout, "      Programmable burst length unlimited\n");
	else
		fprintf(stdout,
			"      Programmable burst length %d longwords\n",
			tmp);
	fprintf(stdout,
		"      %s endian data buffers\n"
		"      Descriptor skip length %d longwords\n"
		"      %s bus arbitration scheme\n"
		,
		v & (1 << 7) ? "Big" : "Little",
		(v >> 2) & 0x1f,
		v & (1 << 1) ? "Round-robin" : "RX-has-priority");
	if (v & (1 << 0))
		fprintf(stdout, "      Software reset asserted\n");

	/*
	 * CSR3, 4
	 */
	print_ring_addresses(data[3], data[4]);

	/*
	 * CSR5
	 */
	v = data[5];
	fprintf(stdout,
		"0x28: CSR5 (Status)                      0x%08x\n"
		"%s"
		"      Transmit process %s\n"
		"      Receive process %s\n"
		"      Link %s\n"
		,
		v,
		v & (1 << 13) ? csr5_buserr[(v >> 23) & 0x7] : "",
		csr5_tx_state[(v >> 20) & 0x7],
		csr5_rx_state[(v >> 17) & 0x7],
		v & (1 << 12) ? "fail" : "OK");
	if (v & (1 << 16))
		fprintf(stdout,
		"      Normal interrupts: %s%s%s\n"
		,
		v & (1 << 0) ? "TxOK " : "",
		v & (1 << 2) ? "TxNoBufs " : "",
		v & (1 << 6) ? "RxOK" : "");
	if (v & (1 << 15))
		fprintf(stdout,
		"      Abnormal intr: %s%s%s%s%s%s%s%s\n"
		,
		v & (1 << 1) ? "TxStop " : "",
		v & (1 << 3) ? "TxJabber " : "",
		v & (1 << 5) ? "TxUnder " : "",
		v & (1 << 7) ? "RxNoBufs " : "",
		v & (1 << 8) ? "RxStopped " : "",
		v & (1 << 9) ? "RxTimeout " : "",
		v & (1 << 10) ? "AUI_TP " : "",
		v & (1 << 11) ? "FD_Short " : "");

	/*
	 * CSR6
	 */
	v = data[6];
	fprintf(stdout,
		"0x30: CSR6 (Operating Mode)              0x%08x\n"
		"%s"
		"%s"
		"      Transmit threshold %d bytes\n"
		"      Transmit DMA %sabled\n"
		"%s"
		"      Operating mode: %s\n"
		"      %s duplex\n"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"      Receive DMA %sabled\n"
		"      %s filtering mode\n"
		,
		v,
		v & (1<<17) ? "      Capture effect enabled\n" : "",
		v & (1<<16) ? "      Back pressure enabled\n" : "",
		csr6_tx_thresh[(v >> 14) & 3],
		v & (1<<13) ? "en" : "dis",
		v & (1<<12) ? "      Forcing collisions\n" : "",
		csr6_om[(v >> 10) & 3],
		v & (1<<9) ? "Full" : "Half",
		v & (1<<8) ? "      Flaky oscillator disable\n" : "",
		v & (1<<7) ? "      Pass All Multicast\n" : "",
		v & (1<<6) ? "      Promisc Mode\n" : "",
		v & (1<<5) ? "      Start/Stop Backoff Counter\n" : "",
		v & (1<<4) ? "      Inverse Filtering\n" : "",
		v & (1<<3) ? "      Pass Bad Frames\n" : "",
		v & (1<<2) ? "      Hash-only Filtering\n" : "",
		v & (1<<1) ? "en" : "dis",
		v & (1<<0) ? "Hash" : "Perfect");

	/*
	 * CSR7
	 */
	v = data[7];
	fprintf(stdout,
		"0x38: CSR7 (Interrupt Mask)              0x%08x\n"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		,
		v,
		v & (1<<16) ? "      Normal interrupt summary\n" : "",
		v & (1<<15) ? "      Abnormal interrupt summary\n" : "",
		v & (1<<13) ? "      System error\n" : "",
		v & (1<<12) ? "      Link fail\n" : "",
		v & (1<<11) ? "      Full duplex\n" : "",
		v & (1<<10) ? "      AUI_TP pin\n" : "",
		v & (1<<9) ? "      Receive watchdog timeout\n" : "",
		v & (1<<8) ? "      Receive stopped\n" : "",
		v & (1<<7) ? "      Receive buffer unavailable\n" : "",
		v & (1<<6) ? "      Receive interrupt\n" : "",
		v & (1<<5) ? "      Transmit underflow\n" : "",
		v & (1<<3) ? "      Transmit jabber timeout\n" : "",
		v & (1<<2) ? "      Transmit buffer unavailable\n" : "",
		v & (1<<1) ? "      Transmit stopped\n" : "",
		v & (1<<0) ? "      Transmit interrupt\n" : "");

	/*
	 * CSR8
	 */
	print_rx_missed(data[8]);

	/*
	 * CSR9
	 */
	v = data[9];
	fprintf(stdout,
		"0x48: CSR9 (Ethernet Address ROM)        0x%08x\n", v);

	/*
	 * CSR11
	 */
	v = data[11];
	fprintf(stdout,
		"0x58: CSR11 (Full Duplex Autoconfig)     0x%08x\n", v);

	/*
	 * CSR12
	 */
	v = data[12];
	fprintf(stdout,
		"0x60: CSR12 (SIA Status)                 0x%08x\n"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"      AUI_TP pin: %s\n"
		,
		v,
		v & (1<<7) ? "      PLL sampler high\n" : "",
		v & (1<<6) ? "      PLL sampler low\n" : "",
		v & (1<<5) ? "      PLL self-test pass\n" : "",
		v & (1<<4) ? "      PLL self-test done\n" : "",
		v & (1<<3) ? "      Autopolarity state\n" : "",
		v & (1<<2) ? "      Link fail\n" : "",
		v & (1<<1) ? "      Network connection error\n" : "",
		v & (1<<0) ? "AUI" : "TP");

	/*
	 * CSR13
	 */
	v = data[13];
	fprintf(stdout,
		"0x68: CSR13 (SIA Connectivity)           0x%08x\n"
		"%s"
		"%s"
		"%s"
		"%s"
		"      External port output multiplexer select: %u%u%u%u\n"
		"%s"
		"%s"
		"%s"
		"%s"
		"      %s interface selected\n"
		"%s"
		"%s"
		"%s"
		,
		v,
		v & (1<<15) ? "      Enable pins 5, 6, 7\n" : "",
		v & (1<<14) ? "      Enable pins 2, 4\n" : "",
		v & (1<<13) ? "      Enable pins 1, 3\n" : "",
		v & (1<<12) ? "      Input enable\n" : "",
		v & (1<<11) ? 1 : 0,
		v & (1<<10) ? 1 : 0,
		v & (1<<9) ? 1 : 0,
		v & (1<<8) ? 1 : 0,
		v & (1<<7) ? "      APLL start\n" : "",
		v & (1<<6) ? "      Serial interface input multiplexer\n" : "",
		v & (1<<5) ? "      Encoder input multiplexer\n" : "",
		v & (1<<4) ? "      SIA PLL external input enable\n" : "",
		v & (1<<3) ? "AUI" : "10base-T",
		v & (1<<2) ? "      CSR autoconfiguration\n" : "",
		v & (1<<1) ? "      AUI_TP pin autoconfiguration\n" : "",
		v & (1<<0) ? "      SIA reset\n" : "");

	/*
	 * CSR14
	 */
	v = data[14];
	fprintf(stdout,
		"0x70: CSR14 (SIA Transmit and Receive)   0x%08x\n"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"      %s\n"
		"%s"
		"%s"
		"%s"
		"%s"
		,
		v,
		v & (1<<14) ? "      Set polarity plus\n" : "",
		v & (1<<13) ? "      Autopolarity enable\n" : "",
		v & (1<<12) ? "      Link test enable\n" : "",
		v & (1<<11) ? "      Heartbeat enable\n" : "",
		v & (1<<10) ? "      Collision detect enable\n" : "",
		v & (1<<9) ? "      Collision squelch enable\n" : "",
		v & (1<<8) ? "      Receive squelch enable\n" : "",
		csr14_tp_comp[(v >> 4) & 0x3],
		v & (1<<3) ? "      Link pulse send enable\n" : "",
		v & (1<<2) ? "      Driver enable\n" : "",
		v & (1<<1) ? "      Loopback enable\n" : "",
		v & (1<<0) ? "      Encoder enable\n" : "");

	/*
	 * CSR15
	 */
	v = data[15];
	fprintf(stdout,
		"0x78: CSR15 (SIA General)                0x%08x\n"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		,
		v,
		v & (1<<13) ? "      Force receiver low\n" : "",
		v & (1<<12) ? "      PLL self-test start\n" : "",
		v & (1<<11) ? "      Force link fail\n" : "",
		v & (1<<9) ? "      Force unsquelch\n" : "",
		v & (1<<8) ? "      Test clock\n" : "",
		v & (1<<5) ? "      Receive watchdog release\n" : "",
		v & (1<<4) ? "      Receive watchdog disable\n" : "",
		v & (1<<2) ? "      Jabber clock\n" : "",
		v & (1<<1) ? "      Host unjab\n" : "",
		v & (1<<0) ? "      Jabber disable\n" : "");
}

static void
de21041_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	u32 tmp, v, *data = (u32 *)regs->data;

	fprintf(stdout, "21041 Registers\n");
	fprintf(stdout, "---------------\n");

	/*
	 * CSR0
	 */
	v = data[0];
	fprintf(stdout,
		"0x00: CSR0 (Bus Mode)                    0x%08x\n"
		"      %s endian descriptors\n"
		"      %s\n"
		"      %s address space\n"
		"      Cache alignment: %s\n"
		,
		v,
		v & (1 << 20) ? "Big" : "Little",
		csr0_tap[(v >> 17) & 3],
		v & (1 << 16) ? "Diagnostic" : "Standard",
		csr0_cache_al[(v >> 14) & 3]);
	tmp = (v >> 8) & 0x3f;
	if (tmp == 0)
		fprintf(stdout, "      Programmable burst length unlimited\n");
	else
		fprintf(stdout,
			"      Programmable burst length %d longwords\n",
			tmp);
	fprintf(stdout,
		"      %s endian data buffers\n"
		"      Descriptor skip length %d longwords\n"
		"      %s bus arbitration scheme\n"
		,
		v & (1 << 7) ? "Big" : "Little",
		(v >> 2) & 0x1f,
		v & (1 << 1) ? "Round-robin" : "RX-has-priority");
	if (v & (1 << 0))
		fprintf(stdout, "      Software reset asserted\n");

	/*
	 * CSR3, 4
	 */
	print_ring_addresses(data[3], data[4]);

	/*
	 * CSR5
	 */
	v = data[5];
	fprintf(stdout,
		"0x28: CSR5 (Status)                      0x%08x\n"
		"%s"
		"      Transmit process %s\n"
		"      Receive process %s\n"
		"      Link %s\n"
		,
		v,
		v & (1 << 13) ? csr5_buserr[(v >> 23) & 0x7] : "",
		csr5_tx_state[(v >> 20) & 0x7],
		csr5_rx_state[(v >> 17) & 0x7],
		v & (1 << 12) ? "fail" : "OK");
	if (v & (1 << 16))
		fprintf(stdout,
		"      Normal interrupts: %s%s%s%s%s\n"
		,
		v & (1 << 0) ? "TxOK " : "",
		v & (1 << 2) ? "TxNoBufs " : "",
		v & (1 << 6) ? "RxOK" : "",
		v & (1 << 11) ? "TimerExp " : "",
		v & (1 << 14) ? "EarlyRx " : "");
	if (v & (1 << 15))
		fprintf(stdout,
		"      Abnormal intr: %s%s%s%s%s%s%s\n"
		,
		v & (1 << 1) ? "TxStop " : "",
		v & (1 << 3) ? "TxJabber " : "",
		v & (1 << 4) ? "ANC " : "",
		v & (1 << 5) ? "TxUnder " : "",
		v & (1 << 7) ? "RxNoBufs " : "",
		v & (1 << 8) ? "RxStopped " : "",
		v & (1 << 9) ? "RxTimeout " : "");

	/*
	 * CSR6
	 */
	v = data[6];
	fprintf(stdout,
		"0x30: CSR6 (Operating Mode)              0x%08x\n"
		"%s"
		"%s"
		"      Transmit threshold %d bytes\n"
		"      Transmit DMA %sabled\n"
		"%s"
		"      Operating mode: %s\n"
		"      %s duplex\n"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"      Receive DMA %sabled\n"
		"      %s filtering mode\n"
		,
		v,
		v & (1<<31) ? "      Special capture effect enabled\n" : "",
		v & (1<<17) ? "      Capture effect enabled\n" : "",
		csr6_tx_thresh[(v >> 14) & 3],
		v & (1<<13) ? "en" : "dis",
		v & (1<<12) ? "      Forcing collisions\n" : "",
		csr6_om[(v >> 10) & 3],
		v & (1<<9) ? "Full" : "Half",
		v & (1<<8) ? "      Flaky oscillator disable\n" : "",
		v & (1<<7) ? "      Pass All Multicast\n" : "",
		v & (1<<6) ? "      Promisc Mode\n" : "",
		v & (1<<5) ? "      Start/Stop Backoff Counter\n" : "",
		v & (1<<4) ? "      Inverse Filtering\n" : "",
		v & (1<<3) ? "      Pass Bad Frames\n" : "",
		v & (1<<2) ? "      Hash-only Filtering\n" : "",
		v & (1<<1) ? "en" : "dis",
		v & (1<<0) ? "Hash" : "Perfect");

	/*
	 * CSR7
	 */
	v = data[7];
	fprintf(stdout,
		"0x38: CSR7 (Interrupt Mask)              0x%08x\n"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		,
		v,
		v & (1<<16) ? "      Normal interrupt summary\n" : "",
		v & (1<<15) ? "      Abnormal interrupt summary\n" : "",
		v & (1<<14) ? "      Early receive interrupt\n" : "",
		v & (1<<13) ? "      System error\n" : "",
		v & (1<<12) ? "      Link fail\n" : "",
		v & (1<<11) ? "      Timer expired\n" : "",
		v & (1<<9) ? "      Receive watchdog timeout\n" : "",
		v & (1<<8) ? "      Receive stopped\n" : "",
		v & (1<<7) ? "      Receive buffer unavailable\n" : "",
		v & (1<<6) ? "      Receive interrupt\n" : "",
		v & (1<<5) ? "      Transmit underflow\n" : "",
		v & (1<<4) ? "      Link pass\n" : "",
		v & (1<<3) ? "      Transmit jabber timeout\n" : "",
		v & (1<<2) ? "      Transmit buffer unavailable\n" : "",
		v & (1<<1) ? "      Transmit stopped\n" : "",
		v & (1<<0) ? "      Transmit interrupt\n" : "");

	/*
	 * CSR8
	 */
	print_rx_missed(data[8]);

	/*
	 * CSR9
	 */
	v = data[9];
	fprintf(stdout,
		"0x48: CSR9 (Boot and Ethernet ROMs)      0x%08x\n"
		"      Select bits: %s%s%s%s%s%s\n"
		"      Data: %d%d%d%d%d%d%d%d\n"
		,
		v,
		v & (1<<15) ? "Mode " : "",
		v & (1<<14) ? "Read " : "",
		v & (1<<13) ? "Write " : "",
		v & (1<<12) ? "BootROM " : "",
		v & (1<<11) ? "SROM " : "",
		v & (1<<10) ? "ExtReg " : "",
		v & (1<<7) ? 1 : 0,
		v & (1<<6) ? 1 : 0,
		v & (1<<5) ? 1 : 0,
		v & (1<<4) ? 1 : 0,
		v & (1<<3) ? 1 : 0,
		v & (1<<2) ? 1 : 0,
		v & (1<<1) ? 1 : 0,
		v & (1<<0) ? 1 : 0);

	/*
	 * CSR10
	 */
	v = data[10];
	fprintf(stdout,
		"0x50: CSR10 (Boot ROM Address)           0x%08x\n", v);

	/*
	 * CSR11
	 */
	v = data[11];
	fprintf(stdout,
		"0x58: CSR11 (General Purpose Timer)      0x%08x\n"
		"%s"
		"      Timer value: %u cycles\n"
		,
		v,
		v & (1<<16) ? "      Continuous mode\n" : "",
		v & 0xffff);

	/*
	 * CSR12
	 */
	v = data[12];
	fprintf(stdout,
		"0x60: CSR12 (SIA Status)                 0x%08x\n"
		"      Link partner code word 0x%04x\n"
		"%s"
		"      NWay state: %s\n"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		,
		v,
		v >> 16,
		v & (1<<15) ? "      Link partner negotiable\n" : "",
		csr12_nway_state[(v >> 12) & 0x7],
		v & (1<<11) ? "      Transmit remote fault\n" : "",
		v & (1<<10) ? "      Unstable NLP detected\n" : "",
		v & (1<<9) ? "      Non-selected port receive activity\n" : "",
		v & (1<<8) ? "      Selected port receive activity\n" : "",
		v & (1<<7) ? "      PLL sampler high\n" : "",
		v & (1<<6) ? "      PLL sampler low\n" : "",
		v & (1<<5) ? "      PLL self-test pass\n" : "",
		v & (1<<4) ? "      PLL self-test done\n" : "",
		v & (1<<3) ? "      Autopolarity state\n" : "",
		v & (1<<2) ? "      Link fail\n" : "",
		v & (1<<1) ? "      Network connection error\n" : "");

	/*
	 * CSR13
	 */
	v = data[13];
	fprintf(stdout,
		"0x68: CSR13 (SIA Connectivity)           0x%08x\n"
		"      SIA Diagnostic Mode 0x%04x\n"
		"      %s\n"
		"%s"
		"%s"
		,
		v,
		(v >> 4) & 0xfff,
		v & (1<<3) ? "AUI/BNC port" : "10base-T port",
		v & (1<<2) ? "      CSR autoconfiguration enabled\n" : "",
		v & (1<<0) ? "      SIA register reset asserted\n" : "");

	/*
	 * CSR14
	 */
	v = data[14];
	fprintf(stdout,
		"0x70: CSR14 (SIA Transmit and Receive)   0x%08x\n"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"      %s\n"
		"%s"
		"%s"
		"%s"
		"%s"
		,
		v,
		v & (1<<15) ? "      10base-T/AUI autosensing\n" : "",
		v & (1<<14) ? "      Set polarity plus\n" : "",
		v & (1<<13) ? "      Autopolarity enable\n" : "",
		v & (1<<12) ? "      Link test enable\n" : "",
		v & (1<<11) ? "      Heartbeat enable\n" : "",
		v & (1<<10) ? "      Collision detect enable\n" : "",
		v & (1<<9) ? "      Collision squelch enable\n" : "",
		v & (1<<8) ? "      Receive squelch enable\n" : "",
		v & (1<<7) ? "      Autonegotiation enable\n" : "",
		v & (1<<6) ? "      Must Be One\n" : "",
		csr14_tp_comp[(v >> 4) & 0x3],
		v & (1<<3) ? "      Link pulse send enable\n" : "",
		v & (1<<2) ? "      Driver enable\n" : "",
		v & (1<<1) ? "      Loopback enable\n" : "",
		v & (1<<0) ? "      Encoder enable\n" : "");

	/*
	 * CSR15
	 */
	v = data[15];
	fprintf(stdout,
		"0x78: CSR15 (SIA General)                0x%08x\n"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"%s"
		"      %s port selected\n"
		"%s"
		"%s"
		"%s"
		,
		v,
		v & (1<<15) ? "      GP LED2 on\n" : "",
		v & (1<<14) ? "      GP LED2 enable\n" : "",
		v & (1<<13) ? "      Force receiver low\n" : "",
		v & (1<<12) ? "      PLL self-test start\n" : "",
		v & (1<<11) ? "      LED stretch disable\n" : "",
		v & (1<<10) ? "      Force link fail\n" : "",
		v & (1<<9) ? "      Force unsquelch\n" : "",
		v & (1<<8) ? "      Test clock\n" : "",
		v & (1<<7) ? "      GP LED1 on\n" : "",
		v & (1<<6) ? "      GP LED1 enable\n" : "",
		v & (1<<5) ? "      Receive watchdog release\n" : "",
		v & (1<<4) ? "      Receive watchdog disable\n" : "",
		v & (1<<3) ? "AUI" : "BNC",
		v & (1<<2) ? "      Jabber clock\n" : "",
		v & (1<<1) ? "      Host unjab\n" : "",
		v & (1<<0) ? "      Jabber disable\n" : "");
}

int
de2104x_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	unsigned int de21040 = regs->version & 1;

	if (de21040)
		de21040_dump_regs(info, regs);
	else
		de21041_dump_regs(info, regs);

	return 0;
}

