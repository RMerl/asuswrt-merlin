/*  This file is part of the program psim.

    Copyright (C) 1994-1996, Andrew Cagney <cagney@highland.com.au>

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


#ifndef _HW_CPU_H_
#define _HW_CPU_H_

enum {
  hw_cpu_hard_reset,
  hw_cpu_soft_reset,
  hw_cpu_external_interrupt,
  hw_cpu_machine_check_interrupt,
  hw_cpu_system_management_interrupt,
  hw_cpu_nr_interrupt_ports
};

#endif /* _HW_CPU_H_ */
