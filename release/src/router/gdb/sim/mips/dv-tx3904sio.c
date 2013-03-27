/*  This file is part of the program GDB, the GNU debugger.
    
    Copyright (C) 1998, 1999, 2007 Free Software Foundation, Inc.
    Contributed by Cygnus Solutions.
    
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


#include "sim-main.h"
#include "hw-main.h"
#include "dv-sockser.h"
#include "sim-assert.h"


/* DEVICE

   
   tx3904sio - tx3904 serial I/O

   
   DESCRIPTION

   
   Implements one tx3904 serial I/O controller described in the tx3904
   user guide.  Three instances are required for SIO0 and SIO1 within
   the tx3904, at different base addresses.

   Both internal and system clocks are synthesized as divided versions
   of the simulator clock.
   
   There is no support for:
    - CTS/RTS flow control
    - baud rate emulation - use infinite speed instead
    - general frame format - use 8N1
    - multi-controller system
    - DMA - use interrupt-driven or polled-I/O instead


   PROPERTIES


   reg <base> <length>

   Base of SIO control register bank.  <length> must equal 0x100.
   Register offsets:       0: SLCR: line control register
                           4: SLSR: line status register
                           8: SDICR: DMA/interrupt control register
                          12: SDISR: DMA/interrupt status register
                          16: SFCR: FIFO control register
			  20: SBGR: baud rate control register
			  32: transfer FIFO buffer
			  48: transfer FIFO buffer

   backend {tcp | stdio}

   Use dv-sockser TCP-port backend or stdio for backend.  Default: stdio.



   PORTS


   int (output)

   Interrupt port.  An event is generated when a timer interrupt
   occurs.


   reset (input)

   Reset port.

   */



/* static functions */

struct tx3904sio_fifo;

static void tx3904sio_tickle(struct hw*);
static int tx3904sio_fifo_nonempty(struct hw*, struct tx3904sio_fifo*);
static char tx3904sio_fifo_pop(struct hw*, struct tx3904sio_fifo*);
static void tx3904sio_fifo_push(struct hw*, struct tx3904sio_fifo*, char);
static void tx3904sio_fifo_reset(struct hw*, struct tx3904sio_fifo*);
static void tx3904sio_poll(struct hw*, void* data);


/* register numbers; each is one word long */
enum 
{
  SLCR_REG = 0,
  SLSR_REG = 1,
  SDICR_REG = 2,
  SDISR_REG = 3,
  SFCR_REG = 4,
  SBGR_REG = 5,
  TFIFO_REG = 8,
  SFIFO_REG = 12,
};



/* port ID's */

enum
 {
  RESET_PORT,
  INT_PORT,
};


static const struct hw_port_descriptor tx3904sio_ports[] = 
{
  { "int", INT_PORT, 0, output_port, },
  { "reset", RESET_PORT, 0, input_port, },
  { NULL, },
};



/* Generic FIFO */
struct tx3904sio_fifo 
{
  int size, used;
  unsigned_1 *buffer;
};



/* The timer/counter register internal state.  Note that we store
   state using the control register images, in host endian order. */

struct tx3904sio 
{
  address_word base_address; /* control register base */
  enum {sio_tcp, sio_stdio} backend; /* backend */

  struct tx3904sio_fifo rx_fifo, tx_fifo; /* FIFOs */

  unsigned_4 slcr;
#define SLCR_WR_MASK        0xe17f0000U
#define SLCR_SET_BYTE(c,o,b) ((c)->slcr = SLCR_WR_MASK & (((c)->slcr & ~LSMASK32((o)*8+7,(o)*8)) | ((b)<< (o)*8)))
  unsigned_4 slsr;
#define SLSR_WR_MASK        0x00000000 /* UFER/UPER/UOER unimplemented */
  unsigned_4 sdicr;
#define SDICR_WR_MASK       0x000f0000U
#define SDICR_SET_BYTE(c,o,b) ((c)->sdicr = SDICR_WR_MASK & (((c)->sdicr & ~LSMASK32((o)*8+7,(o)*8)) | ((b)<< (o)*8)))
#define SDICR_GET_SDMAE(c)  ((c)->sdicr & 0x00080000)
#define SDICR_GET_ERIE(c)   ((c)->sdicr & 0x00040000)
#define SDICR_GET_TDIE(c)   ((c)->sdicr & 0x00020000)
#define SDICR_GET_RDIE(c)   ((c)->sdicr & 0x00010000)
  unsigned_4 sdisr;
#define SDISR_WR_MASK       0x00070000U
#define SDISR_SET_BYTE(c,o,b) ((c)->sdisr = SDISR_WR_MASK & (((c)->sdisr & ~LSMASK32((o)*8+7,(o)*8)) | ((b)<< (o)*8)))
#define SDISR_CLEAR_FLAG_BYTE(c,o,b) ((c)->sdisr = SDISR_WR_MASK & (((c)->sdisr & ~LSMASK32((o)*8+7,(o)*8)) & ((b)<< (o)*8)))
#define SDISR_GET_TDIS(c)   ((c)->sdisr & 0x00020000)
#define SDISR_SET_TDIS(c)   ((c)->sdisr |= 0x00020000)
#define SDISR_GET_RDIS(c)   ((c)->sdisr & 0x00010000)
#define SDISR_SET_RDIS(c)   ((c)->sdisr |= 0x00010000)
  unsigned_4 sfcr;
#define SFCR_WR_MASK       0x001f0000U
#define SFCR_SET_BYTE(c,o,b) ((c)->sfcr = SFCR_WR_MASK & (((c)->sfcr & ~LSMASK32((o)*8+7,(o)*8)) | ((b)<< (o)*8)))
#define SFCR_GET_TFRST(c)   ((c)->sfcr & 0x00040000)
#define SFCR_GET_RFRST(c)   ((c)->sfcr & 0x00020000)
#define SFCR_GET_FRSTE(c)   ((c)->sfcr & 0x00010000)
  unsigned_4 sbgr;
#define SBGR_WR_MASK       0x03ff0000U
#define SBGR_SET_BYTE(c,o,b) ((c)->sbgr = SBGR_WR_MASK & (((c)->sbgr & ~LSMASK32((o)*8+7,(o)*8)) | ((b)<< (o)*8)))

  /* Periodic I/O polling */
  struct hw_event* poll_event;
};



/* Finish off the partially created hw device.  Attach our local
   callbacks.  Wire up our port names etc */

static hw_io_read_buffer_method tx3904sio_io_read_buffer;
static hw_io_write_buffer_method tx3904sio_io_write_buffer;
static hw_port_event_method tx3904sio_port_event;


static void
attach_tx3904sio_regs (struct hw *me,
		      struct tx3904sio *controller)
{
  unsigned_word attach_address;
  int attach_space;
  unsigned attach_size;
  reg_property_spec reg;

  if (hw_find_property (me, "reg") == NULL)
    hw_abort (me, "Missing \"reg\" property");

  if (!hw_find_reg_array_property (me, "reg", 0, &reg))
    hw_abort (me, "\"reg\" property must contain one addr/size entry");

  hw_unit_address_to_attach_address (hw_parent (me),
				     &reg.address,
				     &attach_space,
				     &attach_address,
				     me);
  hw_unit_size_to_attach_size (hw_parent (me),
			       &reg.size,
			       &attach_size, me);

  hw_attach_address (hw_parent (me), 0,
		     attach_space, attach_address, attach_size,
		     me);

  if(hw_find_property(me, "backend") != NULL)
    {
      const char* value = hw_find_string_property(me, "backend");
      if(! strcmp(value, "tcp"))
	controller->backend = sio_tcp;
      else if(! strcmp(value, "stdio"))
	controller->backend = sio_stdio;
      else
	hw_abort(me, "illegal value for backend parameter `%s': use tcp or stdio", value);
    }

  controller->base_address = attach_address;
}


static void
tx3904sio_finish (struct hw *me)
{
  struct tx3904sio *controller;

  controller = HW_ZALLOC (me, struct tx3904sio);
  set_hw_data (me, controller);
  set_hw_io_read_buffer (me, tx3904sio_io_read_buffer);
  set_hw_io_write_buffer (me, tx3904sio_io_write_buffer);
  set_hw_ports (me, tx3904sio_ports);
  set_hw_port_event (me, tx3904sio_port_event);

  /* Preset defaults */
  controller->backend = sio_stdio;

  /* Attach ourself to our parent bus */
  attach_tx3904sio_regs (me, controller);

  /* Initialize to reset state */
  tx3904sio_fifo_reset(me, & controller->rx_fifo);
  tx3904sio_fifo_reset(me, & controller->tx_fifo);
  controller->slsr = controller->sdicr
    = controller->sdisr = controller->sfcr
    = controller->sbgr = 0;
  controller->slcr = 0x40000000; /* set TWUB */
  controller->sbgr = 0x03ff0000; /* set BCLK=3, BRD=FF */
  controller->poll_event = NULL;
}



/* An event arrives on an interrupt port */

static void
tx3904sio_port_event (struct hw *me,
		     int my_port,
		     struct hw *source,
		     int source_port,
		     int level)
{
  struct tx3904sio *controller = hw_data (me);

  switch (my_port)
    {
    case RESET_PORT:
      {
	HW_TRACE ((me, "reset"));

	tx3904sio_fifo_reset(me, & controller->rx_fifo);
	tx3904sio_fifo_reset(me, & controller->tx_fifo);
	controller->slsr = controller->sdicr
	  = controller->sdisr = controller->sfcr
	  = controller->sbgr = 0;
	controller->slcr = 0x40000000; /* set TWUB */
	controller->sbgr = 0x03ff0000; /* set BCLK=3, BRD=FF */
	/* Don't interfere with I/O poller. */
	break;
      }

    default:
      hw_abort (me, "Event on unknown port %d", my_port);
      break;
    }
}


/* generic read/write */

static unsigned
tx3904sio_io_read_buffer (struct hw *me,
			 void *dest,
			 int space,
			 unsigned_word base,
			 unsigned nr_bytes)
{
  struct tx3904sio *controller = hw_data (me);
  unsigned byte;

  HW_TRACE ((me, "read 0x%08lx %d", (long) base, (int) nr_bytes));

  /* tickle fifos */
  tx3904sio_tickle(me);

  for (byte = 0; byte < nr_bytes; byte++)
    {
      address_word address = base + byte;
      int reg_number = (address - controller->base_address) / 4;
      int reg_offset = (address - controller->base_address) % 4;
      unsigned_4 register_value; /* in target byte order */

      /* fill in entire register_value word */
      switch (reg_number)
	{
	case SLCR_REG: register_value = controller->slcr; break;
	case SLSR_REG: register_value = controller->slsr; break;
	case SDICR_REG: register_value = controller->sdicr; break;
	case SDISR_REG: register_value = controller->sdisr; break;
	case SFCR_REG: register_value = controller->sfcr; break;
	case SBGR_REG: register_value = controller->sbgr; break;
	case TFIFO_REG: register_value = 0; break;
	case SFIFO_REG:
	  /* consume rx fifo for MS byte */
	  if(reg_offset == 0 && tx3904sio_fifo_nonempty(me, & controller->rx_fifo))
	    register_value = (tx3904sio_fifo_pop(me, & controller->rx_fifo) << 24);
	  else
	    register_value = 0;
	  break;
	default: register_value = 0;
	}

      /* write requested byte out */
      register_value = H2T_4(register_value);
      /* HW_TRACE ((me, "byte %d %02x", reg_offset, ((char*)& register_value)[reg_offset])); */
      memcpy ((char*) dest + byte, ((char*)& register_value)+reg_offset, 1);
    }

  return nr_bytes;
}     



static unsigned
tx3904sio_io_write_buffer (struct hw *me,
			  const void *source,
			  int space,
			  unsigned_word base,
			  unsigned nr_bytes)
{
  struct tx3904sio *controller = hw_data (me);
  unsigned byte;

  HW_TRACE ((me, "write 0x%08lx %d", (long) base, (int) nr_bytes));
  for (byte = 0; byte < nr_bytes; byte++)
    {
      address_word address = base + byte;
      unsigned_1 write_byte = ((const unsigned char*) source)[byte];
      int reg_number = (address - controller->base_address) / 4;
      int reg_offset = 3 - (address - controller->base_address) % 4;

      /* HW_TRACE ((me, "byte %d %02x", reg_offset, write_byte)); */

      /* fill in entire register_value word */
      switch (reg_number)
	{
	case SLCR_REG:
	  SLCR_SET_BYTE(controller, reg_offset, write_byte);
	  break;

	case SLSR_REG: /* unwriteable */ break;

	case SDICR_REG:
	  {
	    unsigned_4 last_int, next_int;
	    
	    /* deassert interrupt upon clear */
	    last_int = controller->sdisr & controller->sdicr;
	    /* HW_TRACE ((me, "sdicr - sdisr %08x sdicr %08x",
	       controller->sdisr, controller->sdicr)); */
	    SDICR_SET_BYTE(controller, reg_offset, write_byte);
	    /* HW_TRACE ((me, "sdicr + sdisr %08x sdicr %08x",
	       controller->sdisr, controller->sdicr)); */
	    next_int = controller->sdisr & controller->sdicr;
	    
	    if(SDICR_GET_SDMAE(controller))
	      hw_abort(me, "Cannot support DMA-driven sio.");

	    if(~last_int & next_int) /* any bits set? */
	      hw_port_event(me, INT_PORT, 1);
	    if(last_int & ~next_int) /* any bits cleared? */
	      hw_port_event(me, INT_PORT, 0);
	  }
	break;

	case SDISR_REG:
	  {
	    unsigned_4 last_int, next_int;

	    /* deassert interrupt upon clear */
	    last_int = controller->sdisr & controller->sdicr;
	    /* HW_TRACE ((me, "sdisr - sdisr %08x sdicr %08x", 
	       controller->sdisr, controller->sdicr)); */
	    SDISR_CLEAR_FLAG_BYTE(controller, reg_offset, write_byte);
	    /* HW_TRACE ((me, "sdisr + sdisr %08x sdicr %08x", 
	       controller->sdisr, controller->sdicr)); */
	    next_int = controller->sdisr & controller->sdicr;

	    if(~last_int & next_int) /* any bits set? */
	      hw_port_event(me, INT_PORT, 1);
	    if(last_int & ~next_int) /* any bits cleared? */
	      hw_port_event(me, INT_PORT, 0);
	  }
	break;
	
	case SFCR_REG:
	  SFCR_SET_BYTE(controller, reg_offset, write_byte);
	  if(SFCR_GET_FRSTE(controller))
	    {
	      if(SFCR_GET_TFRST(controller)) tx3904sio_fifo_reset(me, & controller->tx_fifo);
	      if(SFCR_GET_RFRST(controller)) tx3904sio_fifo_reset(me, & controller->rx_fifo);
	    }
	  break;
	  
	case SBGR_REG:
	  SBGR_SET_BYTE(controller, reg_offset, write_byte);
	  break;
	  
	case SFIFO_REG: /* unwriteable */ break;
	  
	case TFIFO_REG: 
	  if(reg_offset == 3) /* first byte */
	    tx3904sio_fifo_push(me, & controller->tx_fifo, write_byte);
	  break;

	default: 
	  HW_TRACE ((me, "write to illegal register %d", reg_number));
	}
    } /* loop over bytes */

  /* tickle fifos */
  tx3904sio_tickle(me);

  return nr_bytes;
}     






/* Send enqueued characters from tx_fifo and trigger TX interrupt.
Receive characters into rx_fifo and trigger RX interrupt. */
void
tx3904sio_tickle(struct hw *me)
{
  struct tx3904sio* controller = hw_data(me);
  int c;
  char cc;
  unsigned_4 last_int, next_int;

  /* HW_TRACE ((me, "tickle backend: %02x", controller->backend)); */
  switch(controller->backend) 
    {
    case sio_tcp:

      while(tx3904sio_fifo_nonempty(me, & controller->tx_fifo))
	{
	  cc = tx3904sio_fifo_pop(me, & controller->tx_fifo);
	  dv_sockser_write(hw_system(me), cc);
	  HW_TRACE ((me, "tcp output: %02x", cc));
	}

      c = dv_sockser_read(hw_system(me));
      while(c != -1)
	{
	  cc = (char) c;
	  HW_TRACE ((me, "tcp input: %02x", cc));
	  tx3904sio_fifo_push(me, & controller->rx_fifo, cc);
	  c = dv_sockser_read(hw_system(me));
	}
      break;

    case sio_stdio:

      while(tx3904sio_fifo_nonempty(me, & controller->tx_fifo))
	{
	  cc = tx3904sio_fifo_pop(me, & controller->tx_fifo);
	  sim_io_write_stdout(hw_system(me), & cc, 1);
	  sim_io_flush_stdout(hw_system(me));
	  HW_TRACE ((me, "stdio output: %02x", cc));
	}

      c = sim_io_poll_read(hw_system(me), 0 /* stdin */, & cc, 1);
      while(c == 1)
	{
	  HW_TRACE ((me, "stdio input: %02x", cc));
	  tx3904sio_fifo_push(me, & controller->rx_fifo, cc);
	  c = sim_io_poll_read(hw_system(me), 0 /* stdin */, & cc, 1);
	}

      break;

    default:
      hw_abort(me, "Illegal backend mode: %d", controller->backend);
    }

  /* Update RDIS / TDIS flags */
  last_int = controller->sdisr & controller->sdicr;
  /* HW_TRACE ((me, "tickle - sdisr %08x sdicr %08x", controller->sdisr, controller->sdicr)); */
  if(tx3904sio_fifo_nonempty(me, & controller->rx_fifo))
    SDISR_SET_RDIS(controller);
  if(! tx3904sio_fifo_nonempty(me, & controller->tx_fifo))
    SDISR_SET_TDIS(controller);
  next_int = controller->sdisr & controller->sdicr;
  /* HW_TRACE ((me, "tickle + sdisr %08x sdicr %08x", controller->sdisr, controller->sdicr)); */

  if(~last_int & next_int) /* any bits set? */
    hw_port_event(me, INT_PORT, 1);
  if(last_int & ~next_int) /* any bits cleared? */
    hw_port_event(me, INT_PORT, 0);

  /* Add periodic polling for this port, if it's not already going. */
  if(controller->poll_event == NULL)
    {
      controller->poll_event = hw_event_queue_schedule (me, 1000,
							tx3904sio_poll, NULL);

    }
}




int
tx3904sio_fifo_nonempty(struct hw* me, struct tx3904sio_fifo* fifo)
{
  /* HW_TRACE ((me, "fifo used: %d", fifo->used)); */
  return(fifo->used > 0);
}


char
tx3904sio_fifo_pop(struct hw* me, struct tx3904sio_fifo* fifo)
{
  char it;
  ASSERT(fifo->used > 0);
  ASSERT(fifo->buffer != NULL);
  it = fifo->buffer[0];
  memcpy(& fifo->buffer[0], & fifo->buffer[1], fifo->used - 1);
  fifo->used --;
  /* HW_TRACE ((me, "pop fifo -> %02x", it)); */
  return it;
}


void
tx3904sio_fifo_push(struct hw* me, struct tx3904sio_fifo* fifo, char it)
{
  /* HW_TRACE ((me, "push %02x -> fifo", it)); */
  if(fifo->size == fifo->used) /* full */
    {
      int next_size = fifo->size * 2 + 16;
      char* next_buf = zalloc(next_size);
      memcpy(next_buf, fifo->buffer, fifo->used);

      if(fifo->buffer != NULL) zfree(fifo->buffer);
      fifo->buffer = next_buf;
      fifo->size = next_size;
    }

  fifo->buffer[fifo->used] = it;
  fifo->used ++;
}


void
tx3904sio_fifo_reset(struct hw* me, struct tx3904sio_fifo* fifo)
{
  /* HW_TRACE ((me, "reset fifo")); */
  fifo->used = 0;
  fifo->size = 0;
  zfree(fifo->buffer);
  fifo->buffer = 0;
}


void
tx3904sio_poll(struct hw* me, void* ignored)
{
  struct tx3904sio* controller = hw_data (me);
  tx3904sio_tickle (me);
  hw_event_queue_deschedule (me, controller->poll_event);
  controller->poll_event = hw_event_queue_schedule (me, 1000,
						    tx3904sio_poll, NULL);
}



const struct hw_descriptor dv_tx3904sio_descriptor[] = {
  { "tx3904sio", tx3904sio_finish, },
  { NULL },
};
