/* Simulator header for m32r.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

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

#ifndef M32R_ARCH_H
#define M32R_ARCH_H

#define TARGET_BIG_ENDIAN 1

/* Enum declaration for model types.  */
typedef enum model_type {
  MODEL_M32R_D, MODEL_TEST, MODEL_M32RX, MODEL_M32R2
 , MODEL_MAX
} MODEL_TYPE;

#define MAX_MODELS ((int) MODEL_MAX)

/* Enum declaration for unit types.  */
typedef enum unit_type {
  UNIT_NONE, UNIT_M32R_D_U_STORE, UNIT_M32R_D_U_LOAD, UNIT_M32R_D_U_CTI
 , UNIT_M32R_D_U_MAC, UNIT_M32R_D_U_CMP, UNIT_M32R_D_U_EXEC, UNIT_TEST_U_EXEC
 , UNIT_M32RX_U_STORE, UNIT_M32RX_U_LOAD, UNIT_M32RX_U_CTI, UNIT_M32RX_U_MAC
 , UNIT_M32RX_U_CMP, UNIT_M32RX_U_EXEC, UNIT_M32R2_U_STORE, UNIT_M32R2_U_LOAD
 , UNIT_M32R2_U_CTI, UNIT_M32R2_U_MAC, UNIT_M32R2_U_CMP, UNIT_M32R2_U_EXEC
 , UNIT_MAX
} UNIT_TYPE;

#define MAX_UNITS (2)

#endif /* M32R_ARCH_H */
