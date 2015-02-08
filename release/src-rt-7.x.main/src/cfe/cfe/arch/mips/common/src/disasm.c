/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  MIPS disassembler		    	File: disasm.c
    *  
    *  MIPS disassembler (used by ui_examcmds.c)
    *  
    *  Author:  Justin Carlson (carlson@broadcom.com)
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */


#include "lib_types.h"
#include "lib_string.h"
#include "disasm.h"
#include "lib_printf.h"

#define UINT64_T(x) ((uint64_t) (x##LL))
#define PF_64 "ll"
#define PF_32 ""
/* These aren't guaranteed to be portable, either */
#define SEXT_32(bit, val) \
			   ((((int32_t)(val))<<(31-(bit)))>>(31-(bit)))
#define SEXT_64(bit, val) \
			   ((((int64_t)(val))<<(63-(bit)))>>(63-(bit)))

#define DATASEG __attribute__ ((section(".text")))


#define REGNAME(r) (&_regnames[(r)*5])
static const char * const _regnames =
    "zero\0"
    "AT\0  "
    "v0\0  "
    "v1\0  "
    "a0\0  "
    "a1\0  "
    "a2\0  "
    "a3\0  "
    "t0\0  "
    "t1\0  "
    "t2\0  "
    "t3\0  "
    "t4\0  "
    "t5\0  "
    "t6\0  "
    "t7\0  "
    "s0\0  "
    "s1\0  "
    "s2\0  "
    "s3\0  "
    "s4\0  "
    "s5\0  "
    "s6\0  "
    "s7\0  "
    "t8\0  "
    "t9\0  "
    "k0\0  "
    "k1\0  "
    "gp\0  "
    "sp\0  "
    "fp\0  "
    "ra\0  ";	

#define CP0REGNAME(r) (&_cp0names[(r)*12])
static const char * const _cp0names =
    "C0_INX\0     "
    "C0_RAND\0    "
    "C0_TLBLO0\0  "
    "C0_TLBLO1\0  "
    	
    "C0_CTEXT\0   "
    "C0_PGMASK\0  "
    "C0_WIRED\0   "
    "C0_reserved\0"
    
    "C0_BADVADDR\0"
    "C0_COUNT\0   "
    "C0_TLBHI\0   "
    "C0_COMPARE\0 "
    
    "C0_SR\0      "
    "C0_CAUSE\0   "
    "C0_EPC\0     "
    "C0_PRID\0    "
    	
    "C0_CONFIG\0  "
    "C0_LLADDR\0  "
    "C0_WATCHLO\0 "
    "C0_WATCHHI\0 "
    
    "C0_XCTEXT\0  "
    "C0_reserved\0"
    "C0_reserved\0"
    "C0_reserved\0"
    
    "C0_reserved\0"
    "C0_reserved\0"
    "C0_reserved\0"
    "C0_reserved\0"
    
    "C0_TAGLO\0   "
    "C0_TAGHI\0   "
    "C0_ERREPC\0  "
    "C0_reserved\0";



/*
 * MIPS instruction set disassembly module.  
 */

typedef enum {
	DC_RD_RS_RT,
	DC_RD_RT_RS,
	DC_RT_RS_SIMM,
	DC_RT_RS_XIMM,
	DC_RS_RT_OFS,
	DC_RS_OFS,
	DC_RD_RT_SA,
	DC_RT_UIMM,
	DC_RD,
	DC_J,
	DC_RD_RS,
	DC_RS_RT,
	DC_RT_RS,
	DC_RT_RD_SEL,
	DC_RT_CR_SEL,
	DC_RS,
	DC_RS_SIMM,
	DC_RT_OFS_BASE,
	DC_FT_OFS_BASE,
	DC_FD_IDX_BASE,
	DC_FS_IDX_BASE,
	DC_FD_FS_FT,
	DC_FD_FS_RT,
	DC_FD_FS,
	DC_PREF_OFS,
	DC_PREF_IDX,
	DC_CC_OFS,
	DC_FD_FS_CC,
	DC_RD_RS_CC,
	DC_FD_FR_FS_FT,
	DC_FD_FS_FT_RS,
	DC_CC_FS_FT,
	DC_BARE,
	DC_RT_FS,
	DC_SYSCALL,
	DC_BREAK,
	DC_VD_VS_VT_VEC,
	DC_VD_VS_VT,
	DC_VS_VT,
	DC_VS_VT_VEC,
	DC_VD_VS_VT_RS,
	DC_VD_VS_VT_IMM,
	DC_VD_VT,
	DC_VD,
	DC_VS,
	DC_DEREF,
	DC_OOPS
} DISASM_CLASS;




/* We're pulling some trickery here.  Most of the time, this structure operates
   exactly as one would expect.  The special case, when type == DC_DREF, 
   means name points to a byte that is an index into a dereferencing array.  */

/*
 * To make matters worse, the whole module has been coded to reduce the 
 * number of relocations present, so we don't actually store pointers
 * in the dereferencing array.  Instead, we store fixed-width strings
 * and use digits to represent indicies into the deref array.
 *
 * This is all to make more things fit in the relocatable version,
 * since every initialized pointer goes into our small data segment.
 */

typedef struct {
    char name[15];
    char type;
} disasm_t;

typedef struct {
	const disasm_t *ptr;
	int shift;
	uint32_t mask;
} disasm_deref_t;
		
/* Forward declaration of deref array, we need this for the disasm_t definitions */


extern const disasm_deref_t disasm_deref[];

static const disasm_t disasm_normal[64] DATASEG = 
{{"$1"        , DC_DEREF       }, 
 {"$2"        , DC_DEREF       }, 
 {"j"             , DC_J           }, 
 {"jal"           , DC_J           },
 {"beq"           , DC_RS_RT_OFS   }, 
 {"bne"           , DC_RS_RT_OFS   }, 
 {"blez"          , DC_RS_OFS      }, 
 {"bgtz"          , DC_RS_OFS      },
 {"addi"          , DC_RT_RS_SIMM  }, 
 {"addiu"         , DC_RT_RS_SIMM  }, 
 {"slti"          , DC_RT_RS_SIMM  }, 
 {"sltiu"         , DC_RT_RS_SIMM  },
 {"andi"          , DC_RT_RS_XIMM  }, 
 {"ori"           , DC_RT_RS_XIMM  }, 
 {"xori"          , DC_RT_RS_XIMM  }, 
 {"lui"           , DC_RT_UIMM     },
 {"$4"        , DC_DEREF       }, 
 {"$6"        , DC_DEREF       }, 
 {"invalid"       , DC_BARE        }, 
 {"$15"       , DC_DEREF       },
 {"beql"          , DC_RS_RT_OFS   }, 
 {"bnel"          , DC_RS_RT_OFS   }, 
 {"blezl"         , DC_RS_OFS      }, 
 {"bgtzl"         , DC_RS_OFS      },
 {"daddi"         , DC_RT_RS_SIMM  }, 
 {"daddiu"        , DC_RT_RS_SIMM  }, 
 {"ldl"           , DC_RT_OFS_BASE }, 
 {"ldr"           , DC_RT_OFS_BASE },
 {"$3"        , DC_DEREF       }, 
 {"invalid"       , DC_BARE        }, 
 {"$18"       , DC_DEREF       }, 
 {"invalid"       , DC_BARE        },
 {"lb"            , DC_RT_OFS_BASE }, 
 {"lh"            , DC_RT_OFS_BASE }, 
 {"lwl"           , DC_RT_OFS_BASE }, 
 {"lw"            , DC_RT_OFS_BASE },
 {"lbu"           , DC_RT_OFS_BASE }, 
 {"lhu"           , DC_RT_OFS_BASE }, 
 {"lwr"           , DC_RT_OFS_BASE }, 
 {"lwu"           , DC_RT_OFS_BASE },
 {"sb"            , DC_RT_OFS_BASE }, 
 {"sh"            , DC_RT_OFS_BASE }, 
 {"swl"           , DC_RT_OFS_BASE }, 
 {"sw"            , DC_RT_OFS_BASE },
 {"sdl"           , DC_RT_OFS_BASE }, 
 {"sdr"           , DC_RT_OFS_BASE }, 
 {"swr"           , DC_RT_OFS_BASE }, 
 {"cache"         , DC_BARE        },
 {"ll"            , DC_RT_OFS_BASE }, 
 {"lwc1"          , DC_FT_OFS_BASE }, 
 {"invalid"       , DC_BARE        }, 
 {"pref"          , DC_PREF_OFS    },
 {"lld"           , DC_RT_OFS_BASE }, 
 {"ldc1"          , DC_FT_OFS_BASE }, 
 {"invalid"       , DC_BARE        }, 
 {"ld"            , DC_RT_OFS_BASE },
 {"sc"            , DC_RT_OFS_BASE }, 
 {"swc1"          , DC_FT_OFS_BASE }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"scd"           , DC_RT_OFS_BASE }, 
 {"sdc1"          , DC_FT_OFS_BASE }, 
 {"invalid"       , DC_BARE        }, 
 {"sd"            , DC_RT_OFS_BASE }};

static const disasm_t disasm_special[64] DATASEG = 
{{"sll"           , DC_RD_RT_SA    }, 
 {"$16"       , DC_DEREF       }, 
 {"srl"           , DC_RD_RT_SA    }, 
 {"sra"           , DC_RD_RT_SA    },
 {"sllv"          , DC_RD_RT_RS    }, 
 {"invalid"       , DC_BARE        }, 
 {"srlv"          , DC_RD_RT_RS    }, 
 {"srav"          , DC_RD_RT_RS    },
 {"jr"            , DC_RS          }, 
 {"jalr"          , DC_RD_RS       }, 
 {"movz"          , DC_RD_RS_RT    }, 
 {"movn"          , DC_RD_RS_RT    },
 {"syscall"       , DC_SYSCALL     }, 
 {"break"         , DC_BREAK       }, 
 {"invalid"       , DC_BARE        }, 
 {"sync"          , DC_BARE        },
 {"mfhi"          , DC_RD          }, 
 {"mthi"          , DC_RS          }, 
 {"mflo"          , DC_RD          }, 
 {"mtlo"          , DC_RS          },
 {"dsllv"         , DC_RD_RT_RS    }, 
 {"invalid"       , DC_BARE        }, 
 {"dsrlv"         , DC_RD_RT_RS    }, 
 {"dsrav"         , DC_RD_RT_RS    },
 {"mult"          , DC_RS_RT       }, 
 {"multu"         , DC_RS_RT       }, 
 {"div"           , DC_RS_RT       }, 
 {"divu"          , DC_RS_RT       },
 {"dmult"         , DC_RS_RT       }, 
 {"dmultu"        , DC_RS_RT       }, 
 {"ddiv"          , DC_RS_RT       }, 
 {"ddivu"         , DC_RS_RT       },
 {"add"           , DC_RD_RS_RT    }, 
 {"addu"          , DC_RD_RS_RT    }, 
 {"sub"           , DC_RD_RS_RT    }, 
 {"subu"          , DC_RD_RS_RT    },
 {"and"           , DC_RD_RS_RT    }, 
 {"or"            , DC_RD_RS_RT    }, 
 {"xor"           , DC_RD_RS_RT    }, 
 {"nor"           , DC_RD_RS_RT    },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"slt"           , DC_RD_RS_RT    }, 
 {"sltu"          , DC_RD_RS_RT    },
 {"dadd"          , DC_RD_RS_RT    }, 
 {"daddu"         , DC_RD_RS_RT    }, 
 {"dsub"          , DC_RD_RS_RT    }, 
 {"dsubu"         , DC_RD_RS_RT    },
 {"tge"           , DC_RS_RT       }, 
 {"tgeu"          , DC_RS_RT       }, 
 {"tlt"           , DC_RS_RT       }, 
 {"tltu"          , DC_RS_RT       },
 {"teq"           , DC_RS_RT       }, 
 {"invalid"       , DC_BARE        }, 
 {"tne"           , DC_RS_RT       }, 
 {"invalid"       , DC_BARE        },
 {"dsll"          , DC_RD_RT_SA    }, 
 {"invalid"       , DC_BARE        }, 
 {"dsrl"          , DC_RD_RT_SA    }, 
 {"dsra"          , DC_RD_RT_SA    },
 {"dsll32"        , DC_RD_RT_SA    }, 
 {"invalid"       , DC_BARE        }, 
 {"dsrl32"        , DC_RD_RT_SA    }, 
 {"dsra32"        , DC_RD_RT_SA    }};

static const disasm_t disasm_movci[2] DATASEG = 
{{"movf"          , DC_RD_RS_CC    },
 {"movt"          , DC_RD_RS_CC    }};

static const disasm_t disasm_regimm[32] DATASEG = 
{{"bltz"          , DC_RS_OFS      }, 
 {"bgez"          , DC_RS_OFS      }, 
 {"bltzl"         , DC_RS_OFS      }, 
 {"bgezl"         , DC_RS_OFS      },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"tgei"          , DC_RS_SIMM     }, 
 {"tgeiu"         , DC_RS_SIMM     }, 
 {"tlti"          , DC_RS_SIMM     }, 
 {"tltiu"         , DC_RS_SIMM     },
 {"teqi"          , DC_RS_SIMM     }, 
 {"invalid"       , DC_BARE        }, 
 {"tnei"          , DC_RS_SIMM     }, 
 {"invalid"       , DC_BARE        },
 {"bltzal"        , DC_RS_OFS      }, 
 {"bgezal"        , DC_RS_OFS      }, 
 {"bltzall"       , DC_RS_OFS      }, 
 {"bgezall"       , DC_RS_OFS      },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE     }};

static const disasm_t disasm_spec2[64] DATASEG =
{{"madd"          , DC_RS_RT       }, 
 {"maddu"         , DC_RS_RT       }, 
 {"mul"           , DC_RD_RS_RT    }, 
 {"invalid"       , DC_BARE        },
 {"msub"          , DC_RS_RT       }, 
 {"msubu"         , DC_RS_RT       }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"clz"           , DC_RT_RS       }, 
 {"clo"           , DC_RT_RS       }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"dclz"          , DC_RT_RS       }, 
 {"dclo"          , DC_RT_RS       }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"sdbbp"         , DC_BARE        }};

static const disasm_t disasm_cop0[32] DATASEG = 
{{"mfc0@1"        , DC_RT_CR_SEL   }, 
 {"dmfc0@1"       , DC_RT_CR_SEL   }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"mtc0@1"        , DC_RT_CR_SEL   }, 
 {"dmtc0@1"       , DC_RT_CR_SEL   }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"$5"        , DC_DEREF       },
 {"$5"        , DC_DEREF       },
 {"$5"        , DC_DEREF       },
 {"$5"        , DC_DEREF       },
 {"$5"        , DC_DEREF       },
 {"$5"        , DC_DEREF       },
 {"$5"        , DC_DEREF       },
 {"$5"        , DC_DEREF       },
 {"$5"        , DC_DEREF       },
 {"$5"        , DC_DEREF       },
 {"$5"        , DC_DEREF       },
 {"$5"        , DC_DEREF       },
 {"$5"        , DC_DEREF       },
 {"$5"        , DC_DEREF       },
 {"$5"        , DC_DEREF       },
 {"$5"        , DC_DEREF       }};

static const disasm_t disasm_cop0_c0[64] DATASEG = 
{{"invalid"       , DC_BARE        }, 
 {"tlbr"          , DC_BARE        }, 
 {"tlbwi"         , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"tlbwr"         , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"tlbp"          , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"eret"          , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"deret"         , DC_BARE        },
 {"wait"          , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE     }};

static const disasm_t disasm_cop1[32] DATASEG = 
{{"mfc1"          , DC_RT_FS       }, 
 {"dmfc1"         , DC_RT_FS       }, 
 {"cfc1"          , DC_RT_FS       }, 
 {"invalid"       , DC_BARE        },
 {"mtc1"          , DC_RT_FS       }, 
 {"dmtc1"         , DC_RT_FS       }, 
 {"ctc1"          , DC_RT_FS       }, 
 {"invalid"       , DC_BARE        },
 {"$17"       , DC_DEREF       }, 
 {"$36"       , DC_DEREF       }, 
 {"$37"       , DC_DEREF       }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"$7"        , DC_DEREF       }, 
 {"$9"        , DC_DEREF       }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"$11"       , DC_DEREF       }, 
 {"$12"       , DC_DEREF       }, 
 {"$13"       , DC_DEREF       }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE     }};

static const disasm_t disasm_cop1_bc1[4] DATASEG = 
{{"bc1f"          , DC_CC_OFS      },
 {"bc1t"          , DC_CC_OFS      },
 {"bc1fl"         , DC_CC_OFS      },
 {"bc1tl"         , DC_CC_OFS      },
};

static const disasm_t disasm_cop1_bc1any2[2] DATASEG = 
{{"bc1any2f"      , DC_CC_OFS      },
 {"bc1any2t"      , DC_CC_OFS      },
};

static const disasm_t disasm_cop1_bc1any4[2] DATASEG = 
{{"bc1any4f"      , DC_CC_OFS      },
 {"bc1any4t"      , DC_CC_OFS      },
};

static const disasm_t disasm_cop1_s[64] DATASEG = 
{{"add.s"         , DC_FD_FS_FT    }, 
 {"sub.s"         , DC_FD_FS_FT    }, 
 {"mul.s"         , DC_FD_FS_FT    }, 
 {"div.s"         , DC_FD_FS_FT    },
 {"sqrt.s"        , DC_FD_FS       }, 
 {"abs.s"         , DC_FD_FS       }, 
 {"mov.s"         , DC_FD_FS       }, 
 {"neg.s"         , DC_FD_FS       },
 {"round.l.s"     , DC_FD_FS       }, 
 {"trunc.l.s"     , DC_FD_FS       }, 
 {"ceil.l.s"      , DC_FD_FS       }, 
 {"floor.l.s"     , DC_FD_FS       },
 {"round.w.s"     , DC_FD_FS       }, 
 {"trunc.w.s"     , DC_FD_FS       }, 
 {"ceil.w.s"      , DC_FD_FS       }, 
 {"floor.w.s"     , DC_FD_FS       },
 {"invalid"       , DC_BARE        }, 
 {"$8"        , DC_DEREF       },
 {"movz.s"        , DC_FD_FS_RT    }, 
 {"movn.s"        , DC_FD_FS_RT    },
 {"invalid"       , DC_BARE        }, 
 {"recip.s"       , DC_FD_FS       }, 
 {"rsqrt.s"       , DC_FD_FS       }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"recip2.s"      , DC_FD_FS_FT    }, 
 {"recip1.s"      , DC_FD_FS       }, 
 {"rsqrt1.s"      , DC_FD_FS       }, 
 {"rsqrt2.s"      , DC_FD_FS_FT    },
 {"invalid"       , DC_BARE        }, 
 {"cvt.d.s"       , DC_FD_FS       }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"cvt.w.s"       , DC_FD_FS       }, 
 {"cvt.l.s"       , DC_FD_FS       }, 
 {"cvt.ps.s"      , DC_FD_FS_FT    }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"$38"       , DC_DEREF       }, 
 {"$39"       , DC_DEREF       }, 
 {"$40"       , DC_DEREF       }, 
 {"$41"       , DC_DEREF       }, 
 {"$42"       , DC_DEREF       }, 
 {"$43"       , DC_DEREF       }, 
 {"$44"       , DC_DEREF       }, 
 {"$45"       , DC_DEREF       }, 
 {"$46"       , DC_DEREF       }, 
 {"$47"       , DC_DEREF       }, 
 {"$48"       , DC_DEREF       }, 
 {"$49"       , DC_DEREF       }, 
 {"$50"       , DC_DEREF       }, 
 {"$51"       , DC_DEREF       }, 
 {"$52"       , DC_DEREF       }, 
 {"$53"       , DC_DEREF       }};

static const disasm_t disasm_cop1_s_mvcf[2] DATASEG = 
{{"movf.s"        , DC_FD_FS_CC    }, 
 {"movt.s"        , DC_FD_FS_CC    }};

static const disasm_t disasm_cop1_d[64] DATASEG = 
{{"add.d"         , DC_FD_FS_FT    }, 
 {"sub.d"         , DC_FD_FS_FT    }, 
 {"mul.d"         , DC_FD_FS_FT    }, 
 {"div.d"         , DC_FD_FS_FT    },
 {"sqrt.d"        , DC_FD_FS       }, 
 {"abs.d"         , DC_FD_FS       }, 
 {"mov.d"         , DC_FD_FS       }, 
 {"neg.d"         , DC_FD_FS       },
 {"round.l.d"     , DC_FD_FS       }, 
 {"trunc.l.d"     , DC_FD_FS       }, 
 {"ceil.l.d"      , DC_FD_FS       }, 
 {"floor.l.d"     , DC_FD_FS       },
 {"round.w.d"     , DC_FD_FS       }, 
 {"trunc.w.d"     , DC_FD_FS       }, 
 {"ceil.w.d"      , DC_FD_FS       }, 
 {"floor.w.d"     , DC_FD_FS       },
 {"invalid"       , DC_BARE        }, 
 {"$10"       , DC_DEREF       }, 
 {"movz.d"        , DC_FD_FS_RT    }, 
 {"movn.d"        , DC_FD_FS_RT    },
 {"invalid"       , DC_BARE        }, 
 {"recip.d"       , DC_FD_FS       }, 
 {"rsqrt.d"       , DC_FD_FS       }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"recip2.d"      , DC_FD_FS_FT    }, 
 {"recip1.d"      , DC_FD_FS       }, 
 {"rsqrt1.d"      , DC_FD_FS       }, 
 {"rsqrt2.d"      , DC_FD_FS_FT    },
 {"cvt.s.d"       , DC_FD_FS       }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"cvt.w.d"       , DC_FD_FS       }, 
 {"cvt.l.d"       , DC_FD_FS       }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"$54"       , DC_DEREF       }, 
 {"$55"       , DC_DEREF       }, 
 {"$56"       , DC_DEREF       }, 
 {"$57"       , DC_DEREF       }, 
 {"$58"       , DC_DEREF       }, 
 {"$59"       , DC_DEREF       }, 
 {"$60"       , DC_DEREF       }, 
 {"$61"       , DC_DEREF       }, 
 {"$62"       , DC_DEREF       }, 
 {"$63"       , DC_DEREF       }, 
 {"$64"       , DC_DEREF       }, 
 {"$65"       , DC_DEREF       }, 
 {"$66"       , DC_DEREF       }, 
 {"$67"       , DC_DEREF       }, 
 {"$68"       , DC_DEREF       }, 
 {"$69"       , DC_DEREF       }};

static const disasm_t disasm_cop1_d_mvcf[2] DATASEG = 
{{"movf.d"        , DC_FD_FS_CC    }, 
 {"movt.d"        , DC_FD_FS_CC    }};

static const disasm_t disasm_cop1_w[64] DATASEG =
{{"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"cvt.s.w"       , DC_FD_FS       }, 
 {"cvt.d.w"       , DC_FD_FS       }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"cvt.ps.pw"     , DC_FD_FS       }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE     }};

static const disasm_t disasm_cop1_l[64] DATASEG =
{{"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"cvt.s.l"       , DC_FD_FS       }, 
 {"cvt.d.l"       , DC_FD_FS       }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE     }};

static const disasm_t disasm_cop1_ps[64] DATASEG = 
{{"add.ps"        , DC_FD_FS_FT    }, 
 {"sub.ps"        , DC_FD_FS_FT    }, 
 {"mul.ps"        , DC_FD_FS_FT    }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"abs.ps"        , DC_FD_FS       }, 
 {"mov.ps"        , DC_FD_FS       }, 
 {"neg.ps"        , DC_FD_FS       },
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"$14"       , DC_DEREF       }, 
 {"movz.ps"       , DC_FD_FS_RT    }, 
 {"movn.ps"       , DC_FD_FS_RT    }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"addr.ps"       , DC_FD_FS_FT    }, 
 {"invalid"       , DC_BARE        }, 
 {"mulr.ps"       , DC_FD_FS_FT    }, 
 {"invalid"       , DC_BARE        },
 {"recip2.ps"     , DC_FD_FS_FT    }, 
 {"recip1.ps"     , DC_FD_FS       }, 
 {"rsqrt1.ps"     , DC_FD_FS       }, 
 {"rsqrt2.ps"     , DC_FD_FS_FT    },
 {"cvt.s.pu"      , DC_FD_FS       }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"cvt.pw.ps"     , DC_FD_FS       },
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        },
 {"invalid"       , DC_BARE        },
 {"cvt.s.pl"      , DC_FD_FS       }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        },
 {"pll.ps"        , DC_FD_FS_FT    }, 
 {"plu.ps"        , DC_FD_FS_FT    }, 
 {"pul.ps"        , DC_FD_FS_FT    }, 
 {"puu.ps"        , DC_FD_FS_FT    },
 {"$70"       , DC_DEREF       }, 
 {"$71"       , DC_DEREF       }, 
 {"$72"       , DC_DEREF       }, 
 {"$73"       , DC_DEREF       }, 
 {"$74"       , DC_DEREF       }, 
 {"$75"       , DC_DEREF       }, 
 {"$76"       , DC_DEREF       }, 
 {"$77"       , DC_DEREF       }, 
 {"$78"       , DC_DEREF       }, 
 {"$79"       , DC_DEREF       }, 
 {"$80"       , DC_DEREF       }, 
 {"$81"       , DC_DEREF       }, 
 {"$82"       , DC_DEREF       }, 
 {"$83"       , DC_DEREF       }, 
 {"$84"       , DC_DEREF       }, 
 {"$85"       , DC_DEREF       }};

static const disasm_t disasm_cop1_c_f_s[2] DATASEG = 
{{"c.f.s"         , DC_CC_FS_FT    },
 {"cabs.f.s"      , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_un_s[2] DATASEG = 
{{"c.un.s"        , DC_CC_FS_FT    },
 {"cabs.un.s"     , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_eq_s[2] DATASEG = 
{{"c.eq.s"        , DC_CC_FS_FT    },
 {"cabs.eq.s"     , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ueq_s[2] DATASEG = 
{{"c.ueq.s"       , DC_CC_FS_FT    },
 {"cabs.ueq.s"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_olt_s[2] DATASEG = 
{{"c.olt.s"       , DC_CC_FS_FT    },
 {"cabs.olt.s"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ult_s[2] DATASEG = 
{{"c.ult.s"       , DC_CC_FS_FT    },
 {"cabs.ult.s"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ole_s[2] DATASEG = 
{{"c.ole.s"       , DC_CC_FS_FT    },
 {"cabs.ole.s"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ule_s[2] DATASEG = 
{{"c.ule.s"       , DC_CC_FS_FT    },
 {"cabs.ule.s"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_sf_s[2] DATASEG = 
{{"c.sf.s"        , DC_CC_FS_FT    },
 {"cabs.sf.s"     , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ngle_s[2] DATASEG = 
{{"c.ngle.s"      , DC_CC_FS_FT    },
 {"cabs.ngle.s"   , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_seq_s[2] DATASEG = 
{{"c.seq.s"       , DC_CC_FS_FT    },
 {"cabs.seq.s"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ngl_s[2] DATASEG = 
{{"c.ngl.s"       , DC_CC_FS_FT    },
 {"cabs.ngl.s"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_lt_s[2] DATASEG = 
{{"c.lt.s"        , DC_CC_FS_FT    },
 {"cabs.lt.s"     , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_nge_s[2] DATASEG = 
{{"c.nge.s"       , DC_CC_FS_FT    },
 {"cabs.nge.s"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_le_s[2] DATASEG = 
{{"c.le.s"        , DC_CC_FS_FT    },
 {"cabs.le.s"     , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ngt_s[2] DATASEG = 
{{"c.ngt.s"       , DC_CC_FS_FT    },
 {"cabs.ngt.s"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_f_d[2] DATASEG = 
{{"c.f.d"         , DC_CC_FS_FT    },
 {"cabs.f.d"      , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_un_d[2] DATASEG = 
{{"c.un.d"        , DC_CC_FS_FT    },
 {"cabs.un.d"     , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_eq_d[2] DATASEG = 
{{"c.eq.d"        , DC_CC_FS_FT    },
 {"cabs.eq.d"     , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ueq_d[2] DATASEG = 
{{"c.ueq.d"       , DC_CC_FS_FT    },
 {"cabs.ueq.d"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_olt_d[2] DATASEG = 
{{"c.olt.d"       , DC_CC_FS_FT    },
 {"cabs.olt.d"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ult_d[2] DATASEG = 
{{"c.ult.d"       , DC_CC_FS_FT    },
 {"cabs.ult.d"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ole_d[2] DATASEG = 
{{"c.ole.d"       , DC_CC_FS_FT    },
 {"cabs.ole.d"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ule_d[2] DATASEG = 
{{"c.ule.d"       , DC_CC_FS_FT    },
 {"cabs.ule.d"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_sf_d[2] DATASEG = 
{{"c.sf.d"        , DC_CC_FS_FT    },
 {"cabs.sf.d"     , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ngle_d[2] DATASEG = 
{{"c.ngle.d"      , DC_CC_FS_FT    },
 {"cabs.ngle.d"   , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_seq_d[2] DATASEG = 
{{"c.seq.d"       , DC_CC_FS_FT    },
 {"cabs.seq.d"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ngl_d[2] DATASEG = 
{{"c.ngl.d"       , DC_CC_FS_FT    },
 {"cabs.ngl.d"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_lt_d[2] DATASEG = 
{{"c.lt.d"        , DC_CC_FS_FT    },
 {"cabs.lt.d"     , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_nge_d[2] DATASEG = 
{{"c.nge.d"       , DC_CC_FS_FT    },
 {"cabs.nge.d"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_le_d[2] DATASEG = 
{{"c.le.d"        , DC_CC_FS_FT    },
 {"cabs.le.d"     , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ngt_d[2] DATASEG = 
{{"c.ngt.d"       , DC_CC_FS_FT    },
 {"cabs.ngt.d"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_f_ps[2] DATASEG = 
{{"c.f.ps"        , DC_CC_FS_FT    },
 {"cabs.f.ps"     , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_un_ps[2] DATASEG = 
{{"c.un.ps"       , DC_CC_FS_FT    },
 {"cabs.un.ps"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_eq_ps[2] DATASEG = 
{{"c.eq.ps"       , DC_CC_FS_FT    },
 {"cabs.eq.ps"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ueq_ps[2] DATASEG = 
{{"c.ueq.ps"      , DC_CC_FS_FT    },
 {"cabs.ueq.ps"   , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_olt_ps[2] DATASEG = 
{{"c.olt.ps"      , DC_CC_FS_FT    },
 {"cabs.olt.ps"   , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ult_ps[2] DATASEG = 
{{"c.ult.ps"      , DC_CC_FS_FT    },
 {"cabs.ult.ps"   , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ole_ps[2] DATASEG = 
{{"c.ole.ps"      , DC_CC_FS_FT    },
 {"cabs.ole.ps"   , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ule_ps[2] DATASEG = 
{{"c.ule.ps"      , DC_CC_FS_FT    },
 {"cabs.ule.ps"   , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_sf_ps[2] DATASEG = 
{{"c.sf.ps"       , DC_CC_FS_FT    },
 {"cabs.sf.ps"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ngle_ps[2] DATASEG = 
{{"c.ngle.ps"     , DC_CC_FS_FT    },
 {"cabs.ngle.ps"  , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_seq_ps[2] DATASEG = 
{{"c.seq.ps"      , DC_CC_FS_FT    },
 {"cabs.seq.ps"   , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ngl_ps[2] DATASEG = 
{{"c.ngl.ps"      , DC_CC_FS_FT    },
 {"cabs.ngl.ps"   , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_lt_ps[2] DATASEG = 
{{"c.lt.ps"       , DC_CC_FS_FT    },
 {"cabs.lt.ps"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_nge_ps[2] DATASEG = 
{{"c.nge.ps"      , DC_CC_FS_FT    },
 {"cabs.nge.ps"   , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_le_ps[2] DATASEG = 
{{"c.le.ps"       , DC_CC_FS_FT    },
 {"cabs.le.ps"    , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_c_ngt_ps[2] DATASEG = 
{{"c.ngt.ps"      , DC_CC_FS_FT    },
 {"cabs.ngt.ps"   , DC_CC_FS_FT    }};

static const disasm_t disasm_cop1_ps_mvcf[2] DATASEG = 
{{"movf.ps"       , DC_FD_FS_CC    }, 
 {"movt.ps"       , DC_FD_FS_CC    }};

static const disasm_t disasm_cop1x[64] DATASEG = 
{{"lwxc1"         , DC_FD_IDX_BASE }, 
 {"ldxc1"         , DC_FD_IDX_BASE }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"luxc1"         , DC_FD_IDX_BASE }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"swxc1"         , DC_FS_IDX_BASE }, 
 {"sdxc1"         , DC_FS_IDX_BASE }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"suxc1"         , DC_FS_IDX_BASE }, 
 {"invalid"       , DC_BARE        }, 
 {"prefx"         , DC_PREF_IDX    }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"alnv.ps"       , DC_FD_FS_FT_RS }, 
 {"invalid"       , DC_BARE        }, 
 {"madd.s"        , DC_FD_FR_FS_FT }, 
 {"madd.d"        , DC_FD_FR_FS_FT }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"madd.ps"       , DC_FD_FR_FS_FT }, 
 {"invalid"       , DC_BARE        }, 
 {"msub.s"        , DC_FD_FR_FS_FT }, 
 {"msub.d"        , DC_FD_FR_FS_FT }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"msub.ps"       , DC_FD_FR_FS_FT }, 
 {"invalid"       , DC_BARE        }, 
 {"nmadd.s"       , DC_FD_FR_FS_FT }, 
 {"nmadd.d"       , DC_FD_FR_FS_FT }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"nmadd.ps"      , DC_FD_FR_FS_FT }, 
 {"invalid"       , DC_BARE        }, 
 {"nmsub.s"       , DC_FD_FR_FS_FT }, 
 {"nmsub.d"       , DC_FD_FR_FS_FT }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"invalid"       , DC_BARE        }, 
 {"nmsub.ps"      , DC_FD_FR_FS_FT }, 
 {"invalid"       , DC_BARE        }};

static const disasm_t disasm_mdmx[8] DATASEG = 
{{ "$20"      , DC_DEREF       },
 { "$19"      , DC_DEREF       },
 { "$20"      , DC_DEREF       },
 { "$33"      , DC_DEREF       },
 { "$20"      , DC_DEREF       },
 { "$19"      , DC_DEREF       },
 { "$20"      , DC_DEREF       },
 { "$33"      , DC_DEREF       },
};

static const disasm_t disasm_mdmx_qh[64] DATASEG = 
{{ "msgn.qh"      , DC_VD_VS_VT_VEC},
 { "c.eq.qh"      , DC_VS_VT_VEC   },
 { "pickf.qh"     , DC_VD_VS_VT_VEC},
 { "pickt.qh"     , DC_VD_VS_VT_VEC},
 { "c.lt.qh"      , DC_VS_VT_VEC   },
 { "c.le.qh"      , DC_VS_VT_VEC   },
 { "min.qh"       , DC_VD_VS_VT_VEC},
 { "max.qh"       , DC_VD_VS_VT_VEC},

 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "sub.qh"       , DC_VD_VS_VT_VEC},
 { "add.qh"       , DC_VD_VS_VT_VEC},
 { "and.qh"       , DC_VD_VS_VT_VEC},
 { "xor.qh"       , DC_VD_VS_VT_VEC},
 { "or.qh"        , DC_VD_VS_VT_VEC},
 { "nor.qh"       , DC_VD_VS_VT_VEC},

 { "sll.qh"       , DC_VD_VS_VT_VEC},
 { "invalid"      , DC_BARE        },
 { "srl.qh"       , DC_VD_VS_VT_VEC},
 { "sra.qh"       , DC_VD_VS_VT_VEC},
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },

 { "alni.ob"      , DC_VD_VS_VT_IMM},
 { "alnv.ob"      , DC_VD_VS_VT_RS },
 { "alni.qh"      , DC_VD_VS_VT_IMM},
 { "alnv.qh"      , DC_VD_VS_VT_RS },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "$34"      , DC_DEREF       },

 { "rzu.qh"       , DC_VD_VT       },
 { "rnau.qh"      , DC_VD_VT       },
 { "rneu.qh"      , DC_VD_VT       },
 { "invalid"      , DC_BARE        },
 { "rzs.qh"       , DC_VD_VT       },
 { "rnas.qh"      , DC_VD_VT       },
 { "rnes.qh"      , DC_VD_VT       },
 { "invalid"      , DC_BARE        },

 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },

 { "mul.qh"       , DC_VD_VS_VT_VEC},
 { "invalid"      , DC_BARE        },
 { "$21"      , DC_DEREF       },
 { "$22"      , DC_DEREF       },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "$23"      , DC_DEREF       },
 { "$24"      , DC_DEREF       },

 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "$25"      , DC_DEREF       },
 { "$26"      , DC_DEREF       },
};

static const disasm_t disasm_mdmx_ob[64] DATASEG = 
{{ "invalid"      , DC_BARE        },
 { "c.eq.ob"      , DC_VS_VT_VEC   },
 { "pickf.ob"     , DC_VD_VS_VT_VEC},
 { "pickt.ob"     , DC_VD_VS_VT_VEC},
 { "c.lt.ob"      , DC_VS_VT_VEC   },
 { "c.le.ob"      , DC_VS_VT_VEC   },
 { "min.ob"       , DC_VD_VS_VT_VEC},
 { "max.ob"       , DC_VD_VS_VT_VEC},

 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "sub.ob"       , DC_VD_VS_VT_VEC},
 { "add.ob"       , DC_VD_VS_VT_VEC},
 { "and.ob"       , DC_VD_VS_VT_VEC},
 { "xor.ob"       , DC_VD_VS_VT_VEC},
 { "or.ob"        , DC_VD_VS_VT_VEC},
 { "nor.ob"       , DC_VD_VS_VT_VEC},

 { "sll.ob"       , DC_VD_VS_VT_VEC},
 { "invalid"      , DC_BARE        },
 { "srl.ob"       , DC_VD_VS_VT_VEC},
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },

 { "alni.ob"      , DC_VD_VS_VT_IMM},
 { "alnv.ob"      , DC_VD_VS_VT_RS },
 { "alni.qh"      , DC_VD_VS_VT_IMM},
 { "alnv.qh"      , DC_VD_VS_VT_RS },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "$35"      , DC_DEREF       },

 { "rzu.ob"       , DC_VD_VT       },
 { "rnau.ob"      , DC_VD_VT       },
 { "rneu.ob"      , DC_VD_VT       },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },

 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },

 { "mul.ob"       , DC_VD_VS_VT_VEC},
 { "invalid"      , DC_BARE        },
 { "$27"      , DC_DEREF       },
 { "$28"      , DC_DEREF       },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "$29"      , DC_DEREF       },
 { "$30"      , DC_DEREF       },

 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "$31"      , DC_DEREF       },
 { "$32"      , DC_DEREF       },
};

static const disasm_t disasm_mdmx_alni[64] DATASEG = 
{{ "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },

 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },

 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },

 { "alni.ob"      , DC_VD_VS_VT_IMM},
 { "alnv.ob"      , DC_VD_VS_VT_RS },
 { "alni.qh"      , DC_VD_VS_VT_IMM},
 { "alnv.qh"      , DC_VD_VS_VT_RS },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },

 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },

 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },

 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },

 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
};

static const disasm_t disasm_mdmx_muls_qh[2] DATASEG = 
{{ "muls.qh"      , DC_VS_VT_VEC   },
 { "mulsl.qh"     , DC_VS_VT_VEC   },
};

static const disasm_t disasm_mdmx_mul_qh[2] DATASEG = 
{{ "mula.qh"      , DC_VS_VT_VEC   },
 { "mull.qh"      , DC_VS_VT_VEC   },
};

static const disasm_t disasm_mdmx_sub_qh[2] DATASEG = 
{{ "suba.qh"      , DC_VS_VT_VEC   },
 { "subl.qh"      , DC_VS_VT_VEC   },
};

static const disasm_t disasm_mdmx_add_qh[2] DATASEG =
{{ "adda.qh"      , DC_VS_VT_VEC   },
 { "addl.qh"      , DC_VS_VT_VEC   },
};

static const disasm_t disasm_mdmx_wac_qh[4] DATASEG = 
{{ "wacl.qh"      , DC_VS_VT       },
 { "invalid"      , DC_BARE        },
 { "wach.qh"      , DC_VS          },
 { "invalid"      , DC_BARE        },
};

static const disasm_t disasm_mdmx_rac_qh[4] DATASEG = 
{{ "racl.qh"      , DC_VD          },
 { "racm.qh"      , DC_VD          },
 { "rach.qh"      , DC_VD          },
 { "invalid"      , DC_BARE        },
};

static const disasm_t disasm_mdmx_muls_ob[2] DATASEG = 
{{ "muls.ob"      , DC_VS_VT_VEC   },
 { "mulsl.ob"     , DC_VS_VT_VEC   },
};

static const disasm_t disasm_mdmx_mul_ob[2] DATASEG = 
{{ "mula.ob"      , DC_VS_VT_VEC   },
 { "mull.ob"      , DC_VS_VT_VEC   },
};

static const disasm_t disasm_mdmx_sub_ob[2] DATASEG = 
{{ "suba.ob"      , DC_VS_VT_VEC   },
 { "subl.ob"      , DC_VS_VT_VEC   },
};

static const disasm_t disasm_mdmx_add_ob[2] DATASEG =
{{ "adda.ob"      , DC_VS_VT_VEC   },
 { "addl.ob"      , DC_VS_VT_VEC   },
};

static const disasm_t disasm_mdmx_wac_ob[4] DATASEG = 
{{ "wacl.ob"      , DC_VS_VT       },
 { "invalid"      , DC_BARE        },
 { "wach.ob"      , DC_VS          },
 { "invalid"      , DC_BARE        },
};

static const disasm_t disasm_mdmx_rac_ob[4] DATASEG = 
{{ "racl.ob"      , DC_VD          },
 { "racm.ob"      , DC_VD          },
 { "rach.ob"      , DC_VD          },
 { "invalid"      , DC_BARE        },
};

static const disasm_t disasm_mdmx_shfl_ob[16] DATASEG = 
{{ "shfl.upsl.ob" , DC_VD_VS_VT    },
 { "shfl.pach.ob" , DC_VD_VS_VT    },
 { "shfl.mixh.ob" , DC_VD_VS_VT    },
 { "shfl.mixl.ob" , DC_VD_VS_VT    },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
};

static const disasm_t disasm_mdmx_shfl_qh[8] DATASEG =  
{{ "shfl.bfla.qh" , DC_VD_VS_VT    },
 { "shfl.pach.qh" , DC_VD_VS_VT    },
 { "shfl.mixh.qh" , DC_VD_VS_VT    },
 { "shfl.mixl.qh" , DC_VD_VS_VT    },
 { "shfl.repa.qh" , DC_VD_VS_VT    },
 { "shfl.repb.qh" , DC_VD_VS_VT    },
 { "invalid"      , DC_BARE        },
 { "invalid"      , DC_BARE        },
};



const disasm_deref_t disasm_deref[] = 
/* Disasm array           shft   msk */
{{ disasm_normal      ,     26, 0x3f },   /*  0 */
 { disasm_special     ,      0, 0x3f },   /*  1 */
 { disasm_regimm      ,     16, 0x1f },   /*  2 */
 { disasm_spec2       ,      0, 0x3f },   /*  3 */
 { disasm_cop0        ,     21, 0x1f },   /*  4 */
 { disasm_cop0_c0     ,      0, 0x3f },   /*  5 */
 { disasm_cop1        ,     21, 0x1f },   /*  6 */
 { disasm_cop1_s      ,      0, 0x3f },   /*  7 */
 { disasm_cop1_s_mvcf ,     16,  0x1 },   /*  8 */
 { disasm_cop1_d      ,      0, 0x3f },   /*  9 */
 { disasm_cop1_d_mvcf ,     16,  0x1 },   /* 10 */
 { disasm_cop1_w      ,      0, 0x3f },   /* 11 */
 { disasm_cop1_l      ,      0, 0x3f },   /* 12 */
 { disasm_cop1_ps     ,      0, 0x3f },   /* 13 */
 { disasm_cop1_ps_mvcf,     16,  0x1 },   /* 14 */
 { disasm_cop1x       ,      0, 0x3f },   /* 15 */
 { disasm_movci       ,     16,  0x1 },   /* 16 */
 { disasm_cop1_bc1    ,     16,  0x3 },   /* 17 */
 { disasm_mdmx        ,     21,  0x7 },   /* 18 */
 { disasm_mdmx_qh     ,      0, 0x3f },   /* 19 */
 { disasm_mdmx_ob     ,      0, 0x3f },   /* 20 */
 { disasm_mdmx_muls_qh,     10,  0x1 },   /* 21 */
 { disasm_mdmx_mul_qh ,     10,  0x1 },   /* 22 */
 { disasm_mdmx_sub_qh ,     10,  0x1 },   /* 23 */
 { disasm_mdmx_add_qh ,     10,  0x1 },   /* 24 */
 { disasm_mdmx_wac_qh ,     24,  0x3 },   /* 25 */
 { disasm_mdmx_rac_qh ,     24,  0x3 },   /* 26 */
 { disasm_mdmx_muls_ob,     10,  0x1 },   /* 27 */
 { disasm_mdmx_mul_ob ,     10,  0x1 },   /* 28 */
 { disasm_mdmx_sub_ob ,     10,  0x1 },   /* 29 */
 { disasm_mdmx_add_ob ,     10,  0x1 },   /* 30 */
 { disasm_mdmx_wac_ob ,     24,  0x3 },   /* 31 */
 { disasm_mdmx_rac_ob ,     24,  0x3 },   /* 32 */
 { disasm_mdmx_alni   ,      0, 0x3f },   /* 33 */
 { disasm_mdmx_shfl_ob,     22,  0xf },   /* 34 */
 { disasm_mdmx_shfl_qh,     23,  0x7 },   /* 35 */
 { disasm_cop1_bc1any2,     16,  0x1 },   /* 36 */
 { disasm_cop1_bc1any4,     16,  0x1 },   /* 37 */
 { disasm_cop1_c_f_s  ,      6,  0x1 },   /* 38 */
 { disasm_cop1_c_un_s ,      6,  0x1 },   /* 39 */
 { disasm_cop1_c_eq_s ,      6,  0x1 },   /* 40 */
 { disasm_cop1_c_ueq_s,      6,  0x1 },   /* 41 */
 { disasm_cop1_c_olt_s,      6,  0x1 },   /* 42 */
 { disasm_cop1_c_ult_s,      6,  0x1 },   /* 43 */
 { disasm_cop1_c_ole_s,      6,  0x1 },   /* 44 */
 { disasm_cop1_c_ule_s,      6,  0x1 },   /* 45 */
 { disasm_cop1_c_sf_s ,      6,  0x1 },   /* 46 */
 { disasm_cop1_c_ngle_s,     6,  0x1 },   /* 47 */
 { disasm_cop1_c_seq_s,      6,  0x1 },   /* 48 */
 { disasm_cop1_c_ngl_s,      6,  0x1 },   /* 49 */
 { disasm_cop1_c_lt_s ,      6,  0x1 },   /* 50 */
 { disasm_cop1_c_nge_s,      6,  0x1 },   /* 51 */
 { disasm_cop1_c_le_s ,      6,  0x1 },   /* 52 */
 { disasm_cop1_c_ngt_s,      6,  0x1 },   /* 53 */
 { disasm_cop1_c_f_d  ,      6,  0x1 },   /* 54 */
 { disasm_cop1_c_un_d ,      6,  0x1 },   /* 55 */
 { disasm_cop1_c_eq_d ,      6,  0x1 },   /* 56 */
 { disasm_cop1_c_ueq_d,      6,  0x1 },   /* 57 */
 { disasm_cop1_c_olt_d,      6,  0x1 },   /* 58 */
 { disasm_cop1_c_ult_d,      6,  0x1 },   /* 59 */
 { disasm_cop1_c_ole_d,      6,  0x1 },   /* 60 */
 { disasm_cop1_c_ule_d,      6,  0x1 },   /* 61 */
 { disasm_cop1_c_sf_d ,      6,  0x1 },   /* 62 */
 { disasm_cop1_c_ngle_d,     6,  0x1 },   /* 63 */
 { disasm_cop1_c_seq_d,      6,  0x1 },   /* 64 */
 { disasm_cop1_c_ngl_d,      6,  0x1 },   /* 65 */
 { disasm_cop1_c_lt_d ,      6,  0x1 },   /* 66 */
 { disasm_cop1_c_nge_d,      6,  0x1 },   /* 67 */
 { disasm_cop1_c_le_d ,      6,  0x1 },   /* 68 */
 { disasm_cop1_c_ngt_d,      6,  0x1 },   /* 69 */
 { disasm_cop1_c_f_ps ,      6,  0x1 },   /* 70 */
 { disasm_cop1_c_un_ps,      6,  0x1 },   /* 71 */
 { disasm_cop1_c_eq_ps,      6,  0x1 },   /* 72 */
 { disasm_cop1_c_ueq_ps,     6,  0x1 },   /* 73 */
 { disasm_cop1_c_olt_ps,     6,  0x1 },   /* 74 */
 { disasm_cop1_c_ult_ps,     6,  0x1 },   /* 75 */
 { disasm_cop1_c_ole_ps,     6,  0x1 },   /* 76 */
 { disasm_cop1_c_ule_ps,     6,  0x1 },   /* 77 */
 { disasm_cop1_c_sf_ps,      6,  0x1 },   /* 78 */
 { disasm_cop1_c_ngle_ps,    6,  0x1 },   /* 79 */
 { disasm_cop1_c_seq_ps,     6,  0x1 },   /* 80 */
 { disasm_cop1_c_ngl_ps,     6,  0x1 },   /* 81 */
 { disasm_cop1_c_lt_ps,      6,  0x1 },   /* 82 */
 { disasm_cop1_c_nge_ps,     6,  0x1 },   /* 83 */
 { disasm_cop1_c_le_ps,      6,  0x1 },   /* 84 */
 { disasm_cop1_c_ngt_ps,     6,  0x1 },   /* 85 */
};

#define PREFHINT(x) (&pref_hints[(x)*9])
static char *pref_hints =
  "load    \0"                
  "store   \0"               
  "reserved\0"            
  "reserved\0"            
 
  "ld_strm \0"       
  "st_strm \0"      
  "ld_retn \0"       
  "st_retn \0"      
 
  "reserved\0"            
  "reserved\0"            
  "reserved\0"            
  "reserved\0"            
 
  "reserved\0"            
  "reserved\0"            
  "reserved\0"            
  "reserved\0"            
 
  "reserved\0"            
  "reserved\0"            
  "reserved\0"            
  "reserved\0"            
 
  "reserved\0"            
  "reserved\0"            
  "reserved\0"            
  "reserved\0"            
 
  "reserved\0"            
  "wb_inval\0"
  "reserved\0"            
  "reserved\0"            
 
  "reserved\0"            
  "reserved\0"            
  "reserved\0"            
  "reserved\0";


static int snprintf(char *buf,int len,const char *templat,...)
{
    va_list marker;
    int count;

    va_start(marker,templat);
    count = xvsprintf(buf,templat,marker);
    va_end(marker);

    return count;
}

static const disasm_t *get_disasm_field(uint32_t inst)
{
	const disasm_deref_t *tmp = &disasm_deref[0];
	const disasm_t *rec;
	do {
		rec = &(tmp->ptr[(inst>>tmp->shift) & tmp->mask]);
		tmp = &disasm_deref[atoi(&(rec->name[1]))];
	} while (rec->type == DC_DEREF);
	return rec;
}

char *disasm_inst_name(uint32_t inst)
{
	return (char *)(get_disasm_field(inst)->name);
}

void disasm_inst(char *buf, int buf_size, uint32_t inst, uint64_t pc)
{
	const disasm_t *tmp;
	char instname[32];
	int commentmode = 0;
	char *x;

	tmp = get_disasm_field(inst);

	strcpy(instname,(char *) tmp->name);

	if ((x = strchr(instname,'@'))) {
	    *x++ = 0;
	    commentmode = atoi(x);
	    }

	switch (tmp->type) {
	case DC_RD_RS_RT:
		snprintf(buf, buf_size, "%-8s %s,%s,%s",
			 instname, 
			 REGNAME((inst>>11) & 0x1f),
			 REGNAME((inst>>21) & 0x1f),
			 REGNAME((inst>>16) & 0x1f));
		break;
	case DC_RD_RT_RS:
		snprintf(buf, buf_size, "%-8s %s,%s,%s",
			 instname,
			 REGNAME((inst>>11) & 0x1f),
			 REGNAME((inst>>16) & 0x1f),
			 REGNAME((inst>>21) & 0x1f));
		break;
	case DC_RT_RS_SIMM:
		snprintf(buf, buf_size, "%-8s %s,%s,#%" PF_32 "d",
			 instname,
			 REGNAME((inst>>16) & 0x1f),
			 REGNAME((inst>>21) & 0x1f),
			 SEXT_32(15, inst & 0xffff));
		break;
	case DC_RT_RS_XIMM:
		snprintf(buf, buf_size, "%-8s %s,%s,#0x%" PF_32 "x",
			 instname,
			 REGNAME((inst>>16) & 0x1f),
			 REGNAME((inst>>21) & 0x1f),
			 inst & 0xffff);
		break;
	case DC_RS_RT_OFS:
		snprintf(buf, buf_size, "%-8s %s,%s,0x%" PF_64 "x",
			 instname,
			 REGNAME((inst>>21) & 0x1f),
			 REGNAME((inst>>16) & 0x1f),
			 pc + 4 + (SEXT_64(15, inst & 0xffff)<<2));
		break;
	case DC_RS_OFS:
		snprintf(buf, buf_size, "%-8s %s,0x%" PF_64 "x",
			 instname, 
			 REGNAME((inst>>21) & 0x1f),
			 pc + 4 + (SEXT_64(16, inst & 0xffff)<<2));
		break;
	case DC_RD_RT_SA:
		snprintf(buf, buf_size, "%-8s %s,%s,#%d",
			 instname, 
			 REGNAME((inst>>11) & 0x1f),
			 REGNAME((inst>>16) & 0x1f),
			 (inst>>6) & 0x1f);
		break;
	case DC_RT_UIMM:
		snprintf(buf, buf_size, "%-8s %s,#%d",
			 instname,
			 REGNAME((inst>>16) & 0x1f),
			 inst & 0xffff);
		break;
	case DC_RD:
		snprintf(buf, buf_size, "%-8s %s",
			 instname,
			 REGNAME((inst>>11) & 0x1f));
		break;
	case DC_J:
		snprintf(buf, buf_size, "%-8s 0x%" PF_64 "x",
			 instname, 
			 (pc & UINT64_T(0xfffffffff0000000)) | ((inst & 0x3ffffff)<<2));
		break;
	case DC_RD_RS:
		snprintf(buf, buf_size, "%-8s %s,%s",
			 instname,
			 REGNAME((inst>>11) & 0x1f),
			 REGNAME((inst>>21) & 0x1f));
		break;
	case DC_RS_RT:
		snprintf(buf, buf_size, "%-8s %s,%s",
			 instname,
			 REGNAME((inst>>21) & 0x1f),
			 REGNAME((inst>>16) & 0x1f));
		break;
	case DC_RT_RS:
		snprintf(buf, buf_size, "%-8s %s,%s",
			 instname,
			 REGNAME((inst>>16) & 0x1f),
			 REGNAME((inst>>21) & 0x1f));
		break;
	case DC_RT_RD_SEL:
		snprintf(buf, buf_size, "%-8s %s,%s,#%d",
			 instname, 
			 REGNAME((inst>>16) & 0x1f),
			 REGNAME((inst>>11) & 0x1f),
			 inst & 0x3);
		break;
	case DC_RT_CR_SEL:
		snprintf(buf, buf_size, "%-8s %s,%d,#%d",
			 instname, 
			 REGNAME((inst>>16) & 0x1f),
			 (inst>>11) & 0x1f,
			 inst & 0x3);
		break;
	case DC_RS:
		snprintf(buf, buf_size, "%-8s %s",
			 instname,
			 REGNAME((inst>>21) & 0x1f));
		break;
	case DC_RS_SIMM:
		snprintf(buf, buf_size, "%-8s %s,#%" PF_32 "d",
			 instname,
			 REGNAME((inst>>21) & 0x1f),
			 SEXT_32(15, inst & 0xffff));
		break;
	case DC_RT_OFS_BASE:
		snprintf(buf, buf_size, "%-8s %s,#%" PF_32 "d(%s)",
			 instname,
			 REGNAME((inst>>16) & 0x1f),
			 SEXT_32(15, inst),
			 REGNAME((inst>>21) & 0x1f));
		break;
	case DC_FT_OFS_BASE:
		snprintf(buf, buf_size, "%-8s f%d,#%" PF_32 "d(%s)",
			 instname,
			 (inst>>16) & 0x1f,
			 SEXT_32(15, inst),
			 REGNAME((inst>>21) & 0x1f));
		break;
	case DC_FD_IDX_BASE:
		snprintf(buf, buf_size, "%-8s f%d,%s(%s)",
			 instname,
			 (inst>>6) & 0x1f,
			 REGNAME((inst>>16) & 0x1f),
			 REGNAME((inst>>21) & 0x1f));
		break;
	case DC_FS_IDX_BASE:
		snprintf(buf, buf_size, "%-8s f%d,%s(%s)",
			 instname,
			 (inst>>11) & 0x1f,
			 REGNAME((inst>>16) & 0x1f),
			 REGNAME((inst>>21) & 0x1f));
		break;
	case DC_FD_FS_FT:
		snprintf(buf, buf_size, "%-8s f%d,f%d,f%d",
			 instname,
			 (inst>>6) & 0x1f,
			 (inst>>11) & 0x1f,
			 (inst>>16) & 0x1f);
		break;
	case DC_FD_FS_RT:
		snprintf(buf, buf_size, "%-8s f%d,f%d,%s",
			 instname,
			 (inst>>6) & 0x1f,
			 (inst>>11) & 0x1f,
			 REGNAME((inst>>16) & 0x1f));
		break;
	case DC_FD_FS:
		snprintf(buf, buf_size, "%-8s f%d,f%d", 
			 instname, 
			 (inst>>6)&0x1f, 
			 (inst>>11)&0x1f);
		break;
	case DC_PREF_OFS:
  	        snprintf(buf, buf_size, "%-8s #%" PF_32 "d(%s)  /* %s */",
			 instname,
			 SEXT_32(15, inst & 0xffff),
			 REGNAME((inst>>21) & 0x1f),
			 PREFHINT((inst>>16) & 0x1f));
		break;
	case DC_PREF_IDX:
   	        snprintf(buf, buf_size, "%-8s %s(%s)  /* %s */",
			 instname,
			 REGNAME((inst>>16) & 0x1f),
			 REGNAME((inst>>21) & 0x1f),
			 PREFHINT((inst>>16) & 0x1f));
		break;
	case DC_CC_OFS:
		snprintf(buf, buf_size, "%-8s %d,0x%" PF_64 "x", 
			 instname,
			 (inst>>18) & 0x7,
			 pc + 4 + (SEXT_64(15, inst & 0xffff)<<2));
		break;
	case DC_RD_RS_CC:
		snprintf(buf, buf_size, "%-8s %s,%s,%d",
			 instname,
			 REGNAME((inst>>11) & 0x1f),
			 REGNAME((inst>>21) & 0x1f),
			 (inst>>18) & 0x7);
		break;
	case DC_FD_FS_CC:
		snprintf(buf, buf_size, "%-8s f%d,f%d,%d",
			 instname,
			 (inst>>6) & 0x1f,
			 (inst>>11) & 0x1f,
			 (inst>>18) & 0x7);
		break;
	case DC_FD_FR_FS_FT:
		snprintf(buf, buf_size, "%-8s f%d,f%d,f%d,f%d",
			 instname,
			 (inst>>6) & 0x1f,
			 (inst>>21) & 0x1f,
			 (inst>>11) & 0x1f,
			 (inst>>16) & 0x1f);
		break;
	case DC_FD_FS_FT_RS:
		snprintf(buf, buf_size, "%-8s f%d,f%d,f%d,%s",
			 instname,
			 (inst>>6) & 0x1f,
			 (inst>>11) & 0x1f,
			 (inst>>16) & 0x1f,
			 REGNAME((inst>>21) & 0x1f));
		break;
	case DC_CC_FS_FT:
		snprintf(buf, buf_size, "%-8s %d,f%d,f%d", 
			 instname,
			 (inst>>8) & 0x7,
			 (inst>>11) & 0x1f,
			 (inst>>16) & 0x1f);
		break;
	case DC_BARE:
		snprintf(buf, buf_size, "%-8s", instname);
		break;
	case DC_RT_FS:
		snprintf(buf, buf_size, "%-8s %s,f%d",
			 instname,
			 REGNAME((inst>>16) & 0x1f),
			 (inst>>11) & 0x1f);
		break;
	case DC_VS:
		snprintf(buf, buf_size, "%-8s $v%d",
			 instname,
			 (inst>>11) & 0x1f);
		break;
	case DC_VD:
		snprintf(buf, buf_size, "%-8s $v%d",
			 instname,
			 (inst>>6) & 0x1f);
		break;
	case DC_VD_VT:
		snprintf(buf, buf_size, "%-8s $v%d,$v%d",
			 instname, 
			 (inst>>6) & 0x1f,
			 (inst>>16) & 0x1f);
		break;
	case DC_VD_VS_VT_IMM:
		snprintf(buf, buf_size, "%-8s $v%d,$v%d,$v%d,#%d",
			 instname, 
			 (inst>>6) & 0x1f,
			 (inst>>11) & 0x1f,
			 (inst>>16) & 0x1f,
			 (inst>>21) & 0x7);
		break;
	case DC_VD_VS_VT_RS:
		snprintf(buf, buf_size, "%-8s $v%d,$v%d,$v%d,%s",
			 instname,
			 (inst>>6) & 0x1f,
			 (inst>>11) & 0x1f,
			 (inst>>16) & 0x1f,
			 REGNAME((inst>>21) & 0x1f));
		break;
	case DC_VD_VS_VT:
		snprintf(buf, buf_size, "%-8s $v%d,$v%d,$v%d",
			 instname,
			 (inst>>6) & 0x1f,
			 (inst>>11) & 0x1f,
			 (inst>>16) & 0x1f);
		break;
	case DC_VS_VT:
		snprintf(buf, buf_size, "%-8s $v%d,$v%d",
			 instname,
			 (inst>>11) & 0x1f,
			 (inst>>16) & 0x1f);
		break;
	case DC_VS_VT_VEC:
		switch((inst>>24) & 0x3) {
		case 0:
		case 1:
			/* element select */
			if ((inst>>21) & 1) {
				/* QH */
				snprintf(buf, buf_size, "%-8s $v%d,$v%d[%d]",
					 instname,
					 (inst>>11) & 0x1f,
					 (inst>>16) & 0x1f,
					 (inst>>23) & 0x3);
			} else {
				/* OB */
				snprintf(buf, buf_size, "%-8s $v%d,$v%d[%d]",
					 instname,
					 (inst>>11) & 0x1f,
					 (inst>>16) & 0x1f,
					 (inst>>22) & 0x7);
			
			}
			break;
		case 2:
			/* Vector select */
			snprintf(buf, buf_size, "%-8s $v%d,$v%d",
				 instname, 
				 (inst>>11) & 0x1f,
				 (inst>>16) & 0x1f);
			break;
		case 3:
			/* immediate select */
			snprintf(buf, buf_size, "%-8s $v%d,$#%d",
				 instname, 
				 (inst>>11) & 0x1f,
				 (inst>>16) & 0x1f);
			break;
		}
		break;
		
	case DC_VD_VS_VT_VEC:
		switch((inst>>24) & 0x3) {
		case 0:
		case 1:
			/* element select */
			if ((inst>>21) & 1) {
				/* QH */
				snprintf(buf, buf_size, "%-8s $v%d,$v%d,$v%d[%d]",
					 instname,
					 (inst>>6) & 0x1f,
					 (inst>>11) & 0x1f,
					 (inst>>16) & 0x1f,
					 (inst>>23) & 0x3);
			} else {
				/* OB */
				snprintf(buf, buf_size, "%-8s $v%d,$v%d,$v%d[%d]",
					 instname,
					 (inst>>6) & 0x1f,
					 (inst>>11) & 0x1f,
					 (inst>>16) & 0x1f,
					 (inst>>22) & 0x7);
			
			}
			break;
		case 2:
			/* Vector select */
			snprintf(buf, buf_size, "%-8s $v%d,$v%d,$v%d",
				 instname, 
				 (inst>>6) & 0x1f,
				 (inst>>11) & 0x1f,
				 (inst>>16) & 0x1f);
			break;
		case 3:
			/* immediate select */
			snprintf(buf, buf_size, "%-8s $v%d,$v%d,$#%d",
				 instname, 
				 (inst>>6) & 0x1f,
				 (inst>>11) & 0x1f,
				 (inst>>16) & 0x1f);
			break;
		}
		break;

	case DC_SYSCALL:
		snprintf(buf, buf_size, "%-8s #%d",
			 instname,
			 (inst>>6) & 0xfffff);
		break;
	case DC_BREAK:
		snprintf(buf, buf_size, "%-8s %d", instname, (inst>>6)&0xfffff);
		break;
	case DC_OOPS:
		snprintf(buf, buf_size, "%s OOPS!  FIXME!", instname);
		break;
	default:
		/* Hit something we don't know about...Shouldn't happen. */
	    break;
	}

	/*
	 * Handle comment field
	 */


	switch (commentmode) {
	    case 1:		/* CP0 ops */
		if ((inst & 3) == 0) {		/* select 0 */
		    snprintf(buf + strlen(buf),buf_size-strlen(buf),"   /* %s */",
			     CP0REGNAME((inst >> 11) & 0x1f));
		    }
		break;
	    default:
		break;			 
	    }

	buf[buf_size-1] = 0;

}
