/******************************************************************************
 *
 * Module Name: psutils - Parser miscellaneous utilities (Parser only)
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2010, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include <acpi/acpi.h>
#include "accommon.h"
#include "acparser.h"
#include "amlcode.h"

#define _COMPONENT          ACPI_PARSER
ACPI_MODULE_NAME("psutils")

/*******************************************************************************
 *
 * FUNCTION:    acpi_ps_create_scope_op
 *
 * PARAMETERS:  None
 *
 * RETURN:      A new Scope object, null on failure
 *
 * DESCRIPTION: Create a Scope and associated namepath op with the root name
 *
 ******************************************************************************/
union acpi_parse_object *acpi_ps_create_scope_op(void)
{
	union acpi_parse_object *scope_op;

	scope_op = acpi_ps_alloc_op(AML_SCOPE_OP);
	if (!scope_op) {
		return (NULL);
	}

	scope_op->named.name = ACPI_ROOT_NAME;
	return (scope_op);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ps_init_op
 *
 * PARAMETERS:  Op              - A newly allocated Op object
 *              Opcode          - Opcode to store in the Op
 *
 * RETURN:      None
 *
 * DESCRIPTION: Initialize a parse (Op) object
 *
 ******************************************************************************/

void acpi_ps_init_op(union acpi_parse_object *op, u16 opcode)
{
	ACPI_FUNCTION_ENTRY();

	op->common.descriptor_type = ACPI_DESC_TYPE_PARSER;
	op->common.aml_opcode = opcode;

	ACPI_DISASM_ONLY_MEMBERS(ACPI_STRNCPY(op->common.aml_op_name,
					      (acpi_ps_get_opcode_info
					       (opcode))->name,
					      sizeof(op->common.aml_op_name)));
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ps_alloc_op
 *
 * PARAMETERS:  Opcode          - Opcode that will be stored in the new Op
 *
 * RETURN:      Pointer to the new Op, null on failure
 *
 * DESCRIPTION: Allocate an acpi_op, choose op type (and thus size) based on
 *              opcode.  A cache of opcodes is available for the pure
 *              GENERIC_OP, since this is by far the most commonly used.
 *
 ******************************************************************************/

union acpi_parse_object *acpi_ps_alloc_op(u16 opcode)
{
	union acpi_parse_object *op;
	const struct acpi_opcode_info *op_info;
	u8 flags = ACPI_PARSEOP_GENERIC;

	ACPI_FUNCTION_ENTRY();

	op_info = acpi_ps_get_opcode_info(opcode);

	/* Determine type of parse_op required */

	if (op_info->flags & AML_DEFER) {
		flags = ACPI_PARSEOP_DEFERRED;
	} else if (op_info->flags & AML_NAMED) {
		flags = ACPI_PARSEOP_NAMED;
	} else if (opcode == AML_INT_BYTELIST_OP) {
		flags = ACPI_PARSEOP_BYTELIST;
	}

	/* Allocate the minimum required size object */

	if (flags == ACPI_PARSEOP_GENERIC) {

		/* The generic op (default) is by far the most common (16 to 1) */

		op = acpi_os_acquire_object(acpi_gbl_ps_node_cache);
	} else {
		/* Extended parseop */

		op = acpi_os_acquire_object(acpi_gbl_ps_node_ext_cache);
	}

	/* Initialize the Op */

	if (op) {
		acpi_ps_init_op(op, opcode);
		op->common.flags = flags;
	}

	return (op);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ps_free_op
 *
 * PARAMETERS:  Op              - Op to be freed
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Free an Op object.  Either put it on the GENERIC_OP cache list
 *              or actually free it.
 *
 ******************************************************************************/

void acpi_ps_free_op(union acpi_parse_object *op)
{
	ACPI_FUNCTION_NAME(ps_free_op);

	if (op->common.aml_opcode == AML_INT_RETURN_VALUE_OP) {
		ACPI_DEBUG_PRINT((ACPI_DB_ALLOCATIONS, "Free retval op: %p\n",
				  op));
	}

	if (op->common.flags & ACPI_PARSEOP_GENERIC) {
		(void)acpi_os_release_object(acpi_gbl_ps_node_cache, op);
	} else {
		(void)acpi_os_release_object(acpi_gbl_ps_node_ext_cache, op);
	}
}

/*******************************************************************************
 *
 * FUNCTION:    Utility functions
 *
 * DESCRIPTION: Low level character and object functions
 *
 ******************************************************************************/

/*
 * Is "c" a namestring lead character?
 */
u8 acpi_ps_is_leading_char(u32 c)
{
	return ((u8) (c == '_' || (c >= 'A' && c <= 'Z')));
}

/*
 * Is "c" a namestring prefix character?
 */
u8 acpi_ps_is_prefix_char(u32 c)
{
	return ((u8) (c == '\\' || c == '^'));
}

/*
 * Get op's name (4-byte name segment) or 0 if unnamed
 */
#ifdef ACPI_FUTURE_USAGE
u32 acpi_ps_get_name(union acpi_parse_object * op)
{

	/* The "generic" object has no name associated with it */

	if (op->common.flags & ACPI_PARSEOP_GENERIC) {
		return (0);
	}

	/* Only the "Extended" parse objects have a name */

	return (op->named.name);
}
#endif				/*  ACPI_FUTURE_USAGE  */

/*
 * Set op's name
 */
void acpi_ps_set_name(union acpi_parse_object *op, u32 name)
{

	/* The "generic" object has no name associated with it */

	if (op->common.flags & ACPI_PARSEOP_GENERIC) {
		return;
	}

	op->named.name = name;
}
