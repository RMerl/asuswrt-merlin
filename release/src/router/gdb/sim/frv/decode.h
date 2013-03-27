/* Decode header for frvbf.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright 1996-2005 Free Software Foundation, Inc.

This file is part of the GNU simulators.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef FRVBF_DECODE_H
#define FRVBF_DECODE_H

extern const IDESC *frvbf_decode (SIM_CPU *, IADDR,
                                  CGEN_INSN_INT, CGEN_INSN_INT,
                                  ARGBUF *);
extern void frvbf_init_idesc_table (SIM_CPU *);
extern void frvbf_sem_init_idesc_table (SIM_CPU *);
extern void frvbf_semf_init_idesc_table (SIM_CPU *);

/* Enum declaration for instructions in cpu family frvbf.  */
typedef enum frvbf_insn_type {
  FRVBF_INSN_X_INVALID, FRVBF_INSN_X_AFTER, FRVBF_INSN_X_BEFORE, FRVBF_INSN_X_CTI_CHAIN
 , FRVBF_INSN_X_CHAIN, FRVBF_INSN_X_BEGIN, FRVBF_INSN_ADD, FRVBF_INSN_SUB
 , FRVBF_INSN_AND, FRVBF_INSN_OR, FRVBF_INSN_XOR, FRVBF_INSN_NOT
 , FRVBF_INSN_SDIV, FRVBF_INSN_NSDIV, FRVBF_INSN_UDIV, FRVBF_INSN_NUDIV
 , FRVBF_INSN_SMUL, FRVBF_INSN_UMUL, FRVBF_INSN_SMU, FRVBF_INSN_SMASS
 , FRVBF_INSN_SMSSS, FRVBF_INSN_SLL, FRVBF_INSN_SRL, FRVBF_INSN_SRA
 , FRVBF_INSN_SLASS, FRVBF_INSN_SCUTSS, FRVBF_INSN_SCAN, FRVBF_INSN_CADD
 , FRVBF_INSN_CSUB, FRVBF_INSN_CAND, FRVBF_INSN_COR, FRVBF_INSN_CXOR
 , FRVBF_INSN_CNOT, FRVBF_INSN_CSMUL, FRVBF_INSN_CSDIV, FRVBF_INSN_CUDIV
 , FRVBF_INSN_CSLL, FRVBF_INSN_CSRL, FRVBF_INSN_CSRA, FRVBF_INSN_CSCAN
 , FRVBF_INSN_ADDCC, FRVBF_INSN_SUBCC, FRVBF_INSN_ANDCC, FRVBF_INSN_ORCC
 , FRVBF_INSN_XORCC, FRVBF_INSN_SLLCC, FRVBF_INSN_SRLCC, FRVBF_INSN_SRACC
 , FRVBF_INSN_SMULCC, FRVBF_INSN_UMULCC, FRVBF_INSN_CADDCC, FRVBF_INSN_CSUBCC
 , FRVBF_INSN_CSMULCC, FRVBF_INSN_CANDCC, FRVBF_INSN_CORCC, FRVBF_INSN_CXORCC
 , FRVBF_INSN_CSLLCC, FRVBF_INSN_CSRLCC, FRVBF_INSN_CSRACC, FRVBF_INSN_ADDX
 , FRVBF_INSN_SUBX, FRVBF_INSN_ADDXCC, FRVBF_INSN_SUBXCC, FRVBF_INSN_ADDSS
 , FRVBF_INSN_SUBSS, FRVBF_INSN_ADDI, FRVBF_INSN_SUBI, FRVBF_INSN_ANDI
 , FRVBF_INSN_ORI, FRVBF_INSN_XORI, FRVBF_INSN_SDIVI, FRVBF_INSN_NSDIVI
 , FRVBF_INSN_UDIVI, FRVBF_INSN_NUDIVI, FRVBF_INSN_SMULI, FRVBF_INSN_UMULI
 , FRVBF_INSN_SLLI, FRVBF_INSN_SRLI, FRVBF_INSN_SRAI, FRVBF_INSN_SCANI
 , FRVBF_INSN_ADDICC, FRVBF_INSN_SUBICC, FRVBF_INSN_ANDICC, FRVBF_INSN_ORICC
 , FRVBF_INSN_XORICC, FRVBF_INSN_SMULICC, FRVBF_INSN_UMULICC, FRVBF_INSN_SLLICC
 , FRVBF_INSN_SRLICC, FRVBF_INSN_SRAICC, FRVBF_INSN_ADDXI, FRVBF_INSN_SUBXI
 , FRVBF_INSN_ADDXICC, FRVBF_INSN_SUBXICC, FRVBF_INSN_CMPB, FRVBF_INSN_CMPBA
 , FRVBF_INSN_SETLO, FRVBF_INSN_SETHI, FRVBF_INSN_SETLOS, FRVBF_INSN_LDSB
 , FRVBF_INSN_LDUB, FRVBF_INSN_LDSH, FRVBF_INSN_LDUH, FRVBF_INSN_LD
 , FRVBF_INSN_LDBF, FRVBF_INSN_LDHF, FRVBF_INSN_LDF, FRVBF_INSN_LDC
 , FRVBF_INSN_NLDSB, FRVBF_INSN_NLDUB, FRVBF_INSN_NLDSH, FRVBF_INSN_NLDUH
 , FRVBF_INSN_NLD, FRVBF_INSN_NLDBF, FRVBF_INSN_NLDHF, FRVBF_INSN_NLDF
 , FRVBF_INSN_LDD, FRVBF_INSN_LDDF, FRVBF_INSN_LDDC, FRVBF_INSN_NLDD
 , FRVBF_INSN_NLDDF, FRVBF_INSN_LDQ, FRVBF_INSN_LDQF, FRVBF_INSN_LDQC
 , FRVBF_INSN_NLDQ, FRVBF_INSN_NLDQF, FRVBF_INSN_LDSBU, FRVBF_INSN_LDUBU
 , FRVBF_INSN_LDSHU, FRVBF_INSN_LDUHU, FRVBF_INSN_LDU, FRVBF_INSN_NLDSBU
 , FRVBF_INSN_NLDUBU, FRVBF_INSN_NLDSHU, FRVBF_INSN_NLDUHU, FRVBF_INSN_NLDU
 , FRVBF_INSN_LDBFU, FRVBF_INSN_LDHFU, FRVBF_INSN_LDFU, FRVBF_INSN_LDCU
 , FRVBF_INSN_NLDBFU, FRVBF_INSN_NLDHFU, FRVBF_INSN_NLDFU, FRVBF_INSN_LDDU
 , FRVBF_INSN_NLDDU, FRVBF_INSN_LDDFU, FRVBF_INSN_LDDCU, FRVBF_INSN_NLDDFU
 , FRVBF_INSN_LDQU, FRVBF_INSN_NLDQU, FRVBF_INSN_LDQFU, FRVBF_INSN_LDQCU
 , FRVBF_INSN_NLDQFU, FRVBF_INSN_LDSBI, FRVBF_INSN_LDSHI, FRVBF_INSN_LDI
 , FRVBF_INSN_LDUBI, FRVBF_INSN_LDUHI, FRVBF_INSN_LDBFI, FRVBF_INSN_LDHFI
 , FRVBF_INSN_LDFI, FRVBF_INSN_NLDSBI, FRVBF_INSN_NLDUBI, FRVBF_INSN_NLDSHI
 , FRVBF_INSN_NLDUHI, FRVBF_INSN_NLDI, FRVBF_INSN_NLDBFI, FRVBF_INSN_NLDHFI
 , FRVBF_INSN_NLDFI, FRVBF_INSN_LDDI, FRVBF_INSN_LDDFI, FRVBF_INSN_NLDDI
 , FRVBF_INSN_NLDDFI, FRVBF_INSN_LDQI, FRVBF_INSN_LDQFI, FRVBF_INSN_NLDQFI
 , FRVBF_INSN_STB, FRVBF_INSN_STH, FRVBF_INSN_ST, FRVBF_INSN_STBF
 , FRVBF_INSN_STHF, FRVBF_INSN_STF, FRVBF_INSN_STC, FRVBF_INSN_STD
 , FRVBF_INSN_STDF, FRVBF_INSN_STDC, FRVBF_INSN_STQ, FRVBF_INSN_STQF
 , FRVBF_INSN_STQC, FRVBF_INSN_STBU, FRVBF_INSN_STHU, FRVBF_INSN_STU
 , FRVBF_INSN_STBFU, FRVBF_INSN_STHFU, FRVBF_INSN_STFU, FRVBF_INSN_STCU
 , FRVBF_INSN_STDU, FRVBF_INSN_STDFU, FRVBF_INSN_STDCU, FRVBF_INSN_STQU
 , FRVBF_INSN_STQFU, FRVBF_INSN_STQCU, FRVBF_INSN_CLDSB, FRVBF_INSN_CLDUB
 , FRVBF_INSN_CLDSH, FRVBF_INSN_CLDUH, FRVBF_INSN_CLD, FRVBF_INSN_CLDBF
 , FRVBF_INSN_CLDHF, FRVBF_INSN_CLDF, FRVBF_INSN_CLDD, FRVBF_INSN_CLDDF
 , FRVBF_INSN_CLDQ, FRVBF_INSN_CLDSBU, FRVBF_INSN_CLDUBU, FRVBF_INSN_CLDSHU
 , FRVBF_INSN_CLDUHU, FRVBF_INSN_CLDU, FRVBF_INSN_CLDBFU, FRVBF_INSN_CLDHFU
 , FRVBF_INSN_CLDFU, FRVBF_INSN_CLDDU, FRVBF_INSN_CLDDFU, FRVBF_INSN_CLDQU
 , FRVBF_INSN_CSTB, FRVBF_INSN_CSTH, FRVBF_INSN_CST, FRVBF_INSN_CSTBF
 , FRVBF_INSN_CSTHF, FRVBF_INSN_CSTF, FRVBF_INSN_CSTD, FRVBF_INSN_CSTDF
 , FRVBF_INSN_CSTQ, FRVBF_INSN_CSTBU, FRVBF_INSN_CSTHU, FRVBF_INSN_CSTU
 , FRVBF_INSN_CSTBFU, FRVBF_INSN_CSTHFU, FRVBF_INSN_CSTFU, FRVBF_INSN_CSTDU
 , FRVBF_INSN_CSTDFU, FRVBF_INSN_STBI, FRVBF_INSN_STHI, FRVBF_INSN_STI
 , FRVBF_INSN_STBFI, FRVBF_INSN_STHFI, FRVBF_INSN_STFI, FRVBF_INSN_STDI
 , FRVBF_INSN_STDFI, FRVBF_INSN_STQI, FRVBF_INSN_STQFI, FRVBF_INSN_SWAP
 , FRVBF_INSN_SWAPI, FRVBF_INSN_CSWAP, FRVBF_INSN_MOVGF, FRVBF_INSN_MOVFG
 , FRVBF_INSN_MOVGFD, FRVBF_INSN_MOVFGD, FRVBF_INSN_MOVGFQ, FRVBF_INSN_MOVFGQ
 , FRVBF_INSN_CMOVGF, FRVBF_INSN_CMOVFG, FRVBF_INSN_CMOVGFD, FRVBF_INSN_CMOVFGD
 , FRVBF_INSN_MOVGS, FRVBF_INSN_MOVSG, FRVBF_INSN_BRA, FRVBF_INSN_BNO
 , FRVBF_INSN_BEQ, FRVBF_INSN_BNE, FRVBF_INSN_BLE, FRVBF_INSN_BGT
 , FRVBF_INSN_BLT, FRVBF_INSN_BGE, FRVBF_INSN_BLS, FRVBF_INSN_BHI
 , FRVBF_INSN_BC, FRVBF_INSN_BNC, FRVBF_INSN_BN, FRVBF_INSN_BP
 , FRVBF_INSN_BV, FRVBF_INSN_BNV, FRVBF_INSN_FBRA, FRVBF_INSN_FBNO
 , FRVBF_INSN_FBNE, FRVBF_INSN_FBEQ, FRVBF_INSN_FBLG, FRVBF_INSN_FBUE
 , FRVBF_INSN_FBUL, FRVBF_INSN_FBGE, FRVBF_INSN_FBLT, FRVBF_INSN_FBUGE
 , FRVBF_INSN_FBUG, FRVBF_INSN_FBLE, FRVBF_INSN_FBGT, FRVBF_INSN_FBULE
 , FRVBF_INSN_FBU, FRVBF_INSN_FBO, FRVBF_INSN_BCTRLR, FRVBF_INSN_BRALR
 , FRVBF_INSN_BNOLR, FRVBF_INSN_BEQLR, FRVBF_INSN_BNELR, FRVBF_INSN_BLELR
 , FRVBF_INSN_BGTLR, FRVBF_INSN_BLTLR, FRVBF_INSN_BGELR, FRVBF_INSN_BLSLR
 , FRVBF_INSN_BHILR, FRVBF_INSN_BCLR, FRVBF_INSN_BNCLR, FRVBF_INSN_BNLR
 , FRVBF_INSN_BPLR, FRVBF_INSN_BVLR, FRVBF_INSN_BNVLR, FRVBF_INSN_FBRALR
 , FRVBF_INSN_FBNOLR, FRVBF_INSN_FBEQLR, FRVBF_INSN_FBNELR, FRVBF_INSN_FBLGLR
 , FRVBF_INSN_FBUELR, FRVBF_INSN_FBULLR, FRVBF_INSN_FBGELR, FRVBF_INSN_FBLTLR
 , FRVBF_INSN_FBUGELR, FRVBF_INSN_FBUGLR, FRVBF_INSN_FBLELR, FRVBF_INSN_FBGTLR
 , FRVBF_INSN_FBULELR, FRVBF_INSN_FBULR, FRVBF_INSN_FBOLR, FRVBF_INSN_BCRALR
 , FRVBF_INSN_BCNOLR, FRVBF_INSN_BCEQLR, FRVBF_INSN_BCNELR, FRVBF_INSN_BCLELR
 , FRVBF_INSN_BCGTLR, FRVBF_INSN_BCLTLR, FRVBF_INSN_BCGELR, FRVBF_INSN_BCLSLR
 , FRVBF_INSN_BCHILR, FRVBF_INSN_BCCLR, FRVBF_INSN_BCNCLR, FRVBF_INSN_BCNLR
 , FRVBF_INSN_BCPLR, FRVBF_INSN_BCVLR, FRVBF_INSN_BCNVLR, FRVBF_INSN_FCBRALR
 , FRVBF_INSN_FCBNOLR, FRVBF_INSN_FCBEQLR, FRVBF_INSN_FCBNELR, FRVBF_INSN_FCBLGLR
 , FRVBF_INSN_FCBUELR, FRVBF_INSN_FCBULLR, FRVBF_INSN_FCBGELR, FRVBF_INSN_FCBLTLR
 , FRVBF_INSN_FCBUGELR, FRVBF_INSN_FCBUGLR, FRVBF_INSN_FCBLELR, FRVBF_INSN_FCBGTLR
 , FRVBF_INSN_FCBULELR, FRVBF_INSN_FCBULR, FRVBF_INSN_FCBOLR, FRVBF_INSN_JMPL
 , FRVBF_INSN_CALLL, FRVBF_INSN_JMPIL, FRVBF_INSN_CALLIL, FRVBF_INSN_CALL
 , FRVBF_INSN_RETT, FRVBF_INSN_REI, FRVBF_INSN_TRA, FRVBF_INSN_TNO
 , FRVBF_INSN_TEQ, FRVBF_INSN_TNE, FRVBF_INSN_TLE, FRVBF_INSN_TGT
 , FRVBF_INSN_TLT, FRVBF_INSN_TGE, FRVBF_INSN_TLS, FRVBF_INSN_THI
 , FRVBF_INSN_TC, FRVBF_INSN_TNC, FRVBF_INSN_TN, FRVBF_INSN_TP
 , FRVBF_INSN_TV, FRVBF_INSN_TNV, FRVBF_INSN_FTRA, FRVBF_INSN_FTNO
 , FRVBF_INSN_FTNE, FRVBF_INSN_FTEQ, FRVBF_INSN_FTLG, FRVBF_INSN_FTUE
 , FRVBF_INSN_FTUL, FRVBF_INSN_FTGE, FRVBF_INSN_FTLT, FRVBF_INSN_FTUGE
 , FRVBF_INSN_FTUG, FRVBF_INSN_FTLE, FRVBF_INSN_FTGT, FRVBF_INSN_FTULE
 , FRVBF_INSN_FTU, FRVBF_INSN_FTO, FRVBF_INSN_TIRA, FRVBF_INSN_TINO
 , FRVBF_INSN_TIEQ, FRVBF_INSN_TINE, FRVBF_INSN_TILE, FRVBF_INSN_TIGT
 , FRVBF_INSN_TILT, FRVBF_INSN_TIGE, FRVBF_INSN_TILS, FRVBF_INSN_TIHI
 , FRVBF_INSN_TIC, FRVBF_INSN_TINC, FRVBF_INSN_TIN, FRVBF_INSN_TIP
 , FRVBF_INSN_TIV, FRVBF_INSN_TINV, FRVBF_INSN_FTIRA, FRVBF_INSN_FTINO
 , FRVBF_INSN_FTINE, FRVBF_INSN_FTIEQ, FRVBF_INSN_FTILG, FRVBF_INSN_FTIUE
 , FRVBF_INSN_FTIUL, FRVBF_INSN_FTIGE, FRVBF_INSN_FTILT, FRVBF_INSN_FTIUGE
 , FRVBF_INSN_FTIUG, FRVBF_INSN_FTILE, FRVBF_INSN_FTIGT, FRVBF_INSN_FTIULE
 , FRVBF_INSN_FTIU, FRVBF_INSN_FTIO, FRVBF_INSN_BREAK, FRVBF_INSN_MTRAP
 , FRVBF_INSN_ANDCR, FRVBF_INSN_ORCR, FRVBF_INSN_XORCR, FRVBF_INSN_NANDCR
 , FRVBF_INSN_NORCR, FRVBF_INSN_ANDNCR, FRVBF_INSN_ORNCR, FRVBF_INSN_NANDNCR
 , FRVBF_INSN_NORNCR, FRVBF_INSN_NOTCR, FRVBF_INSN_CKRA, FRVBF_INSN_CKNO
 , FRVBF_INSN_CKEQ, FRVBF_INSN_CKNE, FRVBF_INSN_CKLE, FRVBF_INSN_CKGT
 , FRVBF_INSN_CKLT, FRVBF_INSN_CKGE, FRVBF_INSN_CKLS, FRVBF_INSN_CKHI
 , FRVBF_INSN_CKC, FRVBF_INSN_CKNC, FRVBF_INSN_CKN, FRVBF_INSN_CKP
 , FRVBF_INSN_CKV, FRVBF_INSN_CKNV, FRVBF_INSN_FCKRA, FRVBF_INSN_FCKNO
 , FRVBF_INSN_FCKNE, FRVBF_INSN_FCKEQ, FRVBF_INSN_FCKLG, FRVBF_INSN_FCKUE
 , FRVBF_INSN_FCKUL, FRVBF_INSN_FCKGE, FRVBF_INSN_FCKLT, FRVBF_INSN_FCKUGE
 , FRVBF_INSN_FCKUG, FRVBF_INSN_FCKLE, FRVBF_INSN_FCKGT, FRVBF_INSN_FCKULE
 , FRVBF_INSN_FCKU, FRVBF_INSN_FCKO, FRVBF_INSN_CCKRA, FRVBF_INSN_CCKNO
 , FRVBF_INSN_CCKEQ, FRVBF_INSN_CCKNE, FRVBF_INSN_CCKLE, FRVBF_INSN_CCKGT
 , FRVBF_INSN_CCKLT, FRVBF_INSN_CCKGE, FRVBF_INSN_CCKLS, FRVBF_INSN_CCKHI
 , FRVBF_INSN_CCKC, FRVBF_INSN_CCKNC, FRVBF_INSN_CCKN, FRVBF_INSN_CCKP
 , FRVBF_INSN_CCKV, FRVBF_INSN_CCKNV, FRVBF_INSN_CFCKRA, FRVBF_INSN_CFCKNO
 , FRVBF_INSN_CFCKNE, FRVBF_INSN_CFCKEQ, FRVBF_INSN_CFCKLG, FRVBF_INSN_CFCKUE
 , FRVBF_INSN_CFCKUL, FRVBF_INSN_CFCKGE, FRVBF_INSN_CFCKLT, FRVBF_INSN_CFCKUGE
 , FRVBF_INSN_CFCKUG, FRVBF_INSN_CFCKLE, FRVBF_INSN_CFCKGT, FRVBF_INSN_CFCKULE
 , FRVBF_INSN_CFCKU, FRVBF_INSN_CFCKO, FRVBF_INSN_CJMPL, FRVBF_INSN_CCALLL
 , FRVBF_INSN_ICI, FRVBF_INSN_DCI, FRVBF_INSN_ICEI, FRVBF_INSN_DCEI
 , FRVBF_INSN_DCF, FRVBF_INSN_DCEF, FRVBF_INSN_WITLB, FRVBF_INSN_WDTLB
 , FRVBF_INSN_ITLBI, FRVBF_INSN_DTLBI, FRVBF_INSN_ICPL, FRVBF_INSN_DCPL
 , FRVBF_INSN_ICUL, FRVBF_INSN_DCUL, FRVBF_INSN_BAR, FRVBF_INSN_MEMBAR
 , FRVBF_INSN_LRAI, FRVBF_INSN_LRAD, FRVBF_INSN_TLBPR, FRVBF_INSN_COP1
 , FRVBF_INSN_COP2, FRVBF_INSN_CLRGR, FRVBF_INSN_CLRFR, FRVBF_INSN_CLRGA
 , FRVBF_INSN_CLRFA, FRVBF_INSN_COMMITGR, FRVBF_INSN_COMMITFR, FRVBF_INSN_COMMITGA
 , FRVBF_INSN_COMMITFA, FRVBF_INSN_FITOS, FRVBF_INSN_FSTOI, FRVBF_INSN_FITOD
 , FRVBF_INSN_FDTOI, FRVBF_INSN_FDITOS, FRVBF_INSN_FDSTOI, FRVBF_INSN_NFDITOS
 , FRVBF_INSN_NFDSTOI, FRVBF_INSN_CFITOS, FRVBF_INSN_CFSTOI, FRVBF_INSN_NFITOS
 , FRVBF_INSN_NFSTOI, FRVBF_INSN_FMOVS, FRVBF_INSN_FMOVD, FRVBF_INSN_FDMOVS
 , FRVBF_INSN_CFMOVS, FRVBF_INSN_FNEGS, FRVBF_INSN_FNEGD, FRVBF_INSN_FDNEGS
 , FRVBF_INSN_CFNEGS, FRVBF_INSN_FABSS, FRVBF_INSN_FABSD, FRVBF_INSN_FDABSS
 , FRVBF_INSN_CFABSS, FRVBF_INSN_FSQRTS, FRVBF_INSN_FDSQRTS, FRVBF_INSN_NFDSQRTS
 , FRVBF_INSN_FSQRTD, FRVBF_INSN_CFSQRTS, FRVBF_INSN_NFSQRTS, FRVBF_INSN_FADDS
 , FRVBF_INSN_FSUBS, FRVBF_INSN_FMULS, FRVBF_INSN_FDIVS, FRVBF_INSN_FADDD
 , FRVBF_INSN_FSUBD, FRVBF_INSN_FMULD, FRVBF_INSN_FDIVD, FRVBF_INSN_CFADDS
 , FRVBF_INSN_CFSUBS, FRVBF_INSN_CFMULS, FRVBF_INSN_CFDIVS, FRVBF_INSN_NFADDS
 , FRVBF_INSN_NFSUBS, FRVBF_INSN_NFMULS, FRVBF_INSN_NFDIVS, FRVBF_INSN_FCMPS
 , FRVBF_INSN_FCMPD, FRVBF_INSN_CFCMPS, FRVBF_INSN_FDCMPS, FRVBF_INSN_FMADDS
 , FRVBF_INSN_FMSUBS, FRVBF_INSN_FMADDD, FRVBF_INSN_FMSUBD, FRVBF_INSN_FDMADDS
 , FRVBF_INSN_NFDMADDS, FRVBF_INSN_CFMADDS, FRVBF_INSN_CFMSUBS, FRVBF_INSN_NFMADDS
 , FRVBF_INSN_NFMSUBS, FRVBF_INSN_FMAS, FRVBF_INSN_FMSS, FRVBF_INSN_FDMAS
 , FRVBF_INSN_FDMSS, FRVBF_INSN_NFDMAS, FRVBF_INSN_NFDMSS, FRVBF_INSN_CFMAS
 , FRVBF_INSN_CFMSS, FRVBF_INSN_FMAD, FRVBF_INSN_FMSD, FRVBF_INSN_NFMAS
 , FRVBF_INSN_NFMSS, FRVBF_INSN_FDADDS, FRVBF_INSN_FDSUBS, FRVBF_INSN_FDMULS
 , FRVBF_INSN_FDDIVS, FRVBF_INSN_FDSADS, FRVBF_INSN_FDMULCS, FRVBF_INSN_NFDMULCS
 , FRVBF_INSN_NFDADDS, FRVBF_INSN_NFDSUBS, FRVBF_INSN_NFDMULS, FRVBF_INSN_NFDDIVS
 , FRVBF_INSN_NFDSADS, FRVBF_INSN_NFDCMPS, FRVBF_INSN_MHSETLOS, FRVBF_INSN_MHSETHIS
 , FRVBF_INSN_MHDSETS, FRVBF_INSN_MHSETLOH, FRVBF_INSN_MHSETHIH, FRVBF_INSN_MHDSETH
 , FRVBF_INSN_MAND, FRVBF_INSN_MOR, FRVBF_INSN_MXOR, FRVBF_INSN_CMAND
 , FRVBF_INSN_CMOR, FRVBF_INSN_CMXOR, FRVBF_INSN_MNOT, FRVBF_INSN_CMNOT
 , FRVBF_INSN_MROTLI, FRVBF_INSN_MROTRI, FRVBF_INSN_MWCUT, FRVBF_INSN_MWCUTI
 , FRVBF_INSN_MCUT, FRVBF_INSN_MCUTI, FRVBF_INSN_MCUTSS, FRVBF_INSN_MCUTSSI
 , FRVBF_INSN_MDCUTSSI, FRVBF_INSN_MAVEH, FRVBF_INSN_MSLLHI, FRVBF_INSN_MSRLHI
 , FRVBF_INSN_MSRAHI, FRVBF_INSN_MDROTLI, FRVBF_INSN_MCPLHI, FRVBF_INSN_MCPLI
 , FRVBF_INSN_MSATHS, FRVBF_INSN_MQSATHS, FRVBF_INSN_MSATHU, FRVBF_INSN_MCMPSH
 , FRVBF_INSN_MCMPUH, FRVBF_INSN_MABSHS, FRVBF_INSN_MADDHSS, FRVBF_INSN_MADDHUS
 , FRVBF_INSN_MSUBHSS, FRVBF_INSN_MSUBHUS, FRVBF_INSN_CMADDHSS, FRVBF_INSN_CMADDHUS
 , FRVBF_INSN_CMSUBHSS, FRVBF_INSN_CMSUBHUS, FRVBF_INSN_MQADDHSS, FRVBF_INSN_MQADDHUS
 , FRVBF_INSN_MQSUBHSS, FRVBF_INSN_MQSUBHUS, FRVBF_INSN_CMQADDHSS, FRVBF_INSN_CMQADDHUS
 , FRVBF_INSN_CMQSUBHSS, FRVBF_INSN_CMQSUBHUS, FRVBF_INSN_MQLCLRHS, FRVBF_INSN_MQLMTHS
 , FRVBF_INSN_MQSLLHI, FRVBF_INSN_MQSRAHI, FRVBF_INSN_MADDACCS, FRVBF_INSN_MSUBACCS
 , FRVBF_INSN_MDADDACCS, FRVBF_INSN_MDSUBACCS, FRVBF_INSN_MASACCS, FRVBF_INSN_MDASACCS
 , FRVBF_INSN_MMULHS, FRVBF_INSN_MMULHU, FRVBF_INSN_MMULXHS, FRVBF_INSN_MMULXHU
 , FRVBF_INSN_CMMULHS, FRVBF_INSN_CMMULHU, FRVBF_INSN_MQMULHS, FRVBF_INSN_MQMULHU
 , FRVBF_INSN_MQMULXHS, FRVBF_INSN_MQMULXHU, FRVBF_INSN_CMQMULHS, FRVBF_INSN_CMQMULHU
 , FRVBF_INSN_MMACHS, FRVBF_INSN_MMACHU, FRVBF_INSN_MMRDHS, FRVBF_INSN_MMRDHU
 , FRVBF_INSN_CMMACHS, FRVBF_INSN_CMMACHU, FRVBF_INSN_MQMACHS, FRVBF_INSN_MQMACHU
 , FRVBF_INSN_CMQMACHS, FRVBF_INSN_CMQMACHU, FRVBF_INSN_MQXMACHS, FRVBF_INSN_MQXMACXHS
 , FRVBF_INSN_MQMACXHS, FRVBF_INSN_MCPXRS, FRVBF_INSN_MCPXRU, FRVBF_INSN_MCPXIS
 , FRVBF_INSN_MCPXIU, FRVBF_INSN_CMCPXRS, FRVBF_INSN_CMCPXRU, FRVBF_INSN_CMCPXIS
 , FRVBF_INSN_CMCPXIU, FRVBF_INSN_MQCPXRS, FRVBF_INSN_MQCPXRU, FRVBF_INSN_MQCPXIS
 , FRVBF_INSN_MQCPXIU, FRVBF_INSN_MEXPDHW, FRVBF_INSN_CMEXPDHW, FRVBF_INSN_MEXPDHD
 , FRVBF_INSN_CMEXPDHD, FRVBF_INSN_MPACKH, FRVBF_INSN_MDPACKH, FRVBF_INSN_MUNPACKH
 , FRVBF_INSN_MDUNPACKH, FRVBF_INSN_MBTOH, FRVBF_INSN_CMBTOH, FRVBF_INSN_MHTOB
 , FRVBF_INSN_CMHTOB, FRVBF_INSN_MBTOHE, FRVBF_INSN_CMBTOHE, FRVBF_INSN_MNOP
 , FRVBF_INSN_MCLRACC_0, FRVBF_INSN_MCLRACC_1, FRVBF_INSN_MRDACC, FRVBF_INSN_MRDACCG
 , FRVBF_INSN_MWTACC, FRVBF_INSN_MWTACCG, FRVBF_INSN_MCOP1, FRVBF_INSN_MCOP2
 , FRVBF_INSN_FNOP, FRVBF_INSN__MAX
} FRVBF_INSN_TYPE;

/* Enum declaration for semantic formats in cpu family frvbf.  */
typedef enum frvbf_sfmt_type {
  FRVBF_SFMT_EMPTY, FRVBF_SFMT_ADD, FRVBF_SFMT_NOT, FRVBF_SFMT_SDIV
 , FRVBF_SFMT_SMUL, FRVBF_SFMT_SMU, FRVBF_SFMT_SMASS, FRVBF_SFMT_SCUTSS
 , FRVBF_SFMT_CADD, FRVBF_SFMT_CNOT, FRVBF_SFMT_CSMUL, FRVBF_SFMT_CSDIV
 , FRVBF_SFMT_ADDCC, FRVBF_SFMT_ANDCC, FRVBF_SFMT_SMULCC, FRVBF_SFMT_CADDCC
 , FRVBF_SFMT_CSMULCC, FRVBF_SFMT_ADDX, FRVBF_SFMT_ADDI, FRVBF_SFMT_SDIVI
 , FRVBF_SFMT_SMULI, FRVBF_SFMT_ADDICC, FRVBF_SFMT_ANDICC, FRVBF_SFMT_SMULICC
 , FRVBF_SFMT_ADDXI, FRVBF_SFMT_CMPB, FRVBF_SFMT_SETLO, FRVBF_SFMT_SETHI
 , FRVBF_SFMT_SETLOS, FRVBF_SFMT_LDSB, FRVBF_SFMT_LDBF, FRVBF_SFMT_LDC
 , FRVBF_SFMT_NLDSB, FRVBF_SFMT_NLDBF, FRVBF_SFMT_LDD, FRVBF_SFMT_LDDF
 , FRVBF_SFMT_LDDC, FRVBF_SFMT_NLDD, FRVBF_SFMT_NLDDF, FRVBF_SFMT_LDQ
 , FRVBF_SFMT_LDQF, FRVBF_SFMT_LDQC, FRVBF_SFMT_NLDQ, FRVBF_SFMT_NLDQF
 , FRVBF_SFMT_LDSBU, FRVBF_SFMT_NLDSBU, FRVBF_SFMT_LDBFU, FRVBF_SFMT_LDCU
 , FRVBF_SFMT_NLDBFU, FRVBF_SFMT_LDDU, FRVBF_SFMT_NLDDU, FRVBF_SFMT_LDDFU
 , FRVBF_SFMT_LDDCU, FRVBF_SFMT_NLDDFU, FRVBF_SFMT_LDQU, FRVBF_SFMT_NLDQU
 , FRVBF_SFMT_LDQFU, FRVBF_SFMT_LDQCU, FRVBF_SFMT_NLDQFU, FRVBF_SFMT_LDSBI
 , FRVBF_SFMT_LDBFI, FRVBF_SFMT_NLDSBI, FRVBF_SFMT_NLDBFI, FRVBF_SFMT_LDDI
 , FRVBF_SFMT_LDDFI, FRVBF_SFMT_NLDDI, FRVBF_SFMT_NLDDFI, FRVBF_SFMT_LDQI
 , FRVBF_SFMT_LDQFI, FRVBF_SFMT_NLDQFI, FRVBF_SFMT_STB, FRVBF_SFMT_STBF
 , FRVBF_SFMT_STC, FRVBF_SFMT_STD, FRVBF_SFMT_STDF, FRVBF_SFMT_STDC
 , FRVBF_SFMT_STBU, FRVBF_SFMT_STBFU, FRVBF_SFMT_STCU, FRVBF_SFMT_STDU
 , FRVBF_SFMT_STDFU, FRVBF_SFMT_STDCU, FRVBF_SFMT_STQU, FRVBF_SFMT_CLDSB
 , FRVBF_SFMT_CLDBF, FRVBF_SFMT_CLDD, FRVBF_SFMT_CLDDF, FRVBF_SFMT_CLDQ
 , FRVBF_SFMT_CLDSBU, FRVBF_SFMT_CLDBFU, FRVBF_SFMT_CLDDU, FRVBF_SFMT_CLDDFU
 , FRVBF_SFMT_CLDQU, FRVBF_SFMT_CSTB, FRVBF_SFMT_CSTBF, FRVBF_SFMT_CSTD
 , FRVBF_SFMT_CSTDF, FRVBF_SFMT_CSTBU, FRVBF_SFMT_CSTBFU, FRVBF_SFMT_CSTDU
 , FRVBF_SFMT_CSTDFU, FRVBF_SFMT_STBI, FRVBF_SFMT_STBFI, FRVBF_SFMT_STDI
 , FRVBF_SFMT_STDFI, FRVBF_SFMT_SWAP, FRVBF_SFMT_SWAPI, FRVBF_SFMT_CSWAP
 , FRVBF_SFMT_MOVGF, FRVBF_SFMT_MOVFG, FRVBF_SFMT_MOVGFD, FRVBF_SFMT_MOVFGD
 , FRVBF_SFMT_MOVGFQ, FRVBF_SFMT_MOVFGQ, FRVBF_SFMT_CMOVGF, FRVBF_SFMT_CMOVFG
 , FRVBF_SFMT_CMOVGFD, FRVBF_SFMT_CMOVFGD, FRVBF_SFMT_MOVGS, FRVBF_SFMT_MOVSG
 , FRVBF_SFMT_BRA, FRVBF_SFMT_BNO, FRVBF_SFMT_BEQ, FRVBF_SFMT_FBNE
 , FRVBF_SFMT_BCTRLR, FRVBF_SFMT_BRALR, FRVBF_SFMT_BNOLR, FRVBF_SFMT_BEQLR
 , FRVBF_SFMT_FBEQLR, FRVBF_SFMT_BCRALR, FRVBF_SFMT_BCNOLR, FRVBF_SFMT_BCEQLR
 , FRVBF_SFMT_FCBEQLR, FRVBF_SFMT_JMPL, FRVBF_SFMT_JMPIL, FRVBF_SFMT_CALL
 , FRVBF_SFMT_RETT, FRVBF_SFMT_REI, FRVBF_SFMT_TRA, FRVBF_SFMT_TEQ
 , FRVBF_SFMT_FTNE, FRVBF_SFMT_TIRA, FRVBF_SFMT_TIEQ, FRVBF_SFMT_FTINE
 , FRVBF_SFMT_BREAK, FRVBF_SFMT_ANDCR, FRVBF_SFMT_NOTCR, FRVBF_SFMT_CKRA
 , FRVBF_SFMT_CKEQ, FRVBF_SFMT_FCKRA, FRVBF_SFMT_FCKNE, FRVBF_SFMT_CCKRA
 , FRVBF_SFMT_CCKEQ, FRVBF_SFMT_CFCKRA, FRVBF_SFMT_CFCKNE, FRVBF_SFMT_CJMPL
 , FRVBF_SFMT_ICI, FRVBF_SFMT_ICEI, FRVBF_SFMT_ICPL, FRVBF_SFMT_ICUL
 , FRVBF_SFMT_CLRGR, FRVBF_SFMT_CLRFR, FRVBF_SFMT_COMMITGR, FRVBF_SFMT_COMMITFR
 , FRVBF_SFMT_FITOS, FRVBF_SFMT_FSTOI, FRVBF_SFMT_FITOD, FRVBF_SFMT_FDTOI
 , FRVBF_SFMT_FDITOS, FRVBF_SFMT_FDSTOI, FRVBF_SFMT_CFITOS, FRVBF_SFMT_CFSTOI
 , FRVBF_SFMT_NFITOS, FRVBF_SFMT_NFSTOI, FRVBF_SFMT_FMOVS, FRVBF_SFMT_FMOVD
 , FRVBF_SFMT_FDMOVS, FRVBF_SFMT_CFMOVS, FRVBF_SFMT_NFSQRTS, FRVBF_SFMT_FADDS
 , FRVBF_SFMT_FADDD, FRVBF_SFMT_CFADDS, FRVBF_SFMT_NFADDS, FRVBF_SFMT_FCMPS
 , FRVBF_SFMT_FCMPD, FRVBF_SFMT_CFCMPS, FRVBF_SFMT_FDCMPS, FRVBF_SFMT_FMADDS
 , FRVBF_SFMT_FMADDD, FRVBF_SFMT_FDMADDS, FRVBF_SFMT_CFMADDS, FRVBF_SFMT_NFMADDS
 , FRVBF_SFMT_FMAS, FRVBF_SFMT_FDMAS, FRVBF_SFMT_CFMAS, FRVBF_SFMT_NFDCMPS
 , FRVBF_SFMT_MHSETLOS, FRVBF_SFMT_MHSETHIS, FRVBF_SFMT_MHDSETS, FRVBF_SFMT_MHSETLOH
 , FRVBF_SFMT_MHSETHIH, FRVBF_SFMT_MHDSETH, FRVBF_SFMT_MAND, FRVBF_SFMT_CMAND
 , FRVBF_SFMT_MNOT, FRVBF_SFMT_CMNOT, FRVBF_SFMT_MROTLI, FRVBF_SFMT_MWCUT
 , FRVBF_SFMT_MWCUTI, FRVBF_SFMT_MCUT, FRVBF_SFMT_MCUTI, FRVBF_SFMT_MDCUTSSI
 , FRVBF_SFMT_MSLLHI, FRVBF_SFMT_MDROTLI, FRVBF_SFMT_MCPLHI, FRVBF_SFMT_MCPLI
 , FRVBF_SFMT_MSATHS, FRVBF_SFMT_MQSATHS, FRVBF_SFMT_MCMPSH, FRVBF_SFMT_MABSHS
 , FRVBF_SFMT_CMADDHSS, FRVBF_SFMT_CMQADDHSS, FRVBF_SFMT_MQSLLHI, FRVBF_SFMT_MADDACCS
 , FRVBF_SFMT_MDADDACCS, FRVBF_SFMT_MASACCS, FRVBF_SFMT_MDASACCS, FRVBF_SFMT_MMULHS
 , FRVBF_SFMT_CMMULHS, FRVBF_SFMT_MQMULHS, FRVBF_SFMT_CMQMULHS, FRVBF_SFMT_MMACHS
 , FRVBF_SFMT_MMACHU, FRVBF_SFMT_CMMACHS, FRVBF_SFMT_CMMACHU, FRVBF_SFMT_MQMACHS
 , FRVBF_SFMT_MQMACHU, FRVBF_SFMT_CMQMACHS, FRVBF_SFMT_CMQMACHU, FRVBF_SFMT_MCPXRS
 , FRVBF_SFMT_CMCPXRS, FRVBF_SFMT_MQCPXRS, FRVBF_SFMT_MEXPDHW, FRVBF_SFMT_CMEXPDHW
 , FRVBF_SFMT_MEXPDHD, FRVBF_SFMT_CMEXPDHD, FRVBF_SFMT_MPACKH, FRVBF_SFMT_MDPACKH
 , FRVBF_SFMT_MUNPACKH, FRVBF_SFMT_MDUNPACKH, FRVBF_SFMT_MBTOH, FRVBF_SFMT_CMBTOH
 , FRVBF_SFMT_MHTOB, FRVBF_SFMT_CMHTOB, FRVBF_SFMT_MBTOHE, FRVBF_SFMT_CMBTOHE
 , FRVBF_SFMT_MCLRACC_0, FRVBF_SFMT_MRDACC, FRVBF_SFMT_MRDACCG, FRVBF_SFMT_MWTACC
 , FRVBF_SFMT_MWTACCG
} FRVBF_SFMT_TYPE;

/* Function unit handlers (user written).  */

extern int frvbf_model_frv_u_exec (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int frvbf_model_fr550_u_media_4_quad (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRintieven*/, INT /*FRintjeven*/, INT /*ACC40Sk*/, INT /*ACC40Uk*/);
extern int frvbf_model_fr550_u_media_4_add_sub_dual (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ACC40Si*/, INT /*ACC40Sk*/);
extern int frvbf_model_fr550_u_media_4_add_sub (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ACC40Si*/, INT /*ACC40Sk*/);
extern int frvbf_model_fr550_u_media_4_acc_dual (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ACC40Si*/, INT /*ACC40Sk*/);
extern int frvbf_model_fr550_u_media_4_acc (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ACC40Si*/, INT /*ACC40Sk*/);
extern int frvbf_model_fr550_u_media_4 (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*ACC40Sk*/, INT /*ACC40Uk*/);
extern int frvbf_model_fr550_u_media_set (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRintk*/);
extern int frvbf_model_fr550_u_media_3_mclracc (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int frvbf_model_fr550_u_media_3_wtacc (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*ACC40Sk*/);
extern int frvbf_model_fr550_u_media_3_acc_dual (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ACC40Si*/, INT /*FRintkeven*/);
extern int frvbf_model_fr550_u_media_3_acc (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRintj*/, INT /*ACC40Si*/, INT /*FRintk*/);
extern int frvbf_model_fr550_u_media_3_dual (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintk*/);
extern int frvbf_model_fr550_u_media_dual_expand (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintkeven*/);
extern int frvbf_model_fr550_u_media_quad (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRintieven*/, INT /*FRintjeven*/, INT /*FRintkeven*/);
extern int frvbf_model_fr550_u_media (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*FRintk*/);
extern int frvbf_model_fr550_u_float_convert (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRj*/, INT /*FRintj*/, INT /*FRdoublej*/, INT /*FRk*/, INT /*FRintk*/, INT /*FRdoublek*/);
extern int frvbf_model_fr550_u_commit (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRk*/, INT /*FRk*/);
extern int frvbf_model_fr550_u_dcul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr550_u_icul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr550_u_dcpl (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr550_u_icpl (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr550_u_dcf (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr550_u_dci (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr550_u_ici (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr550_u_clrfr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRk*/);
extern int frvbf_model_fr550_u_clrgr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRk*/);
extern int frvbf_model_fr550_u_fr2fr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRi*/, INT /*FRk*/);
extern int frvbf_model_fr550_u_swap (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/);
extern int frvbf_model_fr550_u_fr_store (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*FRintk*/, INT /*FRdoublek*/);
extern int frvbf_model_fr550_u_fr_load (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*FRintk*/, INT /*FRdoublek*/);
extern int frvbf_model_fr550_u_gr_store (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/, INT /*GRdoublek*/);
extern int frvbf_model_fr550_u_gr_load (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/, INT /*GRdoublek*/);
extern int frvbf_model_fr550_u_set_hilo (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRkhi*/, INT /*GRklo*/);
extern int frvbf_model_fr550_u_gr2spr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRj*/, INT /*spr*/);
extern int frvbf_model_fr550_u_spr2gr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*spr*/, INT /*GRj*/);
extern int frvbf_model_fr550_u_gr2fr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRj*/, INT /*FRintk*/);
extern int frvbf_model_fr550_u_fr2gr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRintk*/, INT /*GRj*/);
extern int frvbf_model_fr550_u_float_dual_compare (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRi*/, INT /*FRj*/, INT /*FCCi_2*/);
extern int frvbf_model_fr550_u_float_compare (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRi*/, INT /*FRj*/, INT /*FRdoublei*/, INT /*FRdoublej*/, INT /*FCCi_2*/);
extern int frvbf_model_fr550_u_float_sqrt (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRj*/, INT /*FRdoublej*/, INT /*FRk*/, INT /*FRdoublek*/);
extern int frvbf_model_fr550_u_float_div (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRi*/, INT /*FRj*/, INT /*FRk*/);
extern int frvbf_model_fr550_u_float_dual_arith (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRi*/, INT /*FRj*/, INT /*FRdoublei*/, INT /*FRdoublej*/, INT /*FRk*/, INT /*FRdoublek*/);
extern int frvbf_model_fr550_u_float_arith (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRi*/, INT /*FRj*/, INT /*FRdoublei*/, INT /*FRdoublej*/, INT /*FRk*/, INT /*FRdoublek*/);
extern int frvbf_model_fr550_u_check (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ICCi_3*/, INT /*FCCi_3*/);
extern int frvbf_model_fr550_u_trap (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*ICCi_2*/, INT /*FCCi_2*/);
extern int frvbf_model_fr550_u_branch (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*ICCi_2*/, INT /*FCCi_2*/);
extern int frvbf_model_fr550_u_idiv (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/, INT /*ICCi_1*/);
extern int frvbf_model_fr550_u_imul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRdoublek*/, INT /*ICCi_1*/);
extern int frvbf_model_fr550_u_integer (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/, INT /*ICCi_1*/);
extern int frvbf_model_fr550_u_exec (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int frvbf_model_fr500_u_commit (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRk*/, INT /*FRk*/);
extern int frvbf_model_fr500_u_dcul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr500_u_icul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr500_u_dcpl (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr500_u_icpl (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr500_u_dcf (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr500_u_dci (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr500_u_ici (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr500_u_membar (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int frvbf_model_fr500_u_barrier (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int frvbf_model_fr500_u_media_dual_btohe (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRintj*/, INT /*FRintk*/);
extern int frvbf_model_fr500_u_media_dual_htob (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRintj*/, INT /*FRintk*/);
extern int frvbf_model_fr500_u_media_dual_btoh (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRintj*/, INT /*FRintk*/);
extern int frvbf_model_fr500_u_media_dual_unpack (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintk*/);
extern int frvbf_model_fr500_u_media_dual_expand (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintk*/);
extern int frvbf_model_fr500_u_media_quad_complex (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*ACC40Sk*/);
extern int frvbf_model_fr500_u_media_quad_mul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*ACC40Sk*/, INT /*ACC40Uk*/);
extern int frvbf_model_fr500_u_media_dual_mul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*ACC40Sk*/, INT /*ACC40Uk*/);
extern int frvbf_model_fr500_u_media_quad_arith (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*FRintk*/);
extern int frvbf_model_fr500_u_media (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*ACC40Si*/, INT /*ACCGi*/, INT /*FRintk*/, INT /*ACC40Sk*/, INT /*ACC40Uk*/, INT /*ACCGk*/);
extern int frvbf_model_fr500_u_float_dual_convert (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRj*/, INT /*FRintj*/, INT /*FRk*/, INT /*FRintk*/);
extern int frvbf_model_fr500_u_float_convert (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRj*/, INT /*FRintj*/, INT /*FRdoublej*/, INT /*FRk*/, INT /*FRintk*/, INT /*FRdoublek*/);
extern int frvbf_model_fr500_u_float_dual_compare (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRi*/, INT /*FRj*/, INT /*FCCi_2*/);
extern int frvbf_model_fr500_u_float_compare (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRi*/, INT /*FRj*/, INT /*FRdoublei*/, INT /*FRdoublej*/, INT /*FCCi_2*/);
extern int frvbf_model_fr500_u_float_dual_sqrt (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRj*/, INT /*FRk*/);
extern int frvbf_model_fr500_u_float_sqrt (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRj*/, INT /*FRdoublej*/, INT /*FRk*/, INT /*FRdoublek*/);
extern int frvbf_model_fr500_u_float_div (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRi*/, INT /*FRj*/, INT /*FRk*/);
extern int frvbf_model_fr500_u_float_dual_arith (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRi*/, INT /*FRj*/, INT /*FRdoublei*/, INT /*FRdoublej*/, INT /*FRk*/, INT /*FRdoublek*/);
extern int frvbf_model_fr500_u_float_arith (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRi*/, INT /*FRj*/, INT /*FRdoublei*/, INT /*FRdoublej*/, INT /*FRk*/, INT /*FRdoublek*/);
extern int frvbf_model_fr500_u_gr2spr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRj*/, INT /*spr*/);
extern int frvbf_model_fr500_u_gr2fr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRj*/, INT /*FRintk*/);
extern int frvbf_model_fr500_u_spr2gr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*spr*/, INT /*GRj*/);
extern int frvbf_model_fr500_u_fr2gr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRintk*/, INT /*GRj*/);
extern int frvbf_model_fr500_u_fr2fr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRi*/, INT /*FRk*/);
extern int frvbf_model_fr500_u_swap (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/);
extern int frvbf_model_fr500_u_fr_r_store (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*FRintk*/, INT /*FRdoublek*/);
extern int frvbf_model_fr500_u_fr_store (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*FRintk*/, INT /*FRdoublek*/);
extern int frvbf_model_fr500_u_fr_load (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*FRintk*/, INT /*FRdoublek*/);
extern int frvbf_model_fr500_u_gr_r_store (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/, INT /*GRdoublek*/);
extern int frvbf_model_fr500_u_gr_store (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/, INT /*GRdoublek*/);
extern int frvbf_model_fr500_u_gr_load (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/, INT /*GRdoublek*/);
extern int frvbf_model_fr500_u_set_hilo (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRkhi*/, INT /*GRklo*/);
extern int frvbf_model_fr500_u_clrfr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRk*/);
extern int frvbf_model_fr500_u_clrgr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRk*/);
extern int frvbf_model_fr500_u_check (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ICCi_3*/, INT /*FCCi_3*/);
extern int frvbf_model_fr500_u_trap (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*ICCi_2*/, INT /*FCCi_2*/);
extern int frvbf_model_fr500_u_branch (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*ICCi_2*/, INT /*FCCi_2*/);
extern int frvbf_model_fr500_u_idiv (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/, INT /*ICCi_1*/);
extern int frvbf_model_fr500_u_imul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRdoublek*/, INT /*ICCi_1*/);
extern int frvbf_model_fr500_u_integer (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/, INT /*ICCi_1*/);
extern int frvbf_model_fr500_u_exec (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int frvbf_model_tomcat_u_exec (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int frvbf_model_fr400_u_dcul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr400_u_icul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr400_u_dcpl (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr400_u_icpl (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr400_u_dcf (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr400_u_dci (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr400_u_ici (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr400_u_membar (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int frvbf_model_fr400_u_barrier (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int frvbf_model_fr400_u_media_dual_htob (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRintj*/, INT /*FRintk*/);
extern int frvbf_model_fr400_u_media_dual_expand (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintk*/);
extern int frvbf_model_fr400_u_media_7 (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*FCCk*/);
extern int frvbf_model_fr400_u_media_6 (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintk*/);
extern int frvbf_model_fr400_u_media_4_acc_dual (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ACC40Si*/, INT /*FRintk*/);
extern int frvbf_model_fr400_u_media_4_accg (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ACCGi*/, INT /*FRinti*/, INT /*ACCGk*/, INT /*FRintk*/);
extern int frvbf_model_fr400_u_media_4 (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ACC40Si*/, INT /*FRintj*/, INT /*ACC40Sk*/, INT /*FRintk*/);
extern int frvbf_model_fr400_u_media_3_quad (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*FRintk*/);
extern int frvbf_model_fr400_u_media_3_dual (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintk*/);
extern int frvbf_model_fr400_u_media_3 (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*FRintk*/);
extern int frvbf_model_fr400_u_media_2_add_sub_dual (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ACC40Si*/, INT /*ACC40Sk*/);
extern int frvbf_model_fr400_u_media_2_add_sub (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ACC40Si*/, INT /*ACC40Sk*/);
extern int frvbf_model_fr400_u_media_2_acc_dual (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ACC40Si*/, INT /*ACC40Sk*/);
extern int frvbf_model_fr400_u_media_2_acc (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ACC40Si*/, INT /*ACC40Sk*/);
extern int frvbf_model_fr400_u_media_2_quad (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*ACC40Sk*/, INT /*ACC40Uk*/);
extern int frvbf_model_fr400_u_media_2 (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*ACC40Sk*/, INT /*ACC40Uk*/);
extern int frvbf_model_fr400_u_media_hilo (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRkhi*/, INT /*FRklo*/);
extern int frvbf_model_fr400_u_media_1_quad (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*FRintk*/);
extern int frvbf_model_fr400_u_media_1 (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*FRintk*/);
extern int frvbf_model_fr400_u_gr2spr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRj*/, INT /*spr*/);
extern int frvbf_model_fr400_u_gr2fr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRj*/, INT /*FRintk*/);
extern int frvbf_model_fr400_u_spr2gr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*spr*/, INT /*GRj*/);
extern int frvbf_model_fr400_u_fr2gr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRintk*/, INT /*GRj*/);
extern int frvbf_model_fr400_u_swap (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/);
extern int frvbf_model_fr400_u_fr_store (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*FRintk*/, INT /*FRdoublek*/);
extern int frvbf_model_fr400_u_fr_load (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*FRintk*/, INT /*FRdoublek*/);
extern int frvbf_model_fr400_u_gr_store (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/, INT /*GRdoublek*/);
extern int frvbf_model_fr400_u_gr_load (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/, INT /*GRdoublek*/);
extern int frvbf_model_fr400_u_set_hilo (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRkhi*/, INT /*GRklo*/);
extern int frvbf_model_fr400_u_check (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ICCi_3*/, INT /*FCCi_3*/);
extern int frvbf_model_fr400_u_trap (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*ICCi_2*/, INT /*FCCi_2*/);
extern int frvbf_model_fr400_u_branch (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*ICCi_2*/, INT /*FCCi_2*/);
extern int frvbf_model_fr400_u_idiv (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/, INT /*ICCi_1*/);
extern int frvbf_model_fr400_u_imul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRdoublek*/, INT /*ICCi_1*/);
extern int frvbf_model_fr400_u_integer (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/, INT /*ICCi_1*/);
extern int frvbf_model_fr400_u_exec (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int frvbf_model_fr450_u_dcul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr450_u_icul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr450_u_dcpl (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr450_u_icpl (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr450_u_dcf (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr450_u_dci (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr450_u_ici (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/);
extern int frvbf_model_fr450_u_membar (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int frvbf_model_fr450_u_barrier (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int frvbf_model_fr450_u_media_dual_htob (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRintj*/, INT /*FRintk*/);
extern int frvbf_model_fr450_u_media_dual_expand (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintk*/);
extern int frvbf_model_fr450_u_media_7 (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*FCCk*/);
extern int frvbf_model_fr450_u_media_6 (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintk*/);
extern int frvbf_model_fr450_u_media_4_mclracca (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int frvbf_model_fr450_u_media_4_acc_dual (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ACC40Si*/, INT /*FRintk*/);
extern int frvbf_model_fr450_u_media_4_accg (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ACCGi*/, INT /*FRinti*/, INT /*ACCGk*/, INT /*FRintk*/);
extern int frvbf_model_fr450_u_media_4 (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ACC40Si*/, INT /*FRintj*/, INT /*ACC40Sk*/, INT /*FRintk*/);
extern int frvbf_model_fr450_u_media_3_quad (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*FRintk*/);
extern int frvbf_model_fr450_u_media_3_dual (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintk*/);
extern int frvbf_model_fr450_u_media_3 (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*FRintk*/);
extern int frvbf_model_fr450_u_media_2_add_sub_dual (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ACC40Si*/, INT /*ACC40Sk*/);
extern int frvbf_model_fr450_u_media_2_add_sub (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ACC40Si*/, INT /*ACC40Sk*/);
extern int frvbf_model_fr450_u_media_2_acc_dual (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ACC40Si*/, INT /*ACC40Sk*/);
extern int frvbf_model_fr450_u_media_2_acc (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ACC40Si*/, INT /*ACC40Sk*/);
extern int frvbf_model_fr450_u_media_2_quad (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*ACC40Sk*/, INT /*ACC40Uk*/);
extern int frvbf_model_fr450_u_media_2 (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*ACC40Sk*/, INT /*ACC40Uk*/);
extern int frvbf_model_fr450_u_media_hilo (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRkhi*/, INT /*FRklo*/);
extern int frvbf_model_fr450_u_media_1_quad (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*FRintk*/);
extern int frvbf_model_fr450_u_media_1 (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRinti*/, INT /*FRintj*/, INT /*FRintk*/);
extern int frvbf_model_fr450_u_gr2spr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRj*/, INT /*spr*/);
extern int frvbf_model_fr450_u_gr2fr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRj*/, INT /*FRintk*/);
extern int frvbf_model_fr450_u_spr2gr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*spr*/, INT /*GRj*/);
extern int frvbf_model_fr450_u_fr2gr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*FRintk*/, INT /*GRj*/);
extern int frvbf_model_fr450_u_swap (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/);
extern int frvbf_model_fr450_u_fr_store (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*FRintk*/, INT /*FRdoublek*/);
extern int frvbf_model_fr450_u_fr_load (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*FRintk*/, INT /*FRdoublek*/);
extern int frvbf_model_fr450_u_gr_store (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/, INT /*GRdoublek*/);
extern int frvbf_model_fr450_u_gr_load (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/, INT /*GRdoublek*/);
extern int frvbf_model_fr450_u_set_hilo (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRkhi*/, INT /*GRklo*/);
extern int frvbf_model_fr450_u_check (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*ICCi_3*/, INT /*FCCi_3*/);
extern int frvbf_model_fr450_u_trap (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*ICCi_2*/, INT /*FCCi_2*/);
extern int frvbf_model_fr450_u_branch (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*ICCi_2*/, INT /*FCCi_2*/);
extern int frvbf_model_fr450_u_idiv (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/, INT /*ICCi_1*/);
extern int frvbf_model_fr450_u_imul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRdoublek*/, INT /*ICCi_1*/);
extern int frvbf_model_fr450_u_integer (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*GRi*/, INT /*GRj*/, INT /*GRk*/, INT /*ICCi_1*/);
extern int frvbf_model_fr450_u_exec (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int frvbf_model_simple_u_exec (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);

/* Profiling before/after handlers (user written) */

extern void frvbf_model_insn_before (SIM_CPU *, int /*first_p*/);
extern void frvbf_model_insn_after (SIM_CPU *, int /*last_p*/, int /*cycles*/);

#endif /* FRVBF_DECODE_H */
