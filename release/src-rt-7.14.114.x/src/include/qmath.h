/*
 * qmath functions used in arithmatic and DSP operations where
 * fractional operations, saturation support is needed.
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: qmath.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef __QMATH_H__
#define __QMATH_H__

#include <typedefs.h>

int16
qm_sat32(int32 op);

int32
qm_mul321616(int16 op1, int16 op2);

int16
qm_mul16(int16 op1, int16 op2);

int32
qm_muls321616(int16 op1, int16 op2);

uint16
qm_mulu16(uint16 op1, uint16 op2);

int16
qm_muls16(int16 op1, int16 op2);

int32
qm_add32(int32 op1, int32 op2);

int16
qm_add16(int16 op1, int16 op2);

int16
qm_sub16(int16 op1, int16 op2);

int32
qm_sub32(int32 op1, int32 op2);

int32
qm_mac321616(int32 acc, int16 op1, int16 op2);

int32
qm_shl32(int32 op, int shift);

int32
qm_shr32(int32 op, int shift);

int16
qm_shl16(int16 op, int shift);

int16
qm_shr16(int16 op, int shift);

int16
qm_norm16(int16 op);

int16
qm_norm32(int32 op);

int16
qm_div_s(int16 num, int16 denom);

int16
qm_abs16(int16 op);

int16
qm_div16(int16 num, int16 denom, int16 *qQuotient);

int32
qm_abs32(int32 op);

int16
qm_div163232(int32 num, int32 denom, int16 *qquotient);

int32
qm_mul323216(int32 op1, int16 op2);

int32
qm_mulsu321616(int16 op1, uint16 op2);

int32
qm_muls323216(int32 op1, int16 op2);

int32
qm_mul32(int32 a, int32 b);

int32
qm_muls32(int32 a, int32 b);

void
qm_log10(int32 N, int16 qN, int16 *log10N, int16 *qLog10N);

void
qm_1byN(int32 N, int16 qN, int32 *result, int16 *qResult);

#endif /* #ifndef __QMATH_H__ */
