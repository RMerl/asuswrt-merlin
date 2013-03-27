/*  dbg_rdi.h -- ARMulator RDI interface:  ARM6 Instruction Emulator.
    Copyright (C) 1994 Advanced RISC Machines Ltd.
 
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA. */

#ifndef dbg_rdi__h
#define dbg_rdi__h

/***************************************************************************\
*                              Error Codes                                  *
\***************************************************************************/

#define RDIError_NoError                0

#define RDIError_Reset                  1
#define RDIError_UndefinedInstruction   2
#define RDIError_SoftwareInterrupt      3
#define RDIError_PrefetchAbort          4
#define RDIError_DataAbort              5
#define RDIError_AddressException       6
#define RDIError_IRQ                    7
#define RDIError_FIQ                    8
#define RDIError_Error                  9
#define RDIError_BranchThrough0         10

#define RDIError_NotInitialised         128
#define RDIError_UnableToInitialise     129
#define RDIError_WrongByteSex           130
#define RDIError_UnableToTerminate      131
#define RDIError_BadInstruction         132
#define RDIError_IllegalInstruction     133
#define RDIError_BadCPUStateSetting     134
#define RDIError_UnknownCoPro           135
#define RDIError_UnknownCoProState      136
#define RDIError_BadCoProState          137
#define RDIError_BadPointType           138
#define RDIError_UnimplementedType      139
#define RDIError_BadPointSize           140
#define RDIError_UnimplementedSize      141
#define RDIError_NoMorePoints           142
#define RDIError_BreakpointReached      143
#define RDIError_WatchpointAccessed     144
#define RDIError_NoSuchPoint            145
#define RDIError_ProgramFinishedInStep  146
#define RDIError_UserInterrupt          147
#define RDIError_CantSetPoint           148
#define RDIError_IncompatibleRDILevels  149

#define RDIError_CantLoadConfig         150
#define RDIError_BadConfigData          151
#define RDIError_NoSuchConfig           152
#define RDIError_BufferFull             153
#define RDIError_OutOfStore             154
#define RDIError_NotInDownload          155
#define RDIError_PointInUse             156
#define RDIError_BadImageFormat         157
#define RDIError_TargetRunning          158

#define RDIError_LittleEndian           240
#define RDIError_BigEndian              241
#define RDIError_SoftInitialiseError    242

#define RDIError_InsufficientPrivilege  253
#define RDIError_UnimplementedMessage   254
#define RDIError_UndefinedMessage       255

/***************************************************************************\
*                          RDP Message Numbers                              *
\***************************************************************************/

#define RDP_Start               (unsigned char)0x0
#define RDP_End                 (unsigned char)0x1
#define RDP_Read                (unsigned char)0x2
#define RDP_Write               (unsigned char)0x3
#define RDP_CPUread             (unsigned char)0x4
#define RDP_CPUwrite            (unsigned char)0x5
#define RDP_CPread              (unsigned char)0x6
#define RDP_CPwrite             (unsigned char)0x7
#define RDP_SetBreak            (unsigned char)0xa
#define RDP_ClearBreak          (unsigned char)0xb
#define RDP_SetWatch            (unsigned char)0xc
#define RDP_ClearWatch          (unsigned char)0xd
#define RDP_Execute             (unsigned char)0x10
#define RDP_Step                (unsigned char)0x11
#define RDP_Info                (unsigned char)0x12
#define RDP_OSOpReply           (unsigned char)0x13

#define RDP_AddConfig           (unsigned char)0x14
#define RDP_LoadConfigData      (unsigned char)0x15
#define RDP_SelectConfig        (unsigned char)0x16
#define RDP_LoadAgent           (unsigned char)0x17

#define RDP_Stopped             (unsigned char)0x20
#define RDP_OSOp                (unsigned char)0x21
#define RDP_Fatal               (unsigned char)0x5e
#define RDP_Return              (unsigned char)0x5f
#define RDP_Reset               (unsigned char)0x7f

/***************************************************************************\
*                            Other RDI values                               *
\***************************************************************************/

#define RDISex_Little           0	/* the byte sex of the debuggee */
#define RDISex_Big              1
#define RDISex_DontCare         2

#define RDIPoint_EQ             0	/* the different types of break/watchpoints */
#define RDIPoint_GT             1
#define RDIPoint_GE             2
#define RDIPoint_LT             3
#define RDIPoint_LE             4
#define RDIPoint_IN             5
#define RDIPoint_OUT            6
#define RDIPoint_MASK           7

#define RDIPoint_Inquiry        64	/* ORRed with point type in extended RDP */
#define RDIPoint_Handle         128	/* messages                              */

#define RDIWatch_ByteRead       1	/* types of data accesses to watch for */
#define RDIWatch_HalfRead       2
#define RDIWatch_WordRead       4
#define RDIWatch_ByteWrite      8
#define RDIWatch_HalfWrite      16
#define RDIWatch_WordWrite      32

#define RDIReg_R15              (1L << 15)	/* mask values for CPU */
#define RDIReg_PC               (1L << 16)
#define RDIReg_CPSR             (1L << 17)
#define RDIReg_SPSR             (1L << 18)
#define RDINumCPURegs           19

#define RDINumCPRegs            10	/* current maximum */

#define RDIMode_Curr            255

/* Bits set in return value from RDIInfo_Target */
#define RDITarget_LogSpeed              0x0f
#define RDITarget_HW                    0x10	/* else emulator */
#define RDITarget_AgentMaxLevel         0xe0
#define RDITarget_AgentLevelShift       5
#define RDITarget_DebuggerMinLevel      0x700
#define RDITarget_DebuggerLevelShift    8
#define RDITarget_CanReloadAgent        0x800
#define RDITarget_CanInquireLoadSize    0x1000

/* Bits set in return value from RDIInfo_Step */
#define RDIStep_Multiple                1
#define RDIStep_PCChange                2
#define RDIStep_Single                  4

/* Bits set in return value from RDIInfo_Points */
#define RDIPointCapability_Comparison   1
#define RDIPointCapability_Range        2
/* 4 to 128 are RDIWatch_xx{Read,Write} left-shifted by two */
#define RDIPointCapability_Mask         256
#define RDIPointCapability_Status       512	/* Point status enquiries available */

/* RDI_Info subcodes */
#define RDIInfo_Target          0
#define RDIInfo_Points          1
#define RDIInfo_Step            2
#define RDIInfo_MMU             3
#define RDIInfo_DownLoad        4	/* Inquires whether configuration download
					   and selection is available.
					 */
#define RDIInfo_SemiHosting     5	/* Inquires whether RDISemiHosting_* RDI_Info
					   calls are available.
					 */
#define RDIInfo_CoPro           6	/* Inquires whether CoPro RDI_Info calls are
					   available.
					 */
#define RDIInfo_Icebreaker      7

/* The next two are only to be used if the value returned by RDIInfo_Points */
/* has RDIPointCapability_Status set.                                       */
#define RDIPointStatus_Watch    0x80
#define RDIPointStatus_Break    0x81

#define RDISignal_Stop          0x100

#define RDIVector_Catch         0x180

/* The next four are only to be used if RDIInfo_Semihosting returned no error */
#define RDISemiHosting_SetState 0x181
#define RDISemiHosting_GetState 0x182
#define RDISemiHosting_SetVector 0x183
#define RDISemiHosting_GetVector 0x184

/* The next two are only to be used if RDIInfo_Icebreaker returned no error */
#define RDIIcebreaker_GetLocks  0x185
#define RDIIcebreaker_SetLocks  0x186

/* Only if RDIInfo_Target returned RDITarget_CanInquireLoadSize */
#define RDIInfo_GetLoadSize     0x187

#define RDICycles               0x200
#define RDICycles_Size          48
#define RDIErrorP               0x201

#define RDISet_Cmdline          0x300
#define RDISet_RDILevel         0x301
#define RDISet_Thread           0x302

/* The next two are only to be used if RDIInfo_CoPro returned no error */
#define RDIInfo_DescribeCoPro   0x400
#define RDIInfo_RequestCoProDesc 0x401

#define RDIInfo_Log             0x800
#define RDIInfo_SetLog          0x801

typedef unsigned long PointHandle;
typedef unsigned long ThreadHandle;
#define RDINoPointHandle        ((PointHandle)-1L)
#define RDINoHandle             ((ThreadHandle)-1L)

struct Dbg_ConfigBlock;
struct Dbg_HostosInterface;
struct Dbg_MCState;
typedef int rdi_open_proc (unsigned type,
			   struct Dbg_ConfigBlock const *config,
			   struct Dbg_HostosInterface const *i,
			   struct Dbg_MCState *dbg_state);
typedef int rdi_close_proc (void);
typedef int rdi_read_proc (ARMword source, void *dest, unsigned *nbytes);
typedef int rdi_write_proc (const void *source, ARMword dest,
			    unsigned *nbytes);
typedef int rdi_CPUread_proc (unsigned mode, unsigned long mask,
			      ARMword * state);
typedef int rdi_CPUwrite_proc (unsigned mode, unsigned long mask,
			       ARMword const *state);
typedef int rdi_CPread_proc (unsigned CPnum, unsigned long mask,
			     ARMword * state);
typedef int rdi_CPwrite_proc (unsigned CPnum, unsigned long mask,
			      ARMword const *state);
typedef int rdi_setbreak_proc (ARMword address, unsigned type, ARMword bound,
			       PointHandle * handle);
typedef int rdi_clearbreak_proc (PointHandle handle);
typedef int rdi_setwatch_proc (ARMword address, unsigned type,
			       unsigned datatype, ARMword bound,
			       PointHandle * handle);
typedef int rdi_clearwatch_proc (PointHandle handle);
typedef int rdi_execute_proc (PointHandle * handle);
typedef int rdi_step_proc (unsigned ninstr, PointHandle * handle);
typedef int rdi_info_proc (unsigned type, ARMword * arg1, ARMword * arg2);
typedef int rdi_pointinq_proc (ARMword * address, unsigned type,
			       unsigned datatype, ARMword * bound);

typedef enum
{
  RDI_ConfigCPU,
  RDI_ConfigSystem
}
RDI_ConfigAspect;

typedef enum
{
  RDI_MatchAny,
  RDI_MatchExactly,
  RDI_MatchNoEarlier
}
RDI_ConfigMatchType;

typedef int rdi_addconfig_proc (unsigned long nbytes);
typedef int rdi_loadconfigdata_proc (unsigned long nbytes, char const *data);
typedef int rdi_selectconfig_proc (RDI_ConfigAspect aspect, char const *name,
				   RDI_ConfigMatchType matchtype,
				   unsigned versionreq, unsigned *versionp);

typedef char *getbufferproc (void *getbarg, unsigned long *sizep);
typedef int rdi_loadagentproc (ARMword dest, unsigned long size,
			       getbufferproc * getb, void *getbarg);

typedef struct
{
  int itemmax;
  char const *const *names;
}
RDI_NameList;

typedef RDI_NameList const *rdi_namelistproc (void);

typedef int rdi_errmessproc (char *buf, int buflen, int errno);

struct RDIProcVec
{
  char rditypename[12];

  rdi_open_proc *open;
  rdi_close_proc *close;
  rdi_read_proc *read;
  rdi_write_proc *write;
  rdi_CPUread_proc *CPUread;
  rdi_CPUwrite_proc *CPUwrite;
  rdi_CPread_proc *CPread;
  rdi_CPwrite_proc *CPwrite;
  rdi_setbreak_proc *setbreak;
  rdi_clearbreak_proc *clearbreak;
  rdi_setwatch_proc *setwatch;
  rdi_clearwatch_proc *clearwatch;
  rdi_execute_proc *execute;
  rdi_step_proc *step;
  rdi_info_proc *info;
  /* V2 RDI */
  rdi_pointinq_proc *pointinquiry;

  /* These three useable only if RDIInfo_DownLoad returns no error */
  rdi_addconfig_proc *addconfig;
  rdi_loadconfigdata_proc *loadconfigdata;
  rdi_selectconfig_proc *selectconfig;

  rdi_namelistproc *drivernames;
  rdi_namelistproc *cpunames;

  rdi_errmessproc *errmess;

  /* Only if RDIInfo_Target returns a value with RDITarget_LoadAgent set */
  rdi_loadagentproc *loadagent;
};

#endif

extern unsigned int swi_mask;

#define SWI_MASK_DEMON		(1 << 0)
#define SWI_MASK_ANGEL		(1 << 1)
#define SWI_MASK_REDBOOT	(1 << 2)
