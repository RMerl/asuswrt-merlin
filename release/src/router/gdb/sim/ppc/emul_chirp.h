/*  This file is part of the program psim.

    Copyright (C) 1994-1997, Andrew Cagney <cagney@highland.com.au>

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
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


#ifndef _EMUL_CHIRP_H_
#define _EMUL_CHIRP_H_

/* EMUL_CHIRP:

   The emulation of the OpenBoot client interface (as defined in 1275)
   illustrates how it is possible for PSIM to implement an interface
   that is both running in virtual memory and is called using a
   standard function call interface.

   The OpenBoot client interface is implemented by using two
   instructions:

   	client_interface:
		<emul_call>
		blr

   A client program makes a function call to `client_interface' using
   the `bl' instruction.  The simulator will then execute the
   <emul_call> instruction (which calls emul_chirp) and then the `blr'
   which will return to the caller.

   In addition to providing the `client_interface' entry point, while
   a client request is being handled, emul_chirp patches (well it will
   one day) the data access exception vector with a <emul_call>
   instruction.  By doing this, emul_chirp is able to catch and handle
   any invalid data accesses it makes while emulating a client call.

   When such an exception occures, emul_chirp is able to recover by
   restoring the processor and then calling the clients callback
   interface so that the client can recover from the data exception.

   Handling this are the emul_chirp states:

                              serving---.
                             /          |
   Emulation compleated     ^           v Client makes call to
     - restore int vectors  |           | emulated interface
                            ^           v   - patch exception vectors
                            |          /
                            `-emulating-. emulating the request
                             /          |
                            |           v Emulation encounters
   Client callback recovers ^           | data access exception
   from data exception and  |           v   - re-enable vm
   returns.                 ^           |   - call client callback
     - restart request      |          /
                            `--faulting
   */


extern const os_emul emul_chirp;

#endif
