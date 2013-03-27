/*
 ******************************************************************************
 ******************************************************************************
 *
 * COPYRIGHT (C) by EMC Corporation, 1997 All rights reserved.
 * $Id: gdb_c_test.c,v 1.1 1998/09/15 22:25:00 msnyder Exp $
 * DESCRIPTION: This module has been provided for the purpose of testing GDB.
 *
 * NOTES:
 *
 ******************************************************************************
 *****************************************************************************/

/*=============================================================================
 *                                  INCLUDE  FILES
 *===========================================================================*/


#ifdef DO_IT_BY_THE_BOOK


#include "symtypes_defs.h"
#include "printp.h"

#include "adbg_expression.h"
#include "common_hw_ds.h"
#include "common_hw_defs.h"
#include "evnttrac.h"
#include "sym_scratch_ds.h"
#include "symglob_ds.h"
#include "sym_protglob_ds.h"

#include "ether.h"

#include <ctype.h>


#else

#include "adbg_dtc.h"

#define YES             1
#define NO              0

#define TRUE            1
#define FALSE           0

#define ENABLED         1
#define DISABLED        0

#define CONTROL_C       3   /* ASCII 'ETX' */


/*
 * Faked after ctype.h
 */

#define isxdigit(X) (((X) >= '0' && (X) <= '9') || \
                     ((X) >= 'A' && (X) <= 'F') || \
                     ((X) >= 'a' && (X) <= 'f'))
/*
 * Borrowed from string.h
 */

extern unsigned int strlen ( const char * );

/*
 * Extracted from symtypes.h:
 */

typedef char                    BOOL;     /*  8 Bits */
typedef unsigned char           UCHAR;    /*  8 Bits */
typedef unsigned short          USHORT;   /* 16 Bits */
typedef unsigned long           ULONG;    /* 32 Bits */

/*
 * for struct t_expr_tag and
 * decl of build_and_add_expression
 */
#include "adbg_expression.h"
#define NULL	0

/*
 * Extracted from printp.h:
 */

extern void printp ( const char * fptr, ... );
extern void sprintp ( const char * fptr, ... );

/*
 * Extracted from ether.h:
 */

extern long eth_to_gdb ( UCHAR *buf, long length );


/*
 * Derived from hwequs.s:
 */

#define CS_CODE_START           0x100000
#define CS_CODE_SIZE            0x200000
#define LAST_CS_WORD            (CS_CODE_START + CS_CODE_SIZE - 2)

#define sh_genstat1             (*((volatile ULONG *) 0xFFFFFE54))

#define rs232_mode1             0               /* rs-232 mode 1 reg. */
#define rs232_mode2             rs232_mode1     /* rs-232 mode 2 reg. */
#define rs232_stat              4               /* rs-232 status reg. */
#define rs232_clk               rs232_stat      /* rs-232 clock select reg. */
#define rs232_cmd               8               /* rs-232 command reg */
#define rs232_transmit          12              /* rs-232 transmit reg. */
#define rs232_receive           rs232_transmit  /* rs-232 transmit reg. */
#define rs232_aux               16              /* rs-232 aux control reg. */
#define rs232_isr               20              /* rs-232 interrupt status reg. */
#define rs232_imr               rs232_isr       /* rs-232 interrupt mask reg. */
#define rs232_tc_high           24              /* rs-232 timer/counter high reg. */
#define rs232_tc_low            28              /* rs-232 timer/counter low reg.  */


#endif


/*============================================================================
 *                                 MODULE  DEFINES
 *===========================================================================*/

#define P_RST_LAN_UART_REG      ((volatile UCHAR  *) 0xFFFFFE45)
#define M_RST_LAN_UART          0x80          /* Bit  7 */

#define P_LAN0TR_REG            P_RST_LAN_UART_REG
#define M_LAN0TR                0x20          /* Bit  5 */

#define M_SH_GENCON_LAN0TR      0x00200000    /* Bit 21 */

#define MAX_RS232_CHARS         512

#define LAN_Q_MOD(X)            ((X) % MAX_RS232_CHARS)

/*---------------------------------------*
 *           LAN  UART  Registers        *
 *---------------------------------------*/

#define LAN_UART_BASE               ((ULONG) 0xfffffc22)

/*  Write-Read  */

#define P_LAN_MR1                   ((volatile UCHAR *) (LAN_UART_BASE + ((ULONG) rs232_mode1   )))
#define P_LAN_MR2                   ((volatile UCHAR *) (LAN_UART_BASE + ((ULONG) rs232_mode2   )))

/*  Write-Only  */

#define P_LAN_ACR                   ((volatile UCHAR *) (LAN_UART_BASE + ((ULONG) rs232_aux     )))
#define P_LAN_CR                    ((volatile UCHAR *) (LAN_UART_BASE + ((ULONG) rs232_cmd     )))
#define P_LAN_CSR                   ((volatile UCHAR *) (LAN_UART_BASE + ((ULONG) rs232_clk     )))
#define P_LAN_CTLR                  ((volatile UCHAR *) (LAN_UART_BASE + ((ULONG) rs232_tc_low  )))
#define P_LAN_CTUR                  ((volatile UCHAR *) (LAN_UART_BASE + ((ULONG) rs232_tc_high )))
#define P_LAN_IMR                   ((volatile UCHAR *) (LAN_UART_BASE + ((ULONG) rs232_imr     )))

/*  Read-Only */

#define P_LAN_SR                    ((volatile UCHAR *) (LAN_UART_BASE + ((ULONG) rs232_stat    )))
#define P_LAN_ISR                   ((volatile UCHAR *) (LAN_UART_BASE + ((ULONG) rs232_isr     )))
#define P_LAN_XMT                   ((volatile UCHAR *) (LAN_UART_BASE + ((ULONG) rs232_transmit)))
#define P_LAN_RCV                   ((volatile UCHAR *) (LAN_UART_BASE + ((ULONG) rs232_receive )))

/*
 *   Bit Values for Write-Read and Write-Only Registers
 */

#define DEFAULT_LAN_MR1             ((UCHAR) 0x13)
#define DEFAULT_LAN_MR2             ((UCHAR) 0x07)
#define DEFAULT_LAN_CSR             ((UCHAR) 0xcc)
#define DEFAULT_LAN_ACR             ((UCHAR) 0x38)
#define DEFAULT_LAN_CTUR            ((UCHAR) 0xff)
#define DEFAULT_LAN_CTLR            ((UCHAR) 0xff)

#define LAN_ACR_SELECT_BRG_0        DEFAULT_LAN_ACR
#define LAN_ACR_SELECT_BRG_1        (DEFAULT_LAN_ACR | 0x80)

#define UART_CR_RESET_MR_PTR        ((UCHAR) 0x10) /* Reset MR pointer (points to MR1). */
#define UART_CR_RESET_RVCR          ((UCHAR) 0x20) /* Reset receiver (disabled).        */
#define UART_CR_RESET_XMTR          ((UCHAR) 0x30) /* Reset transmitter (disabled).     */
#define UART_CR_RESET_ERROR_STATUS  ((UCHAR) 0x40) /* Reset error status.               */
#define UART_CR_RESET_BRK_CHG_INT   ((UCHAR) 0x50) /* Reset break change interrupt.     */
#define UART_CR_START_CNTR_TIMER    ((UCHAR) 0x80) /* Start counter/timer.              */
#define UART_CR_STOP_CNTR           ((UCHAR) 0x90) /* Stop counter.                     */

#define UART_CR_DISABLE_XMTR        ((UCHAR) 0x08) /* Disable transmitter.              */
#define UART_CR_ENABLE_XMTR         ((UCHAR) 0x04) /* Enable transmitter.               */
#define UART_CR_DISABLE_RCVR        ((UCHAR) 0x02) /* Disable receiver.                 */
#define UART_CR_ENABLE_RCVR         ((UCHAR) 0x01) /* Enable receiver.                  */

#define UART_CSR_BR_4800            ((UCHAR) 0x99) /* With either BRG Set selected (via ACR). */
#define UART_CSR_BR_9600            ((UCHAR) 0xbb) /* With either BRG Set selected (via ACR). */
#define UART_CSR_BR_19200           ((UCHAR) 0xcc) /* With BRG Set '1' selected (via ACR). */
#define UART_CSR_BR_38400           ((UCHAR) 0xcc) /* With BRG Set '0' selected (via ACR). */

#define UART_IMR_RxRDY              ((UCHAR) 0x04) /* Enable 'RxRDY' interrupt. */
#define UART_IMR_TxEMT              ((UCHAR) 0x02) /* Enable 'TxEMT' interrupt. */
#define UART_IMR_TxRDY              ((UCHAR) 0x01) /* Enable 'TxRDY' interrupt. */

/*
 *   Bit Masks for Read-Only Registers
 */

#define M_UART_SR_RCVD_BRK      0x80    /* Bit 7 */
#define M_UART_SR_FE            0x40    /* Bit 6 */
#define M_UART_SR_PE            0x20    /* Bit 5 */
#define M_UART_SR_OE            0x10    /* Bit 4 */
#define M_UART_SR_TxEMT         0x08    /* Bit 3 */
#define M_UART_SR_TxRDY         0x04    /* Bit 2 */
#define M_UART_SR_FFULL         0x02    /* Bit 1 */
#define M_UART_SR_RxRDY         0x01    /* Bit 0 */

#define M_UART_ISR_RxRDY        0x04    /* Bit 2 */
#define M_UART_ISR_TxEMT        0x02    /* Bit 1 */
#define M_UART_ISR_TxRDY        0x01    /* Bit 0 */

/*---------------------------------------*
 *       Support for 'Utility 83'.       *
 *---------------------------------------*/

#define LAN_UTIL_CODE           0x83

#define LAN_INIT                ((ULONG) (('I' << 24) | ('N' << 16) | ('I' << 8) | 'T'))
#define LAN_BAUD                ((ULONG) (('B' << 24) | ('A' << 16) | ('U' << 8) | 'D'))
#define LAN_INTR                ((ULONG) (('I' << 24) | ('N' << 16) | ('T' << 8) | 'R'))
#define LAN_XMT                 ((ULONG)               (('X' << 16) | ('M' << 8) | 'T'))
#define LAN_ECHO                ((ULONG) (('E' << 24) | ('C' << 16) | ('H' << 8) | 'O'))
#define LAN_STAT                ((ULONG) (('S' << 24) | ('T' << 16) | ('A' << 8) | 'T'))
#define LAN_IN                  ((ULONG)                             (('I' << 8) | 'N'))
#define LAN_OUT                 ((ULONG)               (('O' << 16) | ('U' << 8) | 'T'))

#define LAN_PUTC                ((ULONG) (('P' << 24) | ('U' << 16) | ('T' << 8) | 'C'))
#define LAN_WPM                 ((ULONG)               (('W' << 16) | ('P' << 8) | 'M'))

#define STATUS(X)               ( ( ( X ) == 0 ) ? "disabled" : "enabled" )

#define XMT_VIA_BP_ENABLED()    ( *P_LAN0TR_REG & M_LAN0TR  ?  1 : 0 )

#define TRAP_1_INST             0x4E41

/*
 *   Bit #13 of shared genstat 1 indicates
 *   which processor we are as follows.
 *
 *           0 => X (side A)
 *           1 => Y (side B)
 */

#define M_PROC_ID               0x00002000

#define IS_SIDE_A()             ( ( (sh_genstat1) & M_PROC_ID ) == 0 )
#define IS_SIDE_B()             ( (sh_genstat1) & M_PROC_ID )


#ifdef STANDALONE       /* Compile this module stand-alone for debugging */
#define LAN_PUT_CHAR(X) printf("%c", X)
#else
#define LAN_PUT_CHAR(X) while ( lan_put_char( X ) )
#endif




#define VIA_RS232             0
#define VIA_ETHERNET          1

#define MAX_IO_BUF_SIZE       400

#define MAX_BYTE_CODES        200 /* maximum length for bytecode string */


static  ULONG           gdb_host_comm;

static  ULONG           gdb_cat_ack;

static  char            eth_outbuffer[ MAX_IO_BUF_SIZE + 1 ];


#ifdef STANDALONE

#define ACK_PKT()       LAN_PUT_CHAR( '+' )
#define NACK_PKT()      LAN_PUT_CHAR( '-' )

#else

#define ACK_PKT()       {                                             \
                          if ( VIA_ETHERNET == gdb_host_comm )        \
                          {                                           \
                            gdb_cat_ack = YES;                        \
                          }                                           \
                          else                                        \
                          {                                           \
                            LAN_PUT_CHAR( '+' );                      \
                          }                                           \
                        }



#define NACK_PKT()      {                                             \
                          if ( VIA_ETHERNET == gdb_host_comm )        \
                          {                                           \
                            eth_outbuffer[ 0 ] = '-';                 \
                            eth_to_gdb( (UCHAR *) eth_outbuffer, 1 ); \
                          }                                           \
                          else                                        \
                          {                                           \
                            LAN_PUT_CHAR( '-' );                      \
                          }                                           \
                        }

#endif




/*============================================================================
 *                                 MODULE  TYPEDEFS
 *===========================================================================*/

typedef struct rs232_queue {

  long    head_index;

  long    tail_index;

  ULONG   overflows;

  long    gdb_packet_start;
  long    gdb_packet_end;
  long    gdb_packet_csum1;
  long    gdb_packet_csum2;

  UCHAR   buf[ MAX_RS232_CHARS ];

} T_RS232_QUEUE;




/*=============================================================================
 *                        EXTERNAL GLOBAL VARIABLES
 *===========================================================================*/

extern volatile UCHAR         sss_trace_flag;


/*=============================================================================
 *                           STATIC  MODULE  DECLARATIONS
 *===========================================================================*/

static  T_RS232_QUEUE lan_input_queue,
                      lan_output_queue;

static  BOOL          test_echo;

#if 0
/* The stub no longer seems to use this.  */
static  BOOL          write_access_enabled;
#endif

static  int           baud_rate_idx;

static  ULONG         tx_by_intr,
                      tx_by_poll;

static  UCHAR         lan_shadow_imr;


/*=============================================================================
 *                        EXTERNAL FUNCTION PROTOTYPES
 *===========================================================================*/

extern  long  write_to_protected_mem( void *address, unsigned short value );


/*=============================================================================
 *                      MODULE GLOBAL FUNCTIONS PROTOTYPES
 *===========================================================================*/

ULONG gdb_c_test( ULONG *parm );


void  lan_init( void );

void  lan_isr( void );

long  lan_get_char( void );

long  lan_put_char( UCHAR c );

ULONG lan_util( ULONG *parm );


/*=============================================================================
 *                      MODULE LOCAL FUNCTION PROTOTYPES
 *===========================================================================*/

static  void  lan_reset( void );

static  void  lan_configure( void );

static  void  lan_init_queue( T_RS232_QUEUE *p_queue );

static  void  lan_add_to_queue( long c, T_RS232_QUEUE *p_queue );

static  UCHAR lan_next_queue_char( T_RS232_QUEUE *p_queue );

static  void  lan_util_menu( void );

static  long  get_gdb_input( long c, T_RS232_QUEUE *p_input_q );


/*=============================================================================
 *                      GDB STUB FUNCTION PROTOTYPES
 *===========================================================================*/

void  gdb_trap_1_handler( void );
void  gdb_trace_handler ( void );

void  gdb_get_eth_input( unsigned char *buf, long length );

static void getpacket ( void );
static void putpacket ( char * );
static void discard_packet ( void );

#ifdef    STANDALONE    /* Compile this module stand-alone for debugging */
#include <stdio.h>
#define printp printf   /* easier than declaring a local varargs stub func.  */
#endif /* STANDALONE */


/*=============================================================================
 *                              MODULE BODY
 *===========================================================================*/

/* ------------------- Things that belong in a header file --------------- */
extern char *memset (char *, int, int);

                  /*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*
                  *                                     *
                  *       Global Module Functions       *
                  *                                     *
                  *%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/


static char   gdb_char_test;
static short  gdb_short_test;
static long   gdb_long_test;
static char   gdb_arr_test[25];
static struct GDB_STRUCT_TEST
{
  char   c;
  short  s;
  long   l;
  int    bfield : 11;	/* collect bitfield */
  char   arr[25];
  struct GDB_STRUCT_TEST *next;
} gdb_struct1_test, gdb_struct2_test, *gdb_structp_test, **gdb_structpp_test;

static union GDB_UNION_TEST
{
  char   c;
  short  s;
  long   l;
  int    bfield : 11;	/* collect bitfield */
  char   arr[4];
  union GDB_UNION_TEST *next;
} gdb_union1_test;

void gdb_recursion_test (int, int, int, int,  int,  int,  int);

void gdb_recursion_test (int depth, 
			 int q1, 
			 int q2, 
			 int q3, 
			 int q4, 
			 int q5, 
			 int q6)
{	/* gdb_recursion_test line 0 */
  int q = q1;						/* gdbtestline 1 */

  q1 = q2;						/* gdbtestline 2 */
  q2 = q3;						/* gdbtestline 3 */
  q3 = q4;						/* gdbtestline 4 */
  q4 = q5;						/* gdbtestline 5 */
  q5 = q6;						/* gdbtestline 6 */
  q6 = q;						/* gdbtestline 7 */
  if (depth--)						/* gdbtestline 8 */
    gdb_recursion_test (depth, q1, q2, q3, q4, q5, q6);	/* gdbtestline 9 */
}


ULONG   gdb_c_test( ULONG *parm )

{
   char *p = "gdb_c_test";
   char *ridiculously_long_variable_name_with_equally_long_string_assignment;
   register long local_reg = 7;
   static unsigned long local_static, local_static_sizeof;
   long local_long;
   unsigned long *stack_ptr;
   unsigned long end_of_stack;

   ridiculously_long_variable_name_with_equally_long_string_assignment = 
     "ridiculously long variable name with equally long string assignment";
   local_static = 9;
   local_static_sizeof = sizeof (struct GDB_STRUCT_TEST);
   local_long = local_reg + 1;
   stack_ptr  = (unsigned long *) &local_long;
   end_of_stack = 
     (unsigned long) &stack_ptr + sizeof(stack_ptr) + sizeof(end_of_stack) - 1;

   printp ("\n$Id: gdb_c_test.c,v 1.1 1998/09/15 22:25:00 msnyder Exp $\n");

   printp( "%s: arguments = %X, %X, %X, %X, %X, %X\n",
           p, parm[ 1 ], parm[ 2 ], parm[ 3 ], parm[ 4 ], parm[ 5 ], parm[ 6 ] );

   gdb_char_test   = gdb_struct1_test.c = (char)   ((long) parm[1] & 0xff);
   gdb_short_test  = gdb_struct1_test.s = (short)  ((long) parm[2] & 0xffff);
   gdb_long_test   = gdb_struct1_test.l = (long)   ((long) parm[3] & 0xffffffff);
   gdb_union1_test.l = (long) parm[4];
   gdb_arr_test[0] = gdb_struct1_test.arr[0] = (char) ((long) parm[1] & 0xff);
   gdb_arr_test[1] = gdb_struct1_test.arr[1] = (char) ((long) parm[2] & 0xff);
   gdb_arr_test[2] = gdb_struct1_test.arr[2] = (char) ((long) parm[3] & 0xff);
   gdb_arr_test[3] = gdb_struct1_test.arr[3] = (char) ((long) parm[4] & 0xff);
   gdb_arr_test[4] = gdb_struct1_test.arr[4] = (char) ((long) parm[5] & 0xff);
   gdb_arr_test[5] = gdb_struct1_test.arr[5] = (char) ((long) parm[6] & 0xff);
   gdb_struct1_test.bfield = 144;
   gdb_struct1_test.next = &gdb_struct2_test;
   gdb_structp_test      = &gdb_struct1_test;
   gdb_structpp_test     = &gdb_structp_test;

   gdb_recursion_test (3, (long) parm[1], (long) parm[2], (long) parm[3],
		       (long) parm[4], (long) parm[5], (long) parm[6]);

   gdb_char_test = gdb_short_test = gdb_long_test = 0;
   gdb_structp_test  = (void *) 0;
   gdb_structpp_test = (void *) 0;
   memset ((char *) &gdb_struct1_test, 0, sizeof (gdb_struct1_test));
   memset ((char *) &gdb_struct2_test, 0, sizeof (gdb_struct2_test));
   local_static_sizeof = 0;
   local_static = 0;
   return ( (ULONG) 0 );
}


/*-----------------------------------------------------------------------------
 *
 * FUNCTION NAME:   lan_init
 *
 *
 * DESCRIPTION:
 *
 *
 * RETURN VALUE:
 *
 *
 * USED GLOBAL VARIABLES:
 *
 *
 * AFFECTED GLOBAL VARIABLES/SIDE EFFECTS:
 *
 *
 * NOTES:
 *
 *
 *
 *---------------------------------------------------------------------------*/

void    lan_init( void )

{

  if ( IS_SIDE_A( ) )
  {

    lan_reset( );

    lan_init_queue( &lan_input_queue );

    lan_init_queue( &lan_output_queue );

    lan_configure( );
  }

  return;
}
/* end of 'lan_init'
 *===========================================================================*/


/*-----------------------------------------------------------------------------
 *
 * FUNCTION NAME:   lan_isr
 *
 *
 * DESCRIPTION:
 *
 *
 * RETURN VALUE:    None.
 *
 *
 * USED GLOBAL VARIABLES:
 *
 *
 * AFFECTED GLOBAL VARIABLES/SIDE EFFECTS:
 *
 *
 * NOTES:
 *
 *
 *---------------------------------------------------------------------------*/

void      lan_isr( void )

{
  UCHAR   c;


  lan_shadow_imr = 0;           /*  Disable all UART interrupts.  */
  *P_LAN_IMR = lan_shadow_imr;


  if ( *P_LAN_ISR & M_UART_ISR_RxRDY )
  {

    gdb_host_comm = VIA_RS232;

    c = *P_LAN_RCV;

    if ( test_echo )
    {
      /* ????? */
    }

    if ( c == CONTROL_C )
    {
        /* can't stop the target, but we can tell gdb to stop waiting... */
      discard_packet( );
      putpacket( "S03" );       /* send back SIGINT to the debugger */
    }

    else
    {
      lan_add_to_queue( (long) c, &lan_input_queue );
      get_gdb_input( (long) c, &lan_input_queue );
    }

  }

  if ( XMT_VIA_BP_ENABLED( ) )
  {

    c = 0;

    while ( (*P_LAN_ISR & M_UART_ISR_TxRDY)  &&  (c = lan_next_queue_char( &lan_output_queue )) )
    {
      *P_LAN_XMT = c;
      ++tx_by_intr;
    }

    if ( c )
    {
      lan_shadow_imr |= UART_IMR_TxRDY;   /*  (Re-)Enable 'TxRDY' interrupt from UART.  */
    }

  }


  lan_shadow_imr |= UART_IMR_RxRDY;       /*  Re-Enable 'RxRDY' interrupt from UART.  */
  *P_LAN_IMR = lan_shadow_imr;



  return;
}
/* end of 'lan_isr'
 *===========================================================================*/


/*-----------------------------------------------------------------------------
 *
 * FUNCTION NAME:   lan_get_char
 *
 *
 * DESCRIPTION:     Fetches a character from the UART.
 *
 *
 * RETURN VALUE:    0 on success, -1 on failure.
 *
 *
 * USED GLOBAL VARIABLES:
 *
 *
 * AFFECTED GLOBAL VARIABLES/SIDE EFFECTS:
 *
 *
 * NOTES:
 *
 *
 *---------------------------------------------------------------------------*/

long    lan_get_char( void )

{
  long status = -2; /* AGD: nothing found in rcv buffer */

  if ( *P_LAN_SR & M_UART_SR_RxRDY )
  {
    char c = (char) *P_LAN_RCV;

    if ( test_echo )
    {
      LAN_PUT_CHAR ( c );
    }

    if ( c == CONTROL_C )
    {
        /* can't stop the target, but we can tell gdb to stop waiting... */
      discard_packet( );
      putpacket( "S03" );       /* send back SIGINT to the debugger */
      status = 0;               /* success */
    }

    else
    {
      lan_add_to_queue( (long) c, &lan_input_queue );
      status = get_gdb_input( (long) c, &lan_input_queue );
    }

  }

  return( status );
}
/* end of 'lan_get_char'
 *===========================================================================*/


/*-----------------------------------------------------------------------------
 *
 * FUNCTION NAME:   lan_put_char
 *
 * DESCRIPTION:     Puts a character out via the UART.
 *
 * RETURN VALUE:    0 on success, -1 on failure.
 *
 * USED GLOBAL VARIABLES: none.
 *
 * AFFECTED GLOBAL VARIABLES/SIDE EFFECTS:
 *
 * NOTES: !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *        !!                                                                  !!
 *        !!  If 'XMT_VIA_BP_ENABLED()' is FALSE then output is THROWN AWAY.  !!
 *        !!  This prevents anyone infinite-looping on this function.         !!
 *        !!                                                                  !!
 *        !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 *---------------------------------------------------------------------------*/

long    lan_put_char( UCHAR c )

{
  long    status = -1;

  if ( XMT_VIA_BP_ENABLED( ) )
  {

    if ( *P_LAN_SR & M_UART_SR_TxRDY )
    {
      lan_add_to_queue( (long) c, &lan_output_queue );

      c = lan_next_queue_char( &lan_output_queue );

      *P_LAN_XMT = c;
      ++tx_by_poll;
      status = 0;
    }
#if 0
    else
    {
      status = 0;
      lan_shadow_imr |= UART_IMR_TxRDY;   /*  Enable 'TxRDY' interrupt from UART. */
      *P_LAN_IMR = lan_shadow_imr;
    }
#endif
  }

  else
  {
    status = 0;   /* You lose: input character goes to the bit bucket. */
  }

  return( status );
}
/* end of 'lan_put_char'
 *===========================================================================*/


/*-----------------------------------------------------------------------------
 *
 * FUNCTION NAME:   lan_util
 *
 * DESCRIPTION:
 *
 * RETURN VALUE:
 *
 * USED GLOBAL VARIABLES:
 *
 * AFFECTED GLOBAL VARIABLES/SIDE EFFECTS:
 *
 * NOTES:
 *
 *---------------------------------------------------------------------------*/

ULONG   lan_util( ULONG *parm )

{


  static const struct {

    ULONG rate_code;
    UCHAR acr_setting;
    UCHAR csr_setting;

  } baud_rate_setting [] = {

    { 0x38400, LAN_ACR_SELECT_BRG_0, UART_CSR_BR_38400 },
    { 0x19200, LAN_ACR_SELECT_BRG_1, UART_CSR_BR_19200 },
    { 0x9600,  LAN_ACR_SELECT_BRG_0, UART_CSR_BR_9600  },
    { 0x4800,  LAN_ACR_SELECT_BRG_0, UART_CSR_BR_4800  }
  };


#define BOGUS_P1        0xE1
#define BOGUS_P2        0xE2

  ULONG   not_done_code;


  ULONG   opcode;
  ULONG   parm_1;
  ULONG   parm_2;

  int     i;
  UCHAR   c;


  not_done_code = 0;

  opcode = parm[ 1 ];
  parm_1 = parm[ 2 ];
  parm_2 = parm[ 3 ];


  switch ( opcode )
  {

    case LAN_INIT:
      {

        lan_init( );
        printp( "\n\n  Interface (Re)Initialized ...\n\n" );

        break;
      }


    case LAN_BAUD:
      {

        for ( i = 0; i < (int)(sizeof(baud_rate_setting) / sizeof(baud_rate_setting[0])); i ++ )
        {
          if ( baud_rate_setting[i].rate_code == parm_1 )
          {
            baud_rate_idx = i;
            *P_LAN_ACR = baud_rate_setting[i].acr_setting;
            *P_LAN_CSR = baud_rate_setting[i].csr_setting;
            printp ( "Baud rate set to %X!\n", baud_rate_setting[i].rate_code );
            return( not_done_code );
          }
        }

        printp( "\n\n  *** SYNTAX Error  -  Invalid baudrate (P2)\n\n" );
        not_done_code = BOGUS_P2;

        break;
      }


    case LAN_INTR:
      {

        switch ( parm_1 )
        {

          case 0x0D: /* Disable 'RxRDY' Interrupts */
            {
              lan_shadow_imr &= ~UART_IMR_RxRDY;
              *P_LAN_IMR = lan_shadow_imr;
              printp( "\n\n  Receive Ready Interrupts DISABLED ...\n\n" );
              break;
            }

          case 0x0E: /* Enable 'RxRDY' Interrupts */
            {
              lan_shadow_imr |= UART_IMR_RxRDY;
              *P_LAN_IMR = lan_shadow_imr;
              printp( "\n\n  Receive Ready Interrupts ENABLED ...\n\n" );
              break;
            }

          default:
            {
              printp( "\n\n  *** SYNTAX Error  -  Invalid P2 (use D or E)\n\n" );
              not_done_code = BOGUS_P2;
            }
        }

        break;
      }


    case LAN_XMT:
      {

        switch ( parm_1 )
        {

          case 0x0E: /* Enable Transmission-via-Backplane */
            {
              if ( !(*P_LAN0TR_REG & M_LAN0TR) )
              {
                *P_LAN0TR_REG |= M_LAN0TR;  /* 0 -> 1 */
              }

              printp( "\n\n  Transmit-via-Backplane ENABLED ...\n\n" );
              break;
            }

          case 0x0D: /* Disable Transmission-via-Backplane */
            {
              if ( *P_LAN0TR_REG & M_LAN0TR )
              {
                *P_LAN0TR_REG &= ~M_LAN0TR; /* 1 -> 0 */
              }

              printp( "\n\n  Transmit-via-Backplane DISABLED ...\n\n" );
              break;
            }

          default:
            {
              printp( "\n\n  *** SYNTAX Error  -  Invalid P2 (use D or E)\n\n" );
              not_done_code = BOGUS_P2;
              lan_util_menu( );
            }
        }

        break;
      }


    case LAN_STAT:
      {

      printp( "\n              -- Status --\n\n" );

        printp( "          Baud Rate: %X *\n",   baud_rate_setting[ baud_rate_idx ].rate_code );
        printp( "         Xmt-via-BP: %s *\n",   STATUS( XMT_VIA_BP_ENABLED( ) ) );
        printp( "         RxRdy Intr: %s *\n",   STATUS( (lan_shadow_imr & M_UART_ISR_RxRDY) ) );
   /*** printp( "         TxRdy Intr: %s\n",     STATUS( (lan_shadow_imr & M_UART_ISR_TxRDY) ) ); ***/
        printp( "               Echo: %s *\n\n", STATUS( test_echo ) );

        printp( "                IMR: %02X\n", (ULONG) lan_shadow_imr );
        printp( "                ISR: %02X\n", (ULONG) *P_LAN_ISR );
        printp( "                 SR: %02X\n\n", (ULONG) *P_LAN_SR );

        printp( "    Input Overflows: %d\n\n", lan_input_queue.overflows );

        printp( "         Tx by Intr: %d\n", tx_by_intr  );
        printp( "         Tx by Poll: %d\n\n", tx_by_poll );

        printp( "         *  Can be set or toggled via Utility %2X.\n\n", (ULONG) LAN_UTIL_CODE );

        break;
      }


    case LAN_IN:
      {

        switch ( parm_1 )
        {

          case 0x0C: /* Clear and Reset Queue */
            {
              lan_init_queue( &lan_input_queue );
              printp( "\n\n  Queue CLEARED/RESET ...\n\n" );
              break;
            }

          case 0x0D: /* Display Queue */
            {
              printp( "\n                        -- Input Queue --\n" );
              printp( "\n        Head Index: %8X     Tail Index: %8X\n\n    ",
                     (ULONG) lan_input_queue.head_index, (ULONG) lan_input_queue.tail_index );

              for ( i = 0; i < MAX_RS232_CHARS; ++i )
              {
                printp( " %02X", (ULONG) lan_input_queue.buf[ i ] );

                if ( 15 == (i % 16) )
                {
                  int j;

                  printp ( "    " );
                  for ( j = i - 15; j <= i; j++ )
                    {
                      if ( lan_input_queue.buf[ j ] >= ' ' &&
                          lan_input_queue.buf[ j ] < 127 )
                        printp ( "%c", lan_input_queue.buf[ j ] );
                      else
                        printp ( "." );
                    }
                  printp( "\n    " );
                }

                else if ( 7 == (i % 8) )
                {
                  printp( " " );
                }

              }

              printp( "\n" );

              break;
            }

          case 0x0F: /* Fetch next character in Queue */
            {
              c = lan_next_queue_char( &lan_input_queue );

              if ( c )
              {
                printp( "\n\n  Next Character: " );
                if (  0x21 <= c  &&  c <= 0x7F )
                {
                  printp( "%c\n\n", (ULONG) c );
                }

                else if ( 0x20 == ((UCHAR) c) )
                {
                  printp( "<space>\n\n" );
                }

                else
                {
                  printp( "%02X\n\n", (ULONG) c );
                }
              }

              else
              {
                printp( "\n\n  Input Queue EMPTY ...\n\n" );
              }

            break;
            }

          default:
            {
            printp( "\n\n  *** SYNTAX Error  -  Invalid P2 ...\n\n" );
            not_done_code = BOGUS_P2;
            break;
            }
        }

      break;
      }


    case LAN_OUT:
      {

        switch ( parm_1 )
        {

          case 0x0C: /* Clear and Reset Queue */
            {
              lan_init_queue( &lan_output_queue );
              printp( "\n\n  Queue CLEARED/RESET ...\n\n" );
              break;
            }

          case 0x0D: /* Display Queue */
            {
              printp( "\n                       -- Output Queue --\n" );
              printp( "\n        Head Index: %8X     Tail Index: %8X\n\n    ",
                     (ULONG) lan_output_queue.head_index, (ULONG) lan_output_queue.tail_index );

              for ( i = 0; i < MAX_RS232_CHARS; ++i )
              {
                printp( " %02X", (ULONG) lan_output_queue.buf[ i ] );

                if ( 15 == (i % 16) )
                {
                  int j;

                  printp ( "    " );
                  for ( j = i - 15; j <= i; j++ )
                    {
                      if ( lan_output_queue.buf[ j ] >= ' ' &&
                          lan_output_queue.buf[ j ] < 127 )
                        printp ( "%c", lan_output_queue.buf[ j ] );
                      else
                        printp ( "." );
                    }
                  printp( "\n    " );
                }

                else if ( 7 == (i % 8) )
                {
                  printp( " " );
                }

              }

              printp( "\n" );

              break;
            }

          case 0x0F: /* Fetch next character in Queue */
            {
              c = lan_next_queue_char( &lan_output_queue );

              if ( c )
              {
                printp( "\n\n  Next Character: " );
                if (  0x21 <= c  &&  c <= 0x7F )
                {
                  printp( "%c\n\n", (ULONG) c );
                }

                else if ( 0x20 == c )
                {
                  printp( "<space>\n\n" );
                }

                else
                {
                  printp( "%02X\n\n", (ULONG) c );
                }
              }

              else
              {
                printp( "\n\n  Input Queue EMPTY ...\n\n" );
              }

              break;
            }

          default:
            {
            printp( "\n\n  *** SYNTAX Error  -  Invalid P2 ...\n\n" );
            not_done_code = BOGUS_P2;
            break;
            }
        }

        break;
      }


    case LAN_ECHO:
      {

        switch ( parm_1 )
        {

          case 0x0E:
            {
              test_echo = ENABLED;
              printp( "\n\n  Test echo ENABLED ...\n\n" );
              break;
            }

          case 0x0D:
            {
              test_echo = DISABLED;
              printp( "\n\n  Test echo DISABLED ...\n\n" );
              break;
            }

          default:
            {
              printp( "\n\n  *** SYNTAX Error  -  Invalid P2 ...\n\n" );
              not_done_code = BOGUS_P2;
              break;
            }
        }

        break;
      }


    case LAN_PUTC:
      {

        if ( 0x20 < parm_1  &&  parm_1 < 0x7F )
        {
          if ( lan_put_char( (UCHAR) parm_1 ) )
          {
            printp( "\n\n  *** 'lan_put_char' Error ...\n" );
          }

          else
          {
            printp( "\n\n  O.K. ...\n" );
          }

        }

        else
        {
          printp( "\n\n  *** Error  -  character must be in the 0x21-0x7E range ...\n" );
          not_done_code = BOGUS_P2;
        }

        break;
      }

/***
    case LAN_WPM:
      {

        if ( write_to_protected_mem( (void *) parm_1, (unsigned short) parm_2 ) )
        {
          printp( "\n  Write to protected memory FAILED ...\n" );
        }

        break;
      }
***/

    case 0: /* no argument -- print menu */
      {
        lan_util_menu( );
        break;
      }


    default:
      {
        parm_2 = 0;  /* to supress compiler warning with 'LAN_WPM' case disabled */

        printp( "\n\n  *** SYNTAX Error  -  Invalid P1 ...\n\n" );
        not_done_code = BOGUS_P1;
        break;
      }


  } /*  End of 'switch ( opcode )'. */


return( not_done_code );
}
/* end of 'lan_util'
 *===========================================================================*/


                  /*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*
                  *                                     *
                  *         Local Module Functions      *
                  *                                     *
                  *%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/*-----------------------------------------------------------------------------
 *
 * FUNCTION NAME:   lan_reset
 *
 * DESCRIPTION:     Resets the LAN UART by strobing the 'RST_LAN_UART' bit in the
 *                  Shared Control 1 area.
 *
 *                             1 _|       ______
 *                                |      |      |
 *                          Bit   |      |      |
 *                                |      |      |
 *                             0 _|______|      |______
 *                                |---------------------> t
 *
 * RETURN VALUE:    None.
 *
 * USED GLOBAL VARIABLES:
 *
 * AFFECTED GLOBAL VARIABLES/SIDE EFFECTS:
 *
 * NOTES:           H/W configuration requires that a byte in the shared
 *                  control 1 area must be read before being written.
 *
 *---------------------------------------------------------------------------*/

static  void    lan_reset( void )

{

  while ( *P_RST_LAN_UART_REG & M_RST_LAN_UART )
  {
    *P_RST_LAN_UART_REG &= ~M_RST_LAN_UART;     /* 0 */
  }

  while ( !(*P_RST_LAN_UART_REG & M_RST_LAN_UART) )
  {
    *P_RST_LAN_UART_REG |= M_RST_LAN_UART;      /* 1 */
  }

  while ( *P_RST_LAN_UART_REG & M_RST_LAN_UART )
  {
    *P_RST_LAN_UART_REG &= ~M_RST_LAN_UART;     /* 0 */
  }

}
/* end of 'lan_reset'
 *===========================================================================*/


/*-----------------------------------------------------------------------------
 *
 * FUNCTION NAME:   lan_configure
 *
 *
 * DESCRIPTION:
 *
 *
 * RETURN VALUE:
 *
 *
 * USED GLOBAL VARIABLES:
 *
 *
 * AFFECTED GLOBAL VARIABLES/SIDE EFFECTS:
 *
 *
 * NOTES:
 *
 *
 *
 *---------------------------------------------------------------------------*/

static  void    lan_configure( void )

{

  *P_LAN_CR = UART_CR_RESET_MR_PTR;       /*  Points to MR1.        */
  *P_LAN_CR = UART_CR_RESET_RVCR;         /*  Receiver disabled.    */
  *P_LAN_CR = UART_CR_RESET_XMTR;         /*  Transmitter disabled. */
  *P_LAN_CR = UART_CR_RESET_ERROR_STATUS;
  *P_LAN_CR = UART_CR_RESET_BRK_CHG_INT;

  *P_LAN_MR1 = DEFAULT_LAN_MR1;
  *P_LAN_MR2 = DEFAULT_LAN_MR2;

  *P_LAN_ACR = DEFAULT_LAN_ACR;

  *P_LAN_CSR = UART_CSR_BR_9600;
  baud_rate_idx = 2;

  *P_LAN_CTUR = DEFAULT_LAN_CTUR;
  *P_LAN_CTLR = DEFAULT_LAN_CTLR;

  *P_LAN_CR = (UART_CR_START_CNTR_TIMER | UART_CR_ENABLE_XMTR | UART_CR_ENABLE_RCVR);

  lan_shadow_imr = UART_IMR_RxRDY;        /*  Enable only 'RxRDY' interrupt from UART. */
  *P_LAN_IMR = lan_shadow_imr;

  tx_by_intr = 0;
  tx_by_poll = 0;

  return;
}
/* end of 'lan_configure'
 *===========================================================================*/


/*-----------------------------------------------------------------------------
 *
 * FUNCTION NAME:   lan_init_queue
 *
 * DESCRIPTION:
 *
 * RETURN VALUE:    None.
 *
 * USED GLOBAL VARIABLES:
 *
 * AFFECTED GLOBAL VARIABLES/SIDE EFFECTS:
 *
 * NOTES:
 *
 *---------------------------------------------------------------------------*/

static  void    lan_init_queue( T_RS232_QUEUE *p_queue )

{
  long i;

    /*
    *   We set "head" equal to "tail" implying the queue is empty,
    *   BUT the "head" and "tail" should each point to valid queue
    *   positions.
    */

  p_queue->head_index = 0;
  p_queue->tail_index = 0;

  p_queue->overflows = 0;

  p_queue->gdb_packet_start = -1;
  p_queue->gdb_packet_end   = -1;

  p_queue->gdb_packet_csum1 = -1;
  p_queue->gdb_packet_csum2 = -1;

  for ( i = 0; i < MAX_RS232_CHARS; ++i )
  {
    p_queue->buf[ i ] = 0;
  }

  return;
}
/* end of 'lan_init_queue'
 *===========================================================================*/


/*-----------------------------------------------------------------------------
 *
 * FUNCTION NAME:   lan_add_to_queue
 *
 *
 * DESCRIPTION:     Adds the specified character to the tail of the
 *                  specified queue.  Observes "oldest thrown on floor"
 *                  rule (i.e. the queue is allowed to "wrap" and the
 *                  input character is unconditionally placed at the
 *                  tail of the queue.
 *
 *
 * RETURN VALUE:    None.
 *
 *
 * USED GLOBAL VARIABLES:
 *
 *
 * AFFECTED GLOBAL VARIABLES/SIDE EFFECTS:
 *
 *
 * NOTES:
 *
 *
 *---------------------------------------------------------------------------*/

static  void    lan_add_to_queue( long c, T_RS232_QUEUE *p_queue )

{

  if ( p_queue )    /*  Sanity check. */
  {

    if ( c & 0x000000FF )   /*  We don't allow NULL characters to be added to a queue.  */
    {
        /*  Insert the new character at the tail of the queue.  */

      p_queue->buf[ p_queue->tail_index ] = (UCHAR) (c & 0x000000FF);

        /*  Increment the tail index. */

      if ( MAX_RS232_CHARS <= ++(p_queue->tail_index) )
      {
        p_queue->tail_index = 0;
      }

        /*  Check for wrapping (i.e. overflow). */

      if ( p_queue->head_index == p_queue->tail_index )
      {
          /*  If the tail has caught up to the head record the overflow . . . */

        ++(p_queue->overflows);

          /*  . . . then increment the head index.  */

        if ( MAX_RS232_CHARS <= ++(p_queue->head_index) )
        {
          p_queue->head_index = 0;
        }

      }

    } /*  End of 'if ( c & 0x000000FF )'. */

  } /*  End of 'if ( p_queue )'.  */


  return;
}
/* end of 'lan_add_to_queue'
 *===========================================================================*/


/*-----------------------------------------------------------------------------
 *
 * FUNCTION NAME:   lan_next_queue_char
 *
 * DESCRIPTION:
 *
 * RETURN VALUE:
 *
 * USED GLOBAL VARIABLES:
 *
 * AFFECTED GLOBAL VARIABLES/SIDE EFFECTS:
 *
 * NOTES:
 *
 *---------------------------------------------------------------------------*/

static  UCHAR   lan_next_queue_char( T_RS232_QUEUE *p_queue )

{
  UCHAR   c;


  c = 0;

  if ( p_queue )
  {

    if ( p_queue->head_index != p_queue->tail_index )
    {
        /*  Return the 'oldest' character in the queue. */

      c = p_queue->buf[ p_queue->head_index ];

        /*  Increment the head index. */

      if ( MAX_RS232_CHARS <= ++(p_queue->head_index) )
      {
        p_queue->head_index = 0;
      }

    }

  } /*  End of 'if ( p_queue )'.  */


  return( c );
}

/* end of 'lan_next_queue_char'
 *===========================================================================*/


/*-----------------------------------------------------------------------------
 *
 * FUNCTION NAME:   lan_util_menu
 *
 * DESCRIPTION:     Prints out a brief help on the LAN UART control utility.
 *
 * RETURN VALUE:    None.
 *
 * USED GLOBAL VARIABLES: None.
 *
 * AFFECTED GLOBAL VARIABLES/SIDE EFFECTS: None.
 *
 * NOTES: None.
 *
 *---------------------------------------------------------------------------*/

static  void    lan_util_menu( void )

{

  /*
   * Multiply calling printp() below is made due to the limitations
   * of printp(), incapable of handling long formatting constants:
   */

 printp( "\n               -- Options --\n\n" );

  printp( "    %2X,'INIT' ............... Reset & (Re)INITIALIZE Interface.\n", (ULONG) LAN_UTIL_CODE );
  printp( "    %2X,'BAUD',<rate> ........ Set BAUD Rate.\n", (ULONG) LAN_UTIL_CODE );
  printp( "    %2X,'INTR',<mode> ........ Toggle 'RxRDY' Interrupts.\n", (ULONG) LAN_UTIL_CODE );
  printp( "    %2X,'XMT',<mode> ......... Toggle TRANSMIT-via-backplane.\n", (ULONG) LAN_UTIL_CODE );
  printp( "    %2X,'STAT' ............... Display STATUS.\n", (ULONG) LAN_UTIL_CODE );
  printp( "    %2X,'ECHO',<mode> ........ Enable/Disable Test ECHO.\n", (ULONG) LAN_UTIL_CODE );
  printp( "    %2X,'IN',<action> ........ Access INPUT Queue.\n", (ULONG) LAN_UTIL_CODE );
  printp( "    %2X,'OUT',<action> ....... Access OUTPUT Queue.\n\n", (ULONG) LAN_UTIL_CODE );

  printp( "    %2X,'PUTC',<char> ........ Output a Character (i.e. <char>).\n\n", (ULONG) LAN_UTIL_CODE );

/***
  printp( "    %2X,'WPM',address,word ... Write Protected Memory Test.\n\n", (ULONG) LAN_UTIL_CODE );
***/

  printp( "    <rate>:  4800  <mode>:  E - enable   <action>:  C - clear/reset\n" );
  printp( "             9600           D - disable             D - display\n" );
  printp( "            19200                                   F - fetch next char\n" );
  printp( "            38400\n" );
}
/* end of 'lan_util_menu'
 *===========================================================================*/


/* Thu Feb  5 17:14:41 EST 1998  CYGNUS...CYGNUS...CYGNUS...CYGNUS...CYGNUS...CYGNUS...CYGNUS...CYGNUS */


static  long    get_gdb_input( long c, T_RS232_QUEUE * p_input_q )

{

  /* Now to detect when we've got a gdb packet... */

  if ( '$' == c ) { /* char marks beginning of a packet */

      if ( -1 != p_input_q->gdb_packet_start ||
           -1 != p_input_q->gdb_packet_end   ||
           -1 != p_input_q->gdb_packet_csum1 ||
           -1 != p_input_q->gdb_packet_csum2 ) { /* PROTOCOL ERROR */

        /* NEW: Actually, this probably means that we muffed a packet,
           and GDB has already resent it.  The thing to do now is to
           throw away the one we WERE working on, but immediately start
           accepting the new one.  Don't NAK, or GDB will have to try
           and send it yet a third time!  */

          /*NACK_PKT( );*/    /*<ETHERNET>*/
          discard_packet( );                    /* throw away old packet */
          lan_add_to_queue ('$', p_input_q);    /* put the new "$" back in */
          return 0;
      } else {          /* match new "$" */
        p_input_q->gdb_packet_start = p_input_q->tail_index;
        p_input_q->gdb_packet_end =
          p_input_q->gdb_packet_csum1 =
            p_input_q->gdb_packet_csum2 = -1;
      }
    } else if ( '#' == c ) { /* # marks end of packet (except for checksum) */

      if ( -1 == p_input_q->gdb_packet_start ||
           -1 != p_input_q->gdb_packet_end   ||
           -1 != p_input_q->gdb_packet_csum1 ||
           -1 != p_input_q->gdb_packet_csum2 ) { /* PROTOCOL ERROR */

          /* Garbled packet.  Discard, but do not NAK.  */

          /*NACK_PKT( );*/    /*<ETHERNET>*/
          discard_packet( );
          return -1;
      }
      p_input_q->gdb_packet_end = p_input_q->tail_index;
      p_input_q->gdb_packet_csum1 = p_input_q->gdb_packet_csum2 = -1;

  } else if ( -1 != p_input_q->gdb_packet_start &&
              -1 != p_input_q->gdb_packet_end) {

    if ( isxdigit( c ) ) { /* char is one of two checksum digits for packet */

      if ( -1 == p_input_q->gdb_packet_csum1 &&
           LAN_Q_MOD( p_input_q->gdb_packet_end + 1 ) ==
           p_input_q->tail_index ) {

        /* first checksum digit */

        p_input_q->gdb_packet_csum1 = p_input_q->tail_index;
        p_input_q->gdb_packet_csum2 = -1;

      } else if ( -1 == p_input_q->gdb_packet_csum2 &&
                  LAN_Q_MOD( p_input_q->gdb_packet_end + 2 ) ==
                  p_input_q->tail_index ) {

        /* second checksum digit: packet is complete! */

        p_input_q->gdb_packet_csum2 = p_input_q->tail_index;
        getpacket();    /* got a packet -- extract it */

      } else { /* probably can't happen (um... three hex digits?) */

        /* PROTOCOL ERROR */
        /* Not sure how this can happen, but ...
           discard it, but do not NAK it.  */
        /*NACK_PKT( );*/    /*<ETHERNET>*/
        discard_packet( );
        return -1;
      }

    } else { /* '#' followed by non-hex char */

      /* PROTOCOL ERROR */
      /* Bad packet -- discard but do not NAK */
      /*NACK_PKT( );*/    /*<ETHERNET>*/
      discard_packet( );
      return -1;
    }
  }

  return 0;
}




#ifdef    STANDALONE

/* stand-alone stand-alone stand-alone stand-alone stand-alone stand-alone
   stand-alone                                                 stand-alone
   stand-alone Enable stand-alone build, for ease of debugging stand-alone
   stand-alone                                                 stand-alone
   stand-alone stand-alone stand-alone stand-alone stand-alone stand-alone */

long write_to_protected_mem (addr, word)
     void *addr;
     unsigned short word;
{
  return 0;
}


char dummy_memory[0x4000];

int main ( void )
{
  long c;

  lan_init_queue( &lan_input_queue );
  printf( "Stand-alone EMC 'stub', pid = %d\n", getpid( ) );
  printf( "Start of simulated 'memory': 0x%08x\n", &dummy_memory);
  while ( (c = getc( stdin ) ) != EOF )
    {
      if ( c == '\\' )  /* escape char */
        break;

      lan_add_to_queue( c, &lan_input_queue );
      get_gdb_input (c, &lan_input_queue);
      fflush( stdout );
    }

  printf( "Goodbye!\n" );
  exit( 0 );
}

#define SRAM_START      ((void *) (&dummy_memory[0] + 0x00000000))
#define SRAM_END        ((void *) (&dummy_memory[0] + 0x00000400))

#define RO_AREA_START   ((void *) (&dummy_memory[0] + 0x00000100))
#define RO_AREA_END     ((void *) (&dummy_memory[0] + 0x00000300))

#define NVD_START       ((void *) (&dummy_memory[0] + 0x00003000))
#define NVD_END         ((void *) (&dummy_memory[0] + 0x00003100))

#else   /* normal stub (not stand-alone) */

#define SRAM_START              ((void *) 0x00000000)
#define SRAM_END                ((void *) 0x00400000)

#define RO_AREA_START           ((void *) 0x00100000)
#define RO_AREA_END             ((void *) 0x00300000)

#define NVD_START               ((void *) 0x03000000)
#define NVD_END                 ((void *) 0x03100000)

#endif /* STANDALONE */




/* gdb gdb gdb gdb gdb gdb gdb gdb gdb gdb gdb gdb gdb gdb gdb gdb gdb gdb
   gdb                                                                 gdb
   gdb                Here begins the gdb stub section.                gdb
   gdb          The following functions were added by Cygnus,          gdb
   gdb             to make this thing act like a gdb stub.             gdb
   gdb                                                                 gdb
   gdb gdb gdb gdb gdb gdb gdb gdb gdb gdb gdb gdb gdb gdb gdb gdb gdb gdb */


/* ------------------- global defines and data decl's -------------------- */

#define hexchars        "0123456789abcdef"

/* there are 180 bytes of registers on a 68020 w/68881      */
/* many of the fpa registers are 12 byte (96 bit) registers */
#define NUMREGBYTES          180
#define NUMREGS              29
#define REGISTER_BYTE(regno) regno

enum regnames { D0, D1, D2, D3, D4, D5, D6, D7,
                A0, A1, A2, A3, A4, A5, A6, A7,
                PS, PC,
                FP0, FP1,
                FP2, FP3,
                FP4, FP5,
                FP6, FP7,
                FPCONTROL, FPSTATUS, FPIADDR
              };

unsigned long registers[NUMREGBYTES/4];

static long remote_debug;

#define BUFMAX                MAX_IO_BUF_SIZE
static char inbuffer[BUFMAX], outbuffer[BUFMAX];
static char spare_buffer[BUFMAX];


struct stub_trace_frame
{
  int                    valid;
  unsigned long          frame_id;
  unsigned long          tdp_id;
  FRAME_DEF             *frame_data;
  COLLECTION_FORMAT_DEF *format;
  unsigned long          traceregs[NUMREGBYTES/4];
  unsigned char         *stack_data;
  unsigned char         *memrange_data;
} curframe;

/* -------------------      function prototypes       -------------------- */

void handle_request ( char * );

/* -------------------         Implementation         -------------------- */

static void
discard_packet( void )
{
  lan_input_queue.head_index = lan_input_queue.tail_index;

  lan_input_queue.gdb_packet_start =
    lan_input_queue.gdb_packet_end   =
      lan_input_queue.gdb_packet_csum1 =
        lan_input_queue.gdb_packet_csum2 = -1;
}

/* Utility function: convert an ASCII isxdigit to a hex nybble */

static long
hex( char ch )
{
  if ( (ch >= 'A') && (ch <= 'F') )
    return ch - 'A' + 10;
  if ( (ch >= 'a') && (ch <= 'f') )
    return ch - 'a' + 10;
  if ( (ch >= '0') && (ch <= '9') )
    return ch - '0';
  return -1;
}

static void
getpacket( void )
{
  unsigned char our_checksum, their_checksum;
  char *copy = inbuffer;
  unsigned char c;

  our_checksum = 0;

  /* first find the '$' */
  while ((c = lan_next_queue_char ( &lan_input_queue )) != '$')
    if (c == 0)                 /* ??? Protocol error? (paranoia) */
      {
          /* PROTOCOL ERROR (missing '$') */
        /*NACK_PKT( );*/    /*<ETHERNET>*/
        return;
      }

  /* Now copy the message (up to the '#') */
  for (c = lan_next_queue_char ( &lan_input_queue );    /* skip  the   '$' */
       c != 0 && c != '#';              /* stop at the '#' */
       c = lan_next_queue_char ( &lan_input_queue ))
    {
      *copy++ = c;
      our_checksum += c;
    }
  *copy++ = '\0';               /* terminate the copy */

  if (c == 0)                   /* ??? Protocol error? (paranoia) */
    {
        /* PROTOCOL ERROR (missing '#') */
      /*NACK_PKT( );*/    /*<ETHERNET>*/
      return;
    }
  their_checksum  = hex( lan_next_queue_char ( &lan_input_queue ) ) << 4;
  their_checksum += hex( lan_next_queue_char ( &lan_input_queue ) );

  /* Now reset the queue packet-recognition bits */
  discard_packet( );

  if ( remote_debug ||
      our_checksum == their_checksum )
    {
      ACK_PKT( );      /* good packet */
      /* Parse and process the packet */
      handle_request( inbuffer );
    }
  else
      /* PROTOCOL ERROR (bad check sum) */
    NACK_PKT( );
}

/* EMC will provide a better implementation
   (perhaps just of LAN_PUT_CHAR) that does not block.
   For now, this works.  */


static void
putpacket( char *str )
{
  unsigned char checksum;

  /* '$'<packet>'#'<checksum> */

  if ( VIA_ETHERNET == gdb_host_comm )
  {
    char  *p_out;
    long  length;

    p_out  = eth_outbuffer;
    length = 0;


    if ( YES == gdb_cat_ack )
    {
      *p_out++ = '+';
      ++length;
    }

    gdb_cat_ack = NO;


    *p_out++ = '$';
    ++length;

    checksum = 0;

    while ( *str )
    {
      *p_out++ = *str;
      ++length;
      checksum += *str++;
    }

    *p_out++ = '#';
    *p_out++ = hexchars[checksum >> 4];
    *p_out = hexchars[checksum % 16];
    length += 3;

    eth_to_gdb( (UCHAR *) eth_outbuffer, length );
  }

  else
  {

      /* via RS-232 */
    do {
      LAN_PUT_CHAR( '$' );
      checksum = 0;

      while ( *str )
        {
          LAN_PUT_CHAR( *str );
          checksum += *str++;
        }

      LAN_PUT_CHAR( '#' );
      LAN_PUT_CHAR( hexchars[checksum >> 4] );
      LAN_PUT_CHAR( hexchars[checksum % 16] );
    } while ( 0 /* get_debug_char( ) != '+' */ );
    /* XXX FIXME: not waiting for the ack. */

  }

}


/*-----------------------------------------------------------------------------
 *
 * FUNCTION NAME:   gdb_get_eth_input
 *
 *
 * DESCRIPTION:
 *
 *
 * RETURN VALUE:    None.
 *
 *
 * USED GLOBAL VARIABLES:
 *
 *
 * AFFECTED GLOBAL VARIABLES/SIDE EFFECTS:
 *
 *
 * NOTES:
 *
 *
 *---------------------------------------------------------------------------*/

void    gdb_get_eth_input( unsigned char *buf, long length )

{

  gdb_host_comm = VIA_ETHERNET;

  for ( ; 0 < length; ++buf, --length)
  {

    if ( *buf == CONTROL_C )
    {
        /* can't stop the target, but we can tell gdb to stop waiting... */
      discard_packet( );
      putpacket( "S03" );       /* send back SIGINT to the debugger */
    }

    else
    {
      lan_add_to_queue( (long) *buf, &lan_input_queue );
      get_gdb_input( (long) *buf, &lan_input_queue );
    }

  }


  return;
}
/* end of 'gdb_get_eth_input'
 *===========================================================================*/




/* STDOUT STDOUT STDOUT STDOUT STDOUT STDOUT STDOUT STDOUT STDOUT STDOUT
   Stuff pertaining to simulating stdout by sending chars to gdb to be echoed.

   Dear reader:
       This code is based on the premise that if GDB receives a packet
   from the stub that begins with the character CAPITAL-OH, GDB will
   echo the rest of the packet to GDB's console / stdout.  This gives
   the stub a way to send a message directly to the user.  In practice,
   (as currently implemented), GDB will only accept such a packet when
   it believes the target to be running (ie. when you say STEP or
   CONTINUE); at other times it does not expect it.  This will probably
   change as a side effect of the "asynchronous" behavior.

   Functions: gdb_putchar(char ch)
              gdb_write(char *str, int len)
              gdb_puts(char *str)
              gdb_error(char *format, char *parm)
 */

#if 0 /* avoid compiler warning while this is not used */

/* Function: gdb_putchar(int)
   Make gdb write a char to stdout.
   Returns: the char */

static int
gdb_putchar( long ch )
{
  char buf[4];

  buf[0] = 'O';
  buf[1] = hexchars[ch >> 4];
  buf[2] = hexchars[ch & 0x0F];
  buf[3] = 0;
  putpacket( buf );
  return ch;
}
#endif

/* Function: gdb_write(char *, int)
   Make gdb write n bytes to stdout (not assumed to be null-terminated).
   Returns: number of bytes written */

static int
gdb_write( char *data, long len )
{
  char *buf, *cpy;
  long i;

  buf = outbuffer;
  buf[0] = 'O';
  i = 0;
  while ( i < len )
    {
      for ( cpy = buf+1;
           i < len && cpy < buf + BUFMAX - 3;
           i++ )
        {
          *cpy++ = hexchars[data[i] >> 4];
          *cpy++ = hexchars[data[i] & 0x0F];
        }
      *cpy = 0;
      putpacket( buf );
    }
  return len;
}

/* Function: gdb_puts(char *)
   Make gdb write a null-terminated string to stdout.
   Returns: the length of the string */

static int
gdb_puts( char *str )
{
  return gdb_write( str, strlen( str ) );
}

/* Function: gdb_error(char *, char *)
   Send an error message to gdb's stdout.
   First string may have 1 (one) optional "%s" in it, which
   will cause the optional second string to be inserted.  */

#if 0
static void
gdb_error( char *format, char *parm )
{
  static char buf[400];
  char *cpy;
  long len;

  if ( remote_debug )
    {
      if ( format && *format )
        len = strlen( format );
      else
        return;             /* empty input */

      if ( parm && *parm )
        len += strlen( parm );

      for ( cpy = buf; *format; )
        {
          if ( format[0] == '%' && format[1] == 's' ) /* include 2nd string */
            {
              format += 2;          /* advance two chars instead of just one */
              while ( parm && *parm )
                *cpy++ = *parm++;
            }
          else
            *cpy++ = *format++;
        }
      *cpy = '\0';
      gdb_puts( buf );
    }
}
#endif

static void gdb_note (char *, int);
static int  error_ret (int, char *, int);

static unsigned long
elinum_to_index (unsigned long elinum)
{
  if ((elinum & 0xf0) == 0xd0)
    return (elinum & 0x0f);
  else if ((elinum & 0xf0) == 0xa0)
    return (elinum & 0x0f) + 8;
  else
    return -1;
}

static long
index_to_elinum (unsigned long index)
{
  if (index <= 7)
    return index + 0xd0;
  else if (index <= 15)
    return (index - 8) + 0xa0;
  else
    return -1;
}


/*
  READMEM READMEM READMEM READMEM READMEM READMEM READMEM READMEM READMEM

  The following code pertains to reading memory from the target.
  Some sort of exception handling should be added to make it safe.

  READMEM READMEM READMEM READMEM READMEM READMEM READMEM READMEM READMEM

  Safe Memory Access:

  All reads and writes into the application's memory will pass thru
  get_uchar() or set_uchar(), which check whether accessing their
  argument is legal before actual access (thus avoiding a bus error).

  */

enum { SUCCESS = 0, FAIL = -1 };

#if 0
static long get_uchar ( const unsigned char * );
#endif
static long set_uchar ( unsigned char *, unsigned char );
static long read_access_violation ( const void * );
static long write_access_violation ( const void * );
static long read_access_range(const void *, long);
static DTC_RESPONSE find_memory(unsigned char *,long,unsigned char **,long *);

static int
dtc_error_ret (int ret, char *src, DTC_RESPONSE code)
{
  if (src)
    sprintp (spare_buffer,
             "'%s' returned DTC error '%s'.\n", src, get_err_text (code));
  else
    sprintp (spare_buffer, "DTC error '%s'.\n", get_err_text (code));

  gdb_puts (spare_buffer);
  return ret;
}


#if 0
/* I think this function is unnecessary since the introduction of
   adbg_find_memory_addr_in_frame.  */

/* Return the number of expressions in the format associated with a
   given trace frame.  */
static int
count_frame_exprs (FRAME_DEF *frame)
{
  CFD *format;
  T_EXPR *expr;
  int num_exprs;

  /* Get the format from the frame.  */
  get_frame_format_pointer (frame, &format);

  /* Walk the linked list of expressions, and count the number of
     expressions we find there.  */
  num_exprs = 0;
  for (expr = format->p_cfd_expr; expr; expr = expr->next)
    num_exprs++;

  return num_exprs;
}
#endif

#if 0
/* Function: get_frame_addr
 *
 * Description: If the input memory address was collected in the
 *     current trace frame, then lookup and return the address
 *     from within the trace buffer from which the collected byte
 * may be retrieved.  Else return -1.  */

unsigned char *
get_frame_addr ( const unsigned char *addr )
{
  unsigned char *base, *regs, *stack, *mem;
  CFD *dummy;
  DTC_RESPONSE ret;

  /* first, see if addr is on the saved piece of stack for curframe */
  if (curframe.format->stack_size > 0 &&
      (base = (unsigned char *) curframe.traceregs[A7]) <= addr  &&
      addr < base + curframe.format->stack_size)
    {
      gdb_puts("STUB: get_frame_addr: call get_addr_to_frame_regs_stack_mem\n");
      if ((ret = get_addr_to_frame_regs_stack_mem (curframe.frame_data,
						   &dummy,
						   (void *) &regs,
						   (void *) &stack,
                                                   (void *) &mem))
          != OK_TARGET_RESPONSE)
        return (void *) dtc_error_ret (-1,
                                       "get_addr_to_frame_regs_stack_mem",
                                       ret);
      else
        return stack + (addr - base);
    }

  /* Next, try to find addr in the current frame's expression-
     collected memory blocks.  I'm sure this is at least quadradic in
     time.  */
  {
    int num_exprs = count_frame_exprs (curframe.frame_data);
    int expr, block;

    /* Try each expression in turn.  */
    for (expr = 0; expr < num_exprs; expr++)
      {
	for (block = 0; ; block++)
	  {
	    T_EXPR_DATA *data;
	    if (adbg_get_expr_data (curframe.frame_data,
				    'x', expr, block,
				    &data)
		!= OK_TARGET_RESPONSE)
	      break;
	    else if ((unsigned char *) data->address <= addr
		     && addr < ((unsigned char *) data->address + data->size))
	      {
		/* We have found the right block; is it valid data?
		   Upper-case stamps mean bad data.  */
		if ('A' <= data->stamp && data->stamp <= 'Z')
		  {
		    gdb_puts("STUB: get_frame_addr: adbg_get_expr_data INVALID\n");
		    return (unsigned char *) -1;
		  }
		else
		  {
		    if (remote_debug > 1)
		      {
			sprintp(spare_buffer,
				"STUB: get_frame_addr: got it [%x,%x)\n",
				data->address, data->address + data->size);
			gdb_puts(spare_buffer);
		      }

		    return (((unsigned char *) &data->data)
			    + (addr - (unsigned char *) data->address));
		  }
	      }
	  }
      }
  }

  /* not found, return error */
  return (unsigned char *) -1;
}

/*============================================================*/

static long get_uchar ( const unsigned char * addr )
{
  unsigned char *frame_addr;

  if ( read_access_violation ( addr ) )
    return ( -1 ); /* Access error */

  if (curframe.valid)   /* if debugging a trace frame? */
    {
      /* If the requested address was collected in the current frame,
       * then fetch and return the data from the trace buffer.
       */
      if ((frame_addr = get_frame_addr (addr)) != (unsigned char *) -1)
        return ( *frame_addr );
      /* If the requested address is in the Code Section,
       * let's be magnanimous and read it anyway (else we shall
       * not be able to disassemble, find function prologues, etc.)
       */
      else if (CS_CODE_START <= (unsigned long) addr &&
               (unsigned long) addr < CS_CODE_START + CS_CODE_SIZE)
        return (*addr);
      else
        return ( -1 );  /* "Access error" (the data was not collected) */
    }
  else
    /* Not debugging a trace frame, read the data from live memory. */
    return ( *addr ); /* Meaningful result >= 0 */
}
#endif

/*============================================================*/

static long set_uchar ( unsigned char * addr, unsigned char val )
{
  long check_result = write_access_violation ( addr );

  if ( check_result != 0L )
    return ( check_result ); /* Access error */

  return ( *addr = val );    /* Successful writing */
}

/*============================================================*/

/*
 * Function read_access_violation() below returns TRUE if dereferencing
 * its argument for reading would cause a bus error - and FALSE otherwise:
 */

static long read_access_violation ( const void * addr )
{
  return ( ( ( addr < SRAM_START ) || ( addr >= SRAM_END ) ) &&
           ( ( addr < NVD_START )  || ( addr >= NVD_END ) ) );
}

/*============================================================*/

/*
 * Function write_access_violation() below returns zero if dereferencing
 * its argument for writing is safe, -1 on a soft error (the argument
 * falls into the write-protected area), -2 on a hard error (the argument
 * points to a non-existent memory location). In other words, it returns
 * FALSE when no bus error is expected - and an error code otherwise:
 */

static long write_access_violation ( const void * addr )
{
  /*
   * The boundaries of the write-protected area have to be received via
   * an API provided in the Symmetrix core code. For now, these limits
   * are hard-coded:
   */

  if ( ( addr >= RO_AREA_START ) && ( addr < RO_AREA_END ) )
    return ( -1 ); /* soft error */

  if ( ( ( addr < SRAM_START ) || ( addr >= SRAM_END ) ) &&
       ( ( addr < NVD_START )  || ( addr >= NVD_END ) ) )
    return ( -2 ); /* hard error */

  return ( 0 );
}


/* read_access_range is like read_access_violation,
   but returns the number of bytes we can read w/o faulting.
   that is, it checks an address range and tells us what portion
   (if any) of the prefix is safe to read without a bus error */
static long
read_access_range(const void *addr, long count)
{
  if ((addr >= SRAM_START) && (addr < SRAM_END))
    {
      if ((char *)addr + count < (char *)SRAM_END)
	return (count);
      else
	return ((char *)SRAM_END - (char *)addr);
    }
  else if (((char *)addr >= (char *)NVD_START) &&
	   ((char *)addr < (char *)NVD_END))
    {
      if ((char *)addr + count < (char *)NVD_END)
	return (count);
      else
	return ((char *)NVD_END - (char *)addr);
    }
  else
    return (0);
}

/* Convert the memory pointed to by mem into hex, placing result in buf.
   Return SUCCESS or FAIL.
   If MAY_FAULT is non-zero, then we should return FAIL in response to
   a fault; if zero treat a fault like any other fault in the stub.  */

static long
mem2hex(unsigned char *mem, char *buf, long count, long may_fault)
{
  long ndx;
  long ndx2;
  long ch;
  long incr;
  unsigned char *location;
  DTC_RESPONSE status;

  if (may_fault)
    {
      for (ndx = 0, incr = 1; (ndx < count) && (incr > 0); ndx += incr)
	{
	  status = find_memory(mem, count - ndx, &location, &incr);

	  if (status == OK_TARGET_RESPONSE)
	    {
	      if (incr > 0)
		{
		  for (ndx2 = 0; ndx2 < incr; ndx2++)
		    {
		      ch = *location++;
		      *buf++ = hexchars[ch >> 4];
		      *buf++ = hexchars[ch & 0xf];
		    }
		  mem += incr;
		}
	      else if (incr <= 0) /* should never happen */
		{
		  *buf = 0;
		  return (0);
		}
	    }
	  else if (status == NOT_FOUND_TARGET_RESPONSE)
	    {
	      *buf = 0;
	      return (ndx);	/* return amount copied */
	    }
	  else
	    {
	      *buf = 0;
	      return (0);	/* XXX: how do we tell the user the status? */
	    }
	}
      *buf = 0;
      return (count);
    }
  else
    {
      for (ndx = 0; ndx < count; ndx++)
	{
	  ch = *mem++;
	  *buf++ = hexchars[ch >> 4];
	  *buf++ = hexchars[ch & 0xf];
	}
      *buf = 0;
      return (count);		/* we copied everything */
    }
}

static DTC_RESPONSE
find_memory(unsigned char *mem, long count,
	    unsigned char **location, long *incr)
{
  DTC_RESPONSE retval;
  long length;

  /* figure out how much of the memory range we can read w/o faulting */
  count = read_access_range(mem, count);
  if (count == 0)
    return (NOT_FOUND_TARGET_RESPONSE);

  if (curframe.valid)
    {
      unsigned char *mem_block;
      unsigned char *mem_addr;
      unsigned long mem_size;
      unsigned long mem_stamp;

      retval = adbg_find_memory_addr_in_frame(curframe.frame_data, mem,
					      (unsigned long **)&mem_block,
					      (unsigned long **)&mem_addr,
					      &mem_size, &mem_stamp);

      switch (retval)
	{
	case OK_TARGET_RESPONSE:
#if 0
	  printp("FOUND: mem %x block %x addr %x size %d stamp %x\n",
		 mem, mem_block, mem_addr, mem_size, mem_stamp);
#endif
	  *location = mem_block + (mem - mem_addr);
	  length = mem_size - (mem - mem_addr);

	  if (length < count)
	    *incr = length;
	  else
	    *incr = count;

	  break;

	case NOT_FOUND_TARGET_RESPONSE:
	case NEAR_FOUND_TARGET_RESPONSE:
#if 0
	  printp("NOT FOUND: mem %x, checking code region\n", mem);
#endif
	  /* check to see if it's in the code region */
	  if ((CS_CODE_START <= (long)mem) &&
	      ((long)mem < CS_CODE_START + CS_CODE_SIZE))
	    {
	      /* some or all of the address range is in the code */
	      *location = mem;
	      if ((long)mem + count <= CS_CODE_START + CS_CODE_SIZE)
		*incr = count; /* it's totally in the code */
	      else
		/* how much is in the code? */
		*incr = CS_CODE_START + CS_CODE_SIZE - (long)mem;
#if 0
	      printp("FOUND in code region: %x\n", mem);
#endif
	      retval = OK_TARGET_RESPONSE;
	    }
	  else
	    retval = NOT_FOUND_TARGET_RESPONSE;

	  break;

	default:
#if 0
	  printp("BAD RETURN: %d\n", retval);
#endif
	  retval = NOT_FOUND_TARGET_RESPONSE;
	  break;
	}
    }
  else
    {
      *location = mem;
      *incr = count;
      retval = OK_TARGET_RESPONSE;
    }

  return (retval);
}

/* Convert the hex array pointed to by buf into binary to be placed in mem.
   Return SUCCESS or FAIL.  */

static long
hex2mem( char *buf, unsigned char *mem, long count, long may_fault )
{
  long i, ch;

  for ( i=0; i<count; i++ )
    {
      ch = hex( *buf++ ) << 4;
      ch = ch + hex( *buf++ );
      if ( may_fault )
        {
          ch = set_uchar( mem++, ch );
          if ( ch < 0 )    /* negative return indicates error */
            return FAIL;
        }
      else
        *mem++ = ch;
    }
  return SUCCESS;
}

/**********************************************/
/* WHILE WE FIND NICE HEX CHARS, BUILD AN INT */
/* RETURN NUMBER OF CHARS PROCESSED           */
/**********************************************/

static int
hexToInt( char **ptr, unsigned long *intValue )
{
  long numChars = 0;
  long hexValue;

  *intValue = 0;
  while ( **ptr )
    {
      hexValue = hex( **ptr );
      if ( hexValue >=0 )
        {
          *intValue = (*intValue << 4) | hexValue;
          numChars ++;
        }
      else
        break;
      (*ptr)++;
    }
  return numChars;
}

static volatile long gdb_handling_trap1;
static volatile long gdb_handling_sstrace;
static volatile long gdb_signo;

/*
   Here is the "callable" stub entry point.
   Call this function with a GDB request as an argument,
   and it will service the request and return.

   May be further broken up as we go along, with individual requests
   broken out as separate functions.
 */

static char * handle_trace_query (char *);
static char * handle_trace_set (char *);
static int    handle_format (char **request, CFD *format);
static unsigned long crc32 (unsigned char *buf, int len, unsigned long crc);
static char * crc_query (char *);
static char * handle_test (char *);

void
handle_request( char *request )
{
#if 0
  remote_debug = 2;
#endif
  switch( *request++ )
    {
    case 'k':		/* "kill" */
      curframe.valid = FALSE;
      putpacket ("");
      break;
    case 'D':		/* "detach" */
      curframe.valid = FALSE;
      putpacket ("");
      break;
    default:            /* Unknown code.  Return an empty reply message. */
      putpacket( "" );  /* return empty packet */
      break;

    case 'H':           /* Set thread for subsequent operations.
    Hct...                 c = 'c' for thread used in step and continue;
                           t... can be -1 for all threads.
                           c = 'g' for thread used in other operations.
                           If zero, pick a thread, any thread.  */

      putpacket( "OK" );
      break;

    case 'g':           /* Read registers.
                           Each byte of register data is described by
                           two hex digits.  registers are in the
                           internal order for GDB, and the bytes in a
                           register are in the same order the machine
                           uses.  */
      {
        /* Return the values in (one of) the registers cache(s).
           Several situations may pertain:
           1) We're synchronous, in which case the "registers" array
              should actually be current.
           2) We're asynchronous, in which case the "registers" array
              holds whatever was cached most recently.
           3) We're looking at a trace frame that was collected earlier:
              we will return those earlier registers.
         */

        /* all registers default to zero */
        memset (outbuffer, '0', NUMREGBYTES);
        outbuffer[NUMREGBYTES] = '\0';

        if (curframe.valid)     /* debugging a trace frame */
          mem2hex( (unsigned char*) curframe.traceregs,
                  outbuffer, NUMREGBYTES, 0 );
        else
          mem2hex( (unsigned char*) registers, outbuffer, NUMREGBYTES, 0 );

        putpacket( outbuffer );
      }
      break;
    case 'G':           /* Write registers.
    Gxxxxxxxx              Each byte of register data is described by
                           two hex digits.  */
      if (curframe.valid)       /* debugging a trace frame */
        putpacket ("E03");      /* can't write regs into a trace frame! */
      else
        {
          /* Write the values into the local registers cache...
             Note that no actual registers are being changed.  */

          hex2mem( request,
                  (unsigned char *) registers, NUMREGBYTES, 0 );
          putpacket( "OK" );
        }
      break;
    case 'P':           /* Write (single) register.
    Pnn=xxxxxxxx           register nn gets value xxxxxxxx;
                           two hex digits for each byte in the register
                           (target byte order).  */

      if (curframe.valid)
        putpacket ("E03");      /* can't write regs into a trace frame! */
      else
        {
          unsigned long regno;

          if ( hexToInt( &request, &regno ) && *(request++) == '=' )
            {
              if ( regno < NUMREGS )
                {
                  hexToInt( &request,
                           (unsigned long *) &registers[REGISTER_BYTE(regno)]);

                  putpacket( "OK" );
                }
              else
                putpacket( "E01" );   /* bad packet or regno */
            }
        }
      break;
    case 'm':           /* Read memory.
    mAAAAAAAA,LLLL         AAAAAAAA is address, LLLL is length.
                           Reply can be fewer bytes than requested
                           if able to read only part of the data.  */
      {
        unsigned long addr, len;

        if ( hexToInt( &request, &addr )    &&
             *(request++) == ','            &&
             hexToInt( &request, &len ) )
          {
            /* better not overwrite outbuffer! */
            if ( len > (BUFMAX / 2) - 5 )
              len = (BUFMAX / 2) - 5;
            if (mem2hex((unsigned char *) addr, outbuffer, len, 1) == 0) /* XXX: eventually use returned value */
              putpacket( "E03" );       /* read fault (access denied) */
            else
              putpacket( outbuffer );   /* read succeeded */
          }
        else
          putpacket( "E01" );           /* badly formed read request */

      }
      break;
    case 'M':           /* Write memory.
    Maaaaaaaa,llll:xxxx    aaaaaaaa is address, llll is length;
                           xxxx is data to write.  */

      {
        unsigned long addr, len;

        if (curframe.valid)     /* can't write memory into a trace frame! */
          putpacket ("E03");    /* "access denied" */
        else /*** if ( write_access_enabled ) ***/
          {
            if ( hexToInt( &request, &addr )  &&
                 *(request++) == ','          &&
                 hexToInt( &request, &len )   &&
                 *(request++) == ':' )
              {
                if (len == 2 &&
                    addr >= CS_CODE_START &&
                    addr <= LAST_CS_WORD)
                  {
                    unsigned long val;

                    if ( !hexToInt( &request, &val ) ||
                         write_to_protected_mem( (void *)addr, val ) )
                      putpacket( "E03" );   /* write fault (access denied) */
                    else
                      putpacket( "OK" );    /* write succeeded */
                  }
                else
                  {
                    if ( hex2mem( request, (unsigned char*) addr, len, 1 ) )
                      putpacket( "E03" );   /* write fault (access denied) */
                    else
                      putpacket( "OK" );    /* write succeeded */
                  }
              }
            else
                putpacket( "E02" );     /* badly formed write request */
          }
      }
      break;
    case 'c':           /* Continue.
    cAAAAAAAA              AAAAAAAA is address from which to resume.
                           If omitted, resume at current PC.  */

      {
        unsigned long addr;

	if (curframe.valid)
	  {
	    /* Don't continue if debugging a trace frame! */
	    gdb_puts ("Error: can't continue!\n");
	    putpacket ("S03");
	  }
	else
	  {
	    gdb_signo = 3;
	    if (isxdigit(request[0]))
	      {
		hexToInt(&request, &addr);
		registers[REGISTER_BYTE(PC)] = addr;
	      }

	    gdb_handling_trap1 = FALSE;
	    gdb_handling_sstrace = FALSE;
	    sss_trace_flag = '\0';
	  }
      }
      break;
    case 's':           /* Step.
    sAAAAAAAA              AAAAAAAA is address from which to begin stepping.
                           If omitted, begin stepping at current PC.  */
      {
        unsigned long addr;

	if (curframe.valid)
	  {
	    /* Don't step if debugging a trace frame! */
	    gdb_puts ("Error: can't step!\n");
	    putpacket ("S03");
	  }
	else
	  {
	    gdb_signo = 3;
	    if (isxdigit(request[0]))
	      {
		hexToInt(&request, &addr);
		registers[REGISTER_BYTE(PC)] = addr;
	      }

	    gdb_handling_trap1 = FALSE;
	    gdb_handling_sstrace = FALSE;
	    sss_trace_flag = 't';
	  }
      }
      break;
    case 'C':           /* Continue with signal.
    Cxx;AAAAAAAA           xx is signal number in hex;
                           AAAAAAAA is adddress from which to resume.
                           If ;AAAAAAAA omitted, continue from PC.   */

      {
        unsigned long addr = 0;

        if (!gdb_handling_trap1 || curframe.valid)
          {
	    /* Don't continue if not currently in synchronous mode,
	       or if currently debugging a trace frame!  */
            gdb_puts( "Error: can't continue!\n" );
            putpacket( "S03" );       /* "sigquit"  (better idea?) */
          }
        else
          {
	    gdb_signo = 3;
            if ( isxdigit( *request ) )
              {
                hex2mem( request, (unsigned char *) &gdb_signo, 2, 0 );
                request += 2;
                if ( *request == ';' && isxdigit( *++request ) )
                  {
                    hexToInt( &request, &addr );
                    registers[REGISTER_BYTE(PC)] = addr;
                  }
              }
            gdb_handling_trap1 = FALSE;
            gdb_handling_sstrace = FALSE;
            sss_trace_flag = '\0';
          }
      }
      break;
    case 'S':           /* Step with signal.
    Sxx;AAAAAAAA           xx is signal number in hex;
                           AAAAAAAA is adddress from which to begin stepping.
                           If ;AAAAAAAA omitted, begin stepping from PC.   */
      {
        unsigned long addr = 0;

        if (!gdb_handling_trap1 || curframe.valid)
          {
	    /* Don't step if not currently in synchronous mode,
	       or if currently debugging a trace frame!  */
            gdb_puts( "Error: can't step!\n" );
            putpacket( "S03" );       /* "sigquit"  (better idea?) */
          }
        else
          {
	    gdb_signo = 3;
            if ( isxdigit( *request ) )
              {
                hex2mem( request, (unsigned char *) &gdb_signo, 2, 0 );
                request += 2;
                if ( *request == ';' && isxdigit( *++request ) )
                  {
                    hexToInt( &request, &addr );
                    registers[REGISTER_BYTE(PC)] = addr;
                  }
              }
            gdb_handling_trap1 = FALSE;
            gdb_handling_sstrace = FALSE;
            sss_trace_flag = 't';
          }
      }
      break;
    case '?':           /* Query the latest reason for stopping.
                           Should be same reply as was last generated
                           for step or continue.  */

      if ( gdb_signo == 0 )
        gdb_signo = 3;  /* default to SIGQUIT */
      outbuffer[ 0 ] = 'S';
      outbuffer[ 1 ] = hexchars[ gdb_signo >>  4 ];
      outbuffer[ 2 ] = hexchars[ gdb_signo & 0xf ];
      outbuffer[ 3 ] = 0;
      putpacket( outbuffer );
      break;

    case 'd':           /* Toggle debug mode
                           I'm sure we can think of something interesting.  */

      remote_debug = !remote_debug;
      putpacket( "" );  /* return empty packet */
      break;

    case 'q':           /* general query */
      switch (*request++)
        {
        default:
          putpacket ("");       /* nak a request which we don't handle */
          break;
        case 'T':               /* trace query */
          putpacket (handle_trace_query (request));
          break;
	case 'C':		/* crc query (?) */
	  if (*request++ == 'R' &&
	      *request++ == 'C' &&
	      *request++ == ':')
	    putpacket (crc_query (request));
	  else
	    putpacket ("");	/* unknown query */
	  break;
        }
      break;

    case 'Q':                   /* general set */
      switch (*request++)
        {
        default:
          putpacket ("");       /* nak a request which we don't handle */
          break;
        case 'T':               /* trace */
          putpacket (handle_trace_set (request));
          break;
        }
      break;

    case 'T':
      /* call test function: TAAA,BBB,CCC 
	 A, B, and C are arguments to pass to gdb_c_test.  Reply is
	 "E01" (bad arguments) or "OK" (test function called).  */
      putpacket (handle_test (request));
      break;
    }
}

static TDP_SETUP_INFO tdp_temp;
static int trace_running;

/*
 * Function msgcmp:
 *
 * If second argument (str) is matched in first argument,
 *    then advance first argument past end of str and return "SAME"
 * else return "DIFFERENT" without changing first argument.
 *
 * Return: zero for DIFFERENT, non-zero for SUCCESS
 */

static int
msgcmp (char **msgp, char *str)
{
  char *next;

  if (msgp != 0 && str != 0)    /* input validation */
    if ((next = *msgp) != 0)
      {
        for (;
             *next && *str && *next == *str;
             next++, str++)
          ;

        if (*str == 0)                  /* matched all of str in msg */
          return (int) (*msgp = next);  /* advance msg ptr past str  */
      }
  return 0;                             /* failure */
}

static char *
handle_trace_query (char *request)
{
  if (msgcmp (&request, "Status"))
    {
      if (adbg_check_if_active ())
	{
	  gdb_puts ("Target trace is running.\n");
	  return "T1";
	}
      else
	{
	  gdb_puts ("Target trace not running.\n");
	  trace_running = 0;
	  return "T0";
	}
    }
  else                  /* unknown trace query */
    {
      return "";
    }
}

static void
gdb_note (char *fmt, int arg1)
{
  if (remote_debug > 1)
    {
      sprintp (spare_buffer, fmt, arg1);
      gdb_puts (spare_buffer);
    }
}

static int
error_ret (int ret, char *fmt, int arg1)
{
  if (remote_debug > 0)
    {
      sprintp (spare_buffer, fmt, arg1);
      gdb_puts (spare_buffer);
    }
  return ret;
}

static int
handle_format (char **request, COLLECTION_FORMAT_DEF *format)
{
  MEMRANGE_DEF m;
  DTC_RESPONSE ret;
  int elinum;
  unsigned long regnum;
  long bytecodes[(MAX_BYTE_CODES + sizeof (struct t_expr_tag))/ 4];
  struct t_expr_tag *t_expr = (struct t_expr_tag *)bytecodes;

  if (format->id == 0)
    {
      if ((ret = get_unused_format_id (&format->id)) != OK_TARGET_RESPONSE)
	return dtc_error_ret (-1, "get_unused_format_id", ret);

      if (**request == 'R')
	{
	  (*request)++;
	  hexToInt (request, &format->regs_mask);
	}
      gdb_note ("STUB: call define_format (id = %d, ", format->id);
      gdb_note ("regs_mask = 0x%X);\n", format->regs_mask);

      if ((ret = define_format (format)) != OK_TARGET_RESPONSE)
	{
	  sprintp (spare_buffer,
		   "'define_format': DTC error '%s' for format id %d.\n",
		   get_err_text (ret),
		   format->id);
	  gdb_puts (spare_buffer);
	  return -1;
	}
    }

  while ((**request == 'M') || (**request == 'X'))
    {
      switch (**request)
	{
	case 'M':		/* M<regnum>,<offset>,<size> */
	  (*request)++;
	  hexToInt(request, &regnum);

	  if (regnum == 0 || regnum == (unsigned long) -1)
	    m.typecode = -1;
	  else if ((elinum = index_to_elinum (regnum)) > 0)
	    m.typecode = elinum;
	  else
	    return error_ret (-1,
			      "Memrange register %d is not between 0 and 15\n",
			      regnum);

	  if (*(*request)++ != ',')
	    return error_ret (-1,"Malformed memrange (comma #%d missing)\n",1);
	  hexToInt(request, &m.offset);
	  if (*(*request)++ != ',')
	    return error_ret (-1,"Malformed memrange (comma #%d missing)\n",2);
	  hexToInt(request, &m.size);

	  gdb_note ("STUB: call add_format_mem_range (typecode =  0x%x, ",
		    m.typecode);
	  gdb_note ("offset = 0x%X, ", m.offset);
	  gdb_note ("size = %d);\n", m.size);
	  if ((ret = add_format_mem_ranges (format->id, &m)) != 
	      OK_TARGET_RESPONSE)
	    {
	      dtc_error_ret (-1, "add_format_mem_ranges", ret);
	      sprintp (spare_buffer,
		       "format id %d: memrange (0x%x, 0x%x, 0x%x).\n",
		       format->id, m.typecode, m.offset, m.size);
	      gdb_puts (spare_buffer);
	      return -1;
	    }
	  break;

	case 'X':		/* X<length>,<bytecodes> */
	  {
	    unsigned long length;

	    (*request)++;
	    hexToInt(request, &length);

	    if ((length <= 0) || (length > MAX_BYTE_CODES))
	      return error_ret (-1, 
				"Bytecode expression length (%d) too large\n",
				length);

	    if (*(*request)++ != ',')
	      return error_ret (-1, 
				"Malformed bytecode expr (comma#%d missing)\n",
				1);
	    t_expr->next = NULL;
	    /* subtract one to account for expr[0] in header */
	    t_expr->size = sizeof(struct t_expr_tag) + length - 1;
	    t_expr->expr_size = length;

	    hex2mem(*request, &t_expr->expr[0], length, 0);
	    *request += 2 * length;
	    build_and_add_expression(format->id, t_expr);
	  }
	  break;
	}
    }
  return 0;
}

static char *
handle_trace_set (char *request)
{
  long n_frame;
  unsigned long frameno, tdp, pc, start, stop;
  DTC_RESPONSE ret = -1;
  static COLLECTION_FORMAT_DEF tempfmt1;
  static char enable;
  static char retbuf[20];

  if (msgcmp (&request, "init"))
    {
      gdb_note ("STUB: call clear_trace_state();\n", 0);
      curframe.valid = 0;       /* all old frames become invalid now */
      if ((ret = clear_trace_state ()) == OK_TARGET_RESPONSE)
        return "OK";
      else
        {
          sprintp (retbuf, "E2%x", ret);
          return (char *) dtc_error_ret ((int) &retbuf,
                                         "clear_trace_state",
                                         ret);
        }
    }
  else if (msgcmp (&request, "Start"))
    {
      trace_running = 1;
      curframe.valid = 0;       /* all old frames become invalid now */
      gdb_note ("STUB: call start_trace_experiment();\n", 0);
      adbg_save_trace_in_nvd ();
      if ((ret = start_trace_experiment ()) == OK_TARGET_RESPONSE)
        return "OK";
      else
        {
          sprintp (retbuf, "E2%x", ret);
          return (char *) dtc_error_ret ((int) &retbuf,
                                         "start_trace_experiment",
                                         ret);
        }
    }
  else if (msgcmp (&request, "Stop"))
    {
      trace_running = 0;
      if (adbg_check_if_active ())
	{
	  gdb_note ("STUB: call end_trace_experiment();\n", 0);
	  if ((ret = end_trace_experiment ()) == OK_TARGET_RESPONSE)
	    return "OK";
	  else
	    {
	      sprintp (retbuf, "E2%x", ret);
	      return (char *) dtc_error_ret ((int) &retbuf,
					     "end_trace_experiment",
					     ret);
	    }
	}
      else return "OK";
    }
  /* "TDP:" (The 'T' was consumed in handle_request.)  */
  else if (msgcmp (&request, "DP:"))
    {
      /* TDP:<id>:<addr>:{D,E}:<stepcount>:<pass_limit>{R[M,X]+}<tdp-format>
                                                     {S{R[M,X]+}}<tp-format>

	 D -- disable tracepoint (illegal from EMC's point of view)
	 E -- enable tracepoint?

	 R -- regs format: R<regs-mask>
	 M -- memory format: M<regnum>,<offset>,<size>
	 X -- expr format: X<size>,<bytecodes>
	 S -- fencepost between trap formats and stepping formats.
	 */

      /* state variable, required for splitting TDP packets. */
      static int doing_step_formats;

      /* 
       * TDP: packets may now be split into multiple packets.
       * If a TDP packet is to be continued in another packet, it
       * must end in a "-" character.  The subsequent continuation
       * packet will then begin with a "-" character, between the
       * token "TDP:" and the tdp_id field.  The ID and address
       * will be repeated in each sub-packet.  The step_count, 
       * pass_count, and 'enabled' field must appear in the first
       * packet.  The boundary between sub-packets may not appear
       * between the "S" that denotes the start of stepping "formats",
       * and the regs_mask that follows it.  The split may also not
       * occur in the middle of either a memrange description or a
       * bytecode string.  -- MVS
       */

      if (*request == '-')	/* this is a continuation of a 
				   trace definition in progress */
	{
	  unsigned long temp_id, temp_addr;

	  request++;
	  if (!(hexToInt (&request, &temp_id) &&
		*request++ == ':'))
	    return "E11";           /* badly formed packet, field 1 */

	  if (!(hexToInt (&request, (unsigned long *) &temp_addr) &&
		*request++ == ':'))
	    return "E12";           /* badly formed packet, field 2 */

	  if (temp_id   != tdp_temp.id)
	    return "E11";	/* something wrong: field 1 doesn't match */
	  if (temp_addr != (unsigned long) tdp_temp.addr)
	    return "E12";	/* something wrong: field 2 doesn't match */
	}
      else			/* This is a new TDP definition */
	{
	  memset ((char *) &tdp_temp, 0, sizeof (tdp_temp));
	  memset ((char *) &tempfmt1, 0, sizeof (tempfmt1));
	  doing_step_formats = FALSE;

	  if (!(hexToInt (&request, &tdp_temp.id) &&
		*request++ == ':'))
	    return "E11";           /* badly formed packet, field 1 */

	  if (!(hexToInt (&request, (unsigned long *) &tdp_temp.addr) &&
		*request++ == ':'))
	    return "E12";           /* badly formed packet, field 2 */

	  if (!(((enable = *request++) == 'D' || enable == 'E') &&
		*request++ == ':'))
	    return "E13";           /* badly formed packet, field 3 */
#if 0
	  if (enable == 'D')
	    {
	      gdb_puts ("Disabling of tracepoints not supported by EMC target\n");
	      return "E20";
	    }
#endif
	  if (!(hexToInt (&request, &tdp_temp.stepcount) &&
		*request++ == ':'))
	    return "E14";           /* badly formed packet, field 4 */

	  if (!hexToInt (&request, &tdp_temp.pass_limit))
	    return "E15";           /* badly formed packet, field 5 */

	}

      /* Typically, the first group of collection descriptors
	 refers to the trap collection.  There is an "S" token
	 to act as a fencepost between collection descriptors for
	 the trap, and those for the single-stepping.

	 However, when the packet is split up into several packets,
	 this "S" token may already have been seen in a previous
	 sub-packet; so we have to remember it in a state variable.  */

      if (*request == 'R' || *request == 'M' || *request == 'X')
        {
          if (handle_format (&request, &tempfmt1))
            return "E16";
	  if (doing_step_formats)
	    tdp_temp.tp_format_p  = tempfmt1.id;
	  else
	    tdp_temp.tdp_format_p = tempfmt1.id;
        }

      /* When we see the "S" token, we remember it in a state variable
         (in case the packet is split up and continued in another message),
	 and discard all current state from the collection "format".  */
      if (*request == 'S')
	{
	  doing_step_formats = TRUE;
	  /* discard prev format and start a new one */
	  memset ((char *) &tempfmt1, 0, sizeof (tempfmt1));
	  request++;

	  /* Having seen the "S" fencepost, it is now possible that
	     we will see some more collection descriptors pertaining
	     to the stepping collection.  */
	  if (*request   == 'R' || *request == 'M' || *request == 'X')
	    {
	      if (handle_format (&request, &tempfmt1))
		return "E17";
	      /* new format ID is tp_format */
	      tdp_temp.tp_format_p = tempfmt1.id;
	    }
	}

      if (*request == '-')	/* this TDP definition will be continued. */
	sprintp (retbuf, "OK");
      else if (enable == 'E')	/* end of TDP definition: pass to ADBG (if enabled!) */
	{
	  gdb_note ("STUB: call define_tdp (id %d, ", tdp_temp.id);
	  gdb_note ("addr 0x%X, ", (int) tdp_temp.addr);
	  gdb_note ("passc %d, ",        tdp_temp.pass_limit);
	  gdb_note ("stepc %d, ",        tdp_temp.stepcount);
	  gdb_note ("TDP fmt #%d, ",     tdp_temp.tdp_format_p);
	  gdb_note ("TP fmt #%d);\n",    tdp_temp.tp_format_p);
	  
	  ret = define_tdp (tdp_temp.id, &tdp_temp, 0);

	  if (ret == OK_TARGET_RESPONSE)
	    {
	      sprintp (retbuf, "OK");
	    }
	  else
	    {
	      sprintp (spare_buffer,
		       "'define_tdp' returned DTC error '%s' for tracepoint %d.\n",
		       get_err_text (ret),
		       tdp_temp.id);
	      gdb_puts (spare_buffer);
	      sprintp (retbuf, "E2%x", ret);
	    }
	  /* Redundant, but let's try to make sure this state gets discarded. */
	  {
	    memset ((char *) &tdp_temp, 0, sizeof (tdp_temp));
	    memset ((char *) &tempfmt1, 0, sizeof (tempfmt1));
	  }
	}
      else /* ADBG_DTC does not support disabled tracepoints -- ignore it.  */
	gdb_note ("STUB: ignoring disabled tracepoint %d.\n", tdp_temp.id);

      return retbuf;
    }
  else if (msgcmp (&request, "Frame:"))
    {
      ret = OK_TARGET_RESPONSE;

      if (msgcmp (&request, "pc:"))
        {
          if (!hexToInt (&request, &pc))
            return "E10";       /* badly formed packet */
          n_frame = curframe.valid ? curframe.frame_id + 1 : 0;
          gdb_note ("STUB: call fetch_trace_frame_pc (id %d, ", n_frame);
          gdb_note ("pc 0x%X);\n", pc);
          ret = fetch_trace_frame_with_pc (&n_frame,
                                           (void *) pc,
                                           &curframe.format,
                                           &curframe.frame_data);
        }
      else if (msgcmp (&request, "tdp:"))
        {
          if (!hexToInt (&request, &tdp))
            return "E10";       /* badly formed packet */
          n_frame = curframe.valid ? curframe.frame_id + 1: 0;
          gdb_note ("STUB: call fetch_trace_frame_tdp (id %d, ", n_frame);
          gdb_note ("tdp 0x%X);\n", tdp);
          ret = fetch_trace_frame_with_tdp (&n_frame,
                                            tdp,
                                            &curframe.format,
                                            &curframe.frame_data);
        }
      else if (msgcmp (&request, "range:"))
        {
          if (!(hexToInt (&request, &start) &&
                *request++ == ':'))
            return "E11";      /* badly formed packet, field 1 */
          else if (!hexToInt (&request, &stop))
            return "E12";       /* badly formed packet, field 2 */
          n_frame = curframe.valid ? curframe.frame_id + 1: 0;
          gdb_note ("STUB: call fetch_trace_frame_range (id %d, ", n_frame);
          gdb_note ("start 0x%X, ",   start);
          gdb_note ("stop  0x%X);\n", stop);
          ret = fetch_trace_frame_with_pc_in_range (&n_frame,
                                                    (void *) start,
                                                    (void *) stop,
                                                    &curframe.format,
                                                    &curframe.frame_data);
        }
      else if (msgcmp (&request, "outside:"))
        {
          if (!(hexToInt (&request, &start) &&
                *request++ == ':'))
            return "E11";       /* badly formed packet, field 1 */
          else if (!hexToInt (&request, &stop))
            return "E12";       /* badly formed packet, field 2 */
          n_frame = curframe.valid ? curframe.frame_id + 1: 0;
          gdb_note ("STUB: call fetch_trace_frame_outside (id %d, ", n_frame);
          gdb_note ("start 0x%X, ",   start);
          gdb_note ("stop  0x%X);\n", stop);
          ret = fetch_trace_frame_with_pc_outside (&n_frame,
                                                   (void *) start,
                                                   (void *) stop,
                                                   &curframe.format,
                                                   &curframe.frame_data);
        }
      else /* simple TFind by frame number: */
        {
          if (!hexToInt (&request, &frameno))
            return "E10";       /* badly formed packet */
          if (frameno != (unsigned long) -1)
            {
              gdb_note ("STUB: call fetch_trace_frame (id %d);\n", frameno);
              ret = fetch_trace_frame (n_frame = frameno,
                                       &curframe.format,
                                       &curframe.frame_data);
#if 0
	      printp("STUB: fetch_trace_frame: return %d\n", ret);
#endif
            }
          else  /* discard any trace frame, debug "the real world" */
            {
              if (curframe.valid)
                gdb_note ("STUB: discard current trace frame #%d.\n",
                          curframe.frame_id);
              curframe.valid = 0;
              return "OK";
            }
        }
      if (ret == OK_TARGET_RESPONSE)    /* fetch_trace_frame succeeded */
        { /* setup for debugging the trace frame */
          curframe.valid    = 1;
          curframe.frame_id = n_frame;
          curframe.tdp_id   = curframe.frame_data->id;

          memset ((char *) &curframe.traceregs, 0,
                  sizeof (curframe.traceregs));
          curframe.traceregs[PC] = (unsigned long)
            curframe.frame_data->program_counter;

          if (curframe.format)
            {
              unsigned long regs_mask = curframe.format->regs_mask;
              unsigned long *regs, *stack, *mem;
              unsigned long regno, index = 0;
              CFD *dummy;

              if ((ret = get_addr_to_frame_regs_stack_mem
                   (curframe.frame_data, &dummy, &regs, &stack, &mem))
                  != OK_TARGET_RESPONSE)
                {
                  curframe.valid = 0;
                  sprintp (retbuf, "E2%x", ret);
                  return (char *)
                    dtc_error_ret ((int) &retbuf,
                                   "get_addr_to_frame_regs_stack_mem",
                                   ret);
                }

              if (remote_debug > 1)
                { /* echo what we've found to gdb console */
                  sprintp (spare_buffer,
                           "STUB: Found frame %d, TDP %d, format %d (%s):\n",
                           curframe.frame_id,
                           curframe.tdp_id & 0x7fffffff,
                           curframe.format->id,
                           curframe.tdp_id & 0x80000000 ?
                           "trap frame" : "stepping frame");
                  gdb_puts (spare_buffer);
                }
              /* copy trace frame regs into stub's data format */
              for (regno = 0, index = 0;
                   regno < 16;
                   regno++, regs_mask >>= 1)
                if (regs_mask & 1)      /* got a collected register */
                  {
                    curframe.traceregs[regno] = regs[index++];
                    if (remote_debug > 1)
                      {
                        sprintp (spare_buffer,
                                 "      Collected 0x%08x for register %d.\n",
                                 curframe.traceregs[regno], regno);
                        gdb_puts (spare_buffer);
                      }
                  }
              if (remote_debug > 1)
                {
                  long           midx, ridx, len;
                  MEMRANGE_DEF  *mrange;
                  unsigned char *data, *base;

                  if (curframe.format->stack_size > 0)
                    {
                      len = curframe.format->stack_size;
                      sprintp (spare_buffer,
                               "      Collected %d bytes of stack at 0x%x:\n",
                               len, curframe.traceregs[A7]);
                      gdb_puts (spare_buffer);

                      /* print stack data, but stay under msg len */
                      if (len >= (NUMREGBYTES/2 - 2))
                        len =    (NUMREGBYTES/2 - 3);
                      mem2hex ((unsigned char *) stack,
                               spare_buffer, len, 0);
                      spare_buffer [len * 2] = '\n';
                      spare_buffer [len * 2 + 1] = '\0'; /* EOS */
                      gdb_puts (spare_buffer);
                    }
		  else
		    gdb_puts ("Stack not collected\n");

                  for (midx = 0;
                       get_addr_to_a_mem_range (curframe.frame_data,
                                                midx,
                                                &mrange,
                                                (void **) &data)
                       == OK_TARGET_RESPONSE;
                       midx++)
                    {
                      if ((mrange->typecode == 0) ||
                          (mrange->typecode == (unsigned long) -1))
                        {
                          sprintp (spare_buffer,
                                   "      Collected %d bytes at MEM: 0x%x:\n",
                                   mrange->size, mrange->offset);
                          base = (unsigned char *) mrange->offset;
                        }
                      else
                        {
                          if ((ridx = elinum_to_index (mrange->typecode)) > 0)
                            base = (unsigned char *) curframe.traceregs[ridx]
                              + (long) mrange->offset;
                          else
                            {
                              sprintp (spare_buffer,
                   "STUB: bad typecode in memrange #%d: (0x%x,0x%x,0x%x).\n",
                                       midx,
                                       mrange->typecode,
                                       mrange->offset,
                                       mrange->size);
                              gdb_puts (spare_buffer);
                              continue;
                            }
                          sprintp (spare_buffer,
                   "      Collected %d bytes at 0x%x (REG %X + %d):\n",
                                   mrange->size,
                                   base,
                                   mrange->typecode,
                                   mrange->offset);
                        }
                      gdb_puts (spare_buffer);
                      len = mrange->size;
                      if (len >= (NUMREGBYTES/2 - 2))
                        len =    (NUMREGBYTES/2 - 3);
                      mem2hex (data, spare_buffer, len, 0);
                      spare_buffer [len * 2] = '\n';
                      spare_buffer [len * 2 + 1] = '\0'; /* EOS */
                      gdb_puts (spare_buffer);
                    }
                }
            }
          sprintp (retbuf, "F%xT%x", n_frame, curframe.tdp_id & 0x7fffffff);
          return   retbuf;
        }
      else if (ret == NOT_FOUND_TARGET_RESPONSE)
        {
          /* Here's a question: if the fetch_trace_frame call failed
             (which probably means a bad "TFIND" command from GDB),
             should we remain focused on the previous frame (if any),
             or should we revert to "no current frame"?
           */
          return "F-1";
        }
      else
        {
          sprintp (retbuf, "E2%x", ret);
          return (char *) dtc_error_ret ((int) &retbuf,
                                         "fetch_trace_frame[...]",
                                         ret);
        }
    }
  else                  /* unknown trace command */
    {
      return "";
    }
}

/* Table used by the crc32 function to calcuate the checksum. */
static unsigned long crc32_table[256];

static int crc_mem_err;

static unsigned long
crc32 (buf, len, crc)
     unsigned char *buf;
     int len;
     unsigned long crc;
{
  crc_mem_err = FALSE;

  if (! crc32_table[1])
    {
      /* Initialize the CRC table and the decoding table. */
      int i, j;
      unsigned int c;

      for (i = 0; i < 256; i++)
	{
	  for (c = i << 24, j = 8; j > 0; --j)
	    c = c & 0x80000000 ? (c << 1) ^ 0x04c11db7 : (c << 1);
	  crc32_table[i] = c;
	}
    }

  while (len--)
    {
      if (read_access_violation (buf))
	{
	  crc_mem_err = TRUE;
	  return -1;
	}
      crc = (crc << 8) ^ crc32_table[((crc >> 24) ^ *buf++) & 255];
    }
  return crc;
}

static char *
crc_query (cmd)
     char *cmd;
{
  unsigned long startmem, len, crc;
  static char buf[32];

  if (hexToInt (&cmd, &startmem) &&
      *cmd++ == ','              &&
      hexToInt (&cmd, &len))
    {
      crc = crc32  ((unsigned char *) startmem, len, 0xffffffff);
      if (!crc_mem_err)
	{
	  sprintp (buf, "C%08x", crc);
	  return buf;
	}
      /* else error, fall thru */
    }
  sprintp (buf, "E01");
  return buf;
}


static char *
handle_test (request)
     char *request;
{
  ULONG args[7];
  int i;

  /* Parse the arguments, a comma-separated list of hex numbers, into
     ARGS.  Parse at most six arguments.  */
  i = 1;
  if (*request != '\0')
    while (i < 7)
      {
	if (! hexToInt (&request, &args[i++]))
	  return "E01";
	if (*request == '\0')
	  break;
	if (*request++ != ',')
	  return "E01";
      }

  /* Fill the rest of the args array with zeros.  This is what the
     INLINES command processor does with omitted arguments.  */
  for (; i < 7; i++)
    args[i] = 0;

  gdb_c_test (args);

  return "OK";
}


/* GDB_TRAP_1_HANDLER

   By the time this is called, the registers have been saved in "registers",
   and the interrupt priority has been set to permit serial UART interrupts.

   However, since no gdb request has yet been received, and there is no
   equivalent of getpacket for us to wait on, we can't sit here waiting
   for packets and processing them.

   In fact, the ONLY thing for us to do here is sit and wait.
   As gdb sends packet requests, they will handle themselves at the
   interrupt level.  When gdb decides we can continue, it will reset
   the global variable "gdb_handling_trap1", and we will return
   (whereupon registers will be restored etc.)   */

void  gdb_trap_1_handler( void )
{
  gdb_handling_trap1 = TRUE;
  sss_trace_flag = '\0';        /* shut off "trace bit" (indirectly) */
  gdb_signo = 5;
  putpacket( "S05" );
  while ( gdb_handling_trap1 )
    ;
  return;
}

void  gdb_trace_handler( void )
{
  sss_trace_flag = '\0';        /* shut off "trace bit" (indirectly) */
  gdb_handling_trap1 = TRUE;
  gdb_handling_sstrace = TRUE;
  gdb_signo = 5;
  putpacket( "S05" );
  while ( gdb_handling_trap1 )
    ;
  return;
}
