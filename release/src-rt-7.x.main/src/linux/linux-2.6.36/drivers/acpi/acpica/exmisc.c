
/******************************************************************************
 *
 * Module Name: exmisc - ACPI AML (p-code) execution - specific opcodes
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
#include "acinterp.h"
#include "amlcode.h"
#include "amlresrc.h"

#define _COMPONENT          ACPI_EXECUTER
ACPI_MODULE_NAME("exmisc")

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_get_object_reference
 *
 * PARAMETERS:  obj_desc            - Create a reference to this object
 *              return_desc         - Where to store the reference
 *              walk_state          - Current state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Obtain and return a "reference" to the target object
 *              Common code for the ref_of_op and the cond_ref_of_op.
 *
 ******************************************************************************/
acpi_status
acpi_ex_get_object_reference(union acpi_operand_object *obj_desc,
			     union acpi_operand_object **return_desc,
			     struct acpi_walk_state *walk_state)
{
	union acpi_operand_object *reference_obj;
	union acpi_operand_object *referenced_obj;

	ACPI_FUNCTION_TRACE_PTR(ex_get_object_reference, obj_desc);

	*return_desc = NULL;

	switch (ACPI_GET_DESCRIPTOR_TYPE(obj_desc)) {
	case ACPI_DESC_TYPE_OPERAND:

		if (obj_desc->common.type != ACPI_TYPE_LOCAL_REFERENCE) {
			return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
		}

		/*
		 * Must be a reference to a Local or Arg
		 */
		switch (obj_desc->reference.class) {
		case ACPI_REFCLASS_LOCAL:
		case ACPI_REFCLASS_ARG:
		case ACPI_REFCLASS_DEBUG:

			/* The referenced object is the pseudo-node for the local/arg */

			referenced_obj = obj_desc->reference.object;
			break;

		default:

			ACPI_ERROR((AE_INFO, "Unknown Reference Class 0x%2.2X",
				    obj_desc->reference.class));
			return_ACPI_STATUS(AE_AML_INTERNAL);
		}
		break;

	case ACPI_DESC_TYPE_NAMED:

		/*
		 * A named reference that has already been resolved to a Node
		 */
		referenced_obj = obj_desc;
		break;

	default:

		ACPI_ERROR((AE_INFO, "Invalid descriptor type 0x%X",
			    ACPI_GET_DESCRIPTOR_TYPE(obj_desc)));
		return_ACPI_STATUS(AE_TYPE);
	}

	/* Create a new reference object */

	reference_obj =
	    acpi_ut_create_internal_object(ACPI_TYPE_LOCAL_REFERENCE);
	if (!reference_obj) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	reference_obj->reference.class = ACPI_REFCLASS_REFOF;
	reference_obj->reference.object = referenced_obj;
	*return_desc = reference_obj;

	ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
			  "Object %p Type [%s], returning Reference %p\n",
			  obj_desc, acpi_ut_get_object_type_name(obj_desc),
			  *return_desc));

	return_ACPI_STATUS(AE_OK);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_concat_template
 *
 * PARAMETERS:  Operand0            - First source object
 *              Operand1            - Second source object
 *              actual_return_desc  - Where to place the return object
 *              walk_state          - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Concatenate two resource templates
 *
 ******************************************************************************/

acpi_status
acpi_ex_concat_template(union acpi_operand_object *operand0,
			union acpi_operand_object *operand1,
			union acpi_operand_object **actual_return_desc,
			struct acpi_walk_state *walk_state)
{
	acpi_status status;
	union acpi_operand_object *return_desc;
	u8 *new_buf;
	u8 *end_tag;
	acpi_size length0;
	acpi_size length1;
	acpi_size new_length;

	ACPI_FUNCTION_TRACE(ex_concat_template);

	/*
	 * Find the end_tag descriptor in each resource template.
	 * Note1: returned pointers point TO the end_tag, not past it.
	 * Note2: zero-length buffers are allowed; treated like one end_tag
	 */

	/* Get the length of the first resource template */

	status = acpi_ut_get_resource_end_tag(operand0, &end_tag);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	length0 = ACPI_PTR_DIFF(end_tag, operand0->buffer.pointer);

	/* Get the length of the second resource template */

	status = acpi_ut_get_resource_end_tag(operand1, &end_tag);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	length1 = ACPI_PTR_DIFF(end_tag, operand1->buffer.pointer);

	/* Combine both lengths, minimum size will be 2 for end_tag */

	new_length = length0 + length1 + sizeof(struct aml_resource_end_tag);

	/* Create a new buffer object for the result (with one end_tag) */

	return_desc = acpi_ut_create_buffer_object(new_length);
	if (!return_desc) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	/*
	 * Copy the templates to the new buffer, 0 first, then 1 follows. One
	 * end_tag descriptor is copied from Operand1.
	 */
	new_buf = return_desc->buffer.pointer;
	ACPI_MEMCPY(new_buf, operand0->buffer.pointer, length0);
	ACPI_MEMCPY(new_buf + length0, operand1->buffer.pointer, length1);

	/* Insert end_tag and set the checksum to zero, means "ignore checksum" */

	new_buf[new_length - 1] = 0;
	new_buf[new_length - 2] = ACPI_RESOURCE_NAME_END_TAG | 1;

	/* Return the completed resource template */

	*actual_return_desc = return_desc;
	return_ACPI_STATUS(AE_OK);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_do_concatenate
 *
 * PARAMETERS:  Operand0            - First source object
 *              Operand1            - Second source object
 *              actual_return_desc  - Where to place the return object
 *              walk_state          - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Concatenate two objects OF THE SAME TYPE.
 *
 ******************************************************************************/

acpi_status
acpi_ex_do_concatenate(union acpi_operand_object *operand0,
		       union acpi_operand_object *operand1,
		       union acpi_operand_object **actual_return_desc,
		       struct acpi_walk_state *walk_state)
{
	union acpi_operand_object *local_operand1 = operand1;
	union acpi_operand_object *return_desc;
	char *new_buf;
	acpi_status status;

	ACPI_FUNCTION_TRACE(ex_do_concatenate);

	/*
	 * Convert the second operand if necessary.  The first operand
	 * determines the type of the second operand, (See the Data Types
	 * section of the ACPI specification.)  Both object types are
	 * guaranteed to be either Integer/String/Buffer by the operand
	 * resolution mechanism.
	 */
	switch (operand0->common.type) {
	case ACPI_TYPE_INTEGER:
		status =
		    acpi_ex_convert_to_integer(operand1, &local_operand1, 16);
		break;

	case ACPI_TYPE_STRING:
		status = acpi_ex_convert_to_string(operand1, &local_operand1,
						   ACPI_IMPLICIT_CONVERT_HEX);
		break;

	case ACPI_TYPE_BUFFER:
		status = acpi_ex_convert_to_buffer(operand1, &local_operand1);
		break;

	default:
		ACPI_ERROR((AE_INFO, "Invalid object type: 0x%X",
			    operand0->common.type));
		status = AE_AML_INTERNAL;
	}

	if (ACPI_FAILURE(status)) {
		goto cleanup;
	}

	/*
	 * Both operands are now known to be the same object type
	 * (Both are Integer, String, or Buffer), and we can now perform the
	 * concatenation.
	 */

	/*
	 * There are three cases to handle:
	 *
	 * 1) Two Integers concatenated to produce a new Buffer
	 * 2) Two Strings concatenated to produce a new String
	 * 3) Two Buffers concatenated to produce a new Buffer
	 */
	switch (operand0->common.type) {
	case ACPI_TYPE_INTEGER:

		/* Result of two Integers is a Buffer */
		/* Need enough buffer space for two integers */

		return_desc = acpi_ut_create_buffer_object((acpi_size)
							   ACPI_MUL_2
							   (acpi_gbl_integer_byte_width));
		if (!return_desc) {
			status = AE_NO_MEMORY;
			goto cleanup;
		}

		new_buf = (char *)return_desc->buffer.pointer;

		/* Copy the first integer, LSB first */

		ACPI_MEMCPY(new_buf, &operand0->integer.value,
			    acpi_gbl_integer_byte_width);

		/* Copy the second integer (LSB first) after the first */

		ACPI_MEMCPY(new_buf + acpi_gbl_integer_byte_width,
			    &local_operand1->integer.value,
			    acpi_gbl_integer_byte_width);
		break;

	case ACPI_TYPE_STRING:

		/* Result of two Strings is a String */

		return_desc = acpi_ut_create_string_object(((acpi_size)
							    operand0->string.
							    length +
							    local_operand1->
							    string.length));
		if (!return_desc) {
			status = AE_NO_MEMORY;
			goto cleanup;
		}

		new_buf = return_desc->string.pointer;

		/* Concatenate the strings */

		ACPI_STRCPY(new_buf, operand0->string.pointer);
		ACPI_STRCPY(new_buf + operand0->string.length,
			    local_operand1->string.pointer);
		break;

	case ACPI_TYPE_BUFFER:

		/* Result of two Buffers is a Buffer */

		return_desc = acpi_ut_create_buffer_object(((acpi_size)
							    operand0->buffer.
							    length +
							    local_operand1->
							    buffer.length));
		if (!return_desc) {
			status = AE_NO_MEMORY;
			goto cleanup;
		}

		new_buf = (char *)return_desc->buffer.pointer;

		/* Concatenate the buffers */

		ACPI_MEMCPY(new_buf, operand0->buffer.pointer,
			    operand0->buffer.length);
		ACPI_MEMCPY(new_buf + operand0->buffer.length,
			    local_operand1->buffer.pointer,
			    local_operand1->buffer.length);
		break;

	default:

		/* Invalid object type, should not happen here */

		ACPI_ERROR((AE_INFO, "Invalid object type: 0x%X",
			    operand0->common.type));
		status = AE_AML_INTERNAL;
		goto cleanup;
	}

	*actual_return_desc = return_desc;

      cleanup:
	if (local_operand1 != operand1) {
		acpi_ut_remove_reference(local_operand1);
	}
	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_do_math_op
 *
 * PARAMETERS:  Opcode              - AML opcode
 *              Integer0            - Integer operand #0
 *              Integer1            - Integer operand #1
 *
 * RETURN:      Integer result of the operation
 *
 * DESCRIPTION: Execute a math AML opcode. The purpose of having all of the
 *              math functions here is to prevent a lot of pointer dereferencing
 *              to obtain the operands.
 *
 ******************************************************************************/

u64 acpi_ex_do_math_op(u16 opcode, u64 integer0, u64 integer1)
{

	ACPI_FUNCTION_ENTRY();

	switch (opcode) {
	case AML_ADD_OP:	/* Add (Integer0, Integer1, Result) */

		return (integer0 + integer1);

	case AML_BIT_AND_OP:	/* And (Integer0, Integer1, Result) */

		return (integer0 & integer1);

	case AML_BIT_NAND_OP:	/* NAnd (Integer0, Integer1, Result) */

		return (~(integer0 & integer1));

	case AML_BIT_OR_OP:	/* Or (Integer0, Integer1, Result) */

		return (integer0 | integer1);

	case AML_BIT_NOR_OP:	/* NOr (Integer0, Integer1, Result) */

		return (~(integer0 | integer1));

	case AML_BIT_XOR_OP:	/* XOr (Integer0, Integer1, Result) */

		return (integer0 ^ integer1);

	case AML_MULTIPLY_OP:	/* Multiply (Integer0, Integer1, Result) */

		return (integer0 * integer1);

	case AML_SHIFT_LEFT_OP:	/* shift_left (Operand, shift_count, Result) */

		/*
		 * We need to check if the shiftcount is larger than the integer bit
		 * width since the behavior of this is not well-defined in the C language.
		 */
		if (integer1 >= acpi_gbl_integer_bit_width) {
			return (0);
		}
		return (integer0 << integer1);

	case AML_SHIFT_RIGHT_OP:	/* shift_right (Operand, shift_count, Result) */

		/*
		 * We need to check if the shiftcount is larger than the integer bit
		 * width since the behavior of this is not well-defined in the C language.
		 */
		if (integer1 >= acpi_gbl_integer_bit_width) {
			return (0);
		}
		return (integer0 >> integer1);

	case AML_SUBTRACT_OP:	/* Subtract (Integer0, Integer1, Result) */

		return (integer0 - integer1);

	default:

		return (0);
	}
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_do_logical_numeric_op
 *
 * PARAMETERS:  Opcode              - AML opcode
 *              Integer0            - Integer operand #0
 *              Integer1            - Integer operand #1
 *              logical_result      - TRUE/FALSE result of the operation
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute a logical "Numeric" AML opcode. For these Numeric
 *              operators (LAnd and LOr), both operands must be integers.
 *
 *              Note: cleanest machine code seems to be produced by the code
 *              below, rather than using statements of the form:
 *                  Result = (Integer0 && Integer1);
 *
 ******************************************************************************/

acpi_status
acpi_ex_do_logical_numeric_op(u16 opcode,
			      u64 integer0, u64 integer1, u8 *logical_result)
{
	acpi_status status = AE_OK;
	u8 local_result = FALSE;

	ACPI_FUNCTION_TRACE(ex_do_logical_numeric_op);

	switch (opcode) {
	case AML_LAND_OP:	/* LAnd (Integer0, Integer1) */

		if (integer0 && integer1) {
			local_result = TRUE;
		}
		break;

	case AML_LOR_OP:	/* LOr (Integer0, Integer1) */

		if (integer0 || integer1) {
			local_result = TRUE;
		}
		break;

	default:
		status = AE_AML_INTERNAL;
		break;
	}

	/* Return the logical result and status */

	*logical_result = local_result;
	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_do_logical_op
 *
 * PARAMETERS:  Opcode              - AML opcode
 *              Operand0            - operand #0
 *              Operand1            - operand #1
 *              logical_result      - TRUE/FALSE result of the operation
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute a logical AML opcode. The purpose of having all of the
 *              functions here is to prevent a lot of pointer dereferencing
 *              to obtain the operands and to simplify the generation of the
 *              logical value. For the Numeric operators (LAnd and LOr), both
 *              operands must be integers. For the other logical operators,
 *              operands can be any combination of Integer/String/Buffer. The
 *              first operand determines the type to which the second operand
 *              will be converted.
 *
 *              Note: cleanest machine code seems to be produced by the code
 *              below, rather than using statements of the form:
 *                  Result = (Operand0 == Operand1);
 *
 ******************************************************************************/

acpi_status
acpi_ex_do_logical_op(u16 opcode,
		      union acpi_operand_object *operand0,
		      union acpi_operand_object *operand1, u8 * logical_result)
{
	union acpi_operand_object *local_operand1 = operand1;
	u64 integer0;
	u64 integer1;
	u32 length0;
	u32 length1;
	acpi_status status = AE_OK;
	u8 local_result = FALSE;
	int compare;

	ACPI_FUNCTION_TRACE(ex_do_logical_op);

	/*
	 * Convert the second operand if necessary.  The first operand
	 * determines the type of the second operand, (See the Data Types
	 * section of the ACPI 3.0+ specification.)  Both object types are
	 * guaranteed to be either Integer/String/Buffer by the operand
	 * resolution mechanism.
	 */
	switch (operand0->common.type) {
	case ACPI_TYPE_INTEGER:
		status =
		    acpi_ex_convert_to_integer(operand1, &local_operand1, 16);
		break;

	case ACPI_TYPE_STRING:
		status = acpi_ex_convert_to_string(operand1, &local_operand1,
						   ACPI_IMPLICIT_CONVERT_HEX);
		break;

	case ACPI_TYPE_BUFFER:
		status = acpi_ex_convert_to_buffer(operand1, &local_operand1);
		break;

	default:
		status = AE_AML_INTERNAL;
		break;
	}

	if (ACPI_FAILURE(status)) {
		goto cleanup;
	}

	/*
	 * Two cases: 1) Both Integers, 2) Both Strings or Buffers
	 */
	if (operand0->common.type == ACPI_TYPE_INTEGER) {
		/*
		 * 1) Both operands are of type integer
		 *    Note: local_operand1 may have changed above
		 */
		integer0 = operand0->integer.value;
		integer1 = local_operand1->integer.value;

		switch (opcode) {
		case AML_LEQUAL_OP:	/* LEqual (Operand0, Operand1) */

			if (integer0 == integer1) {
				local_result = TRUE;
			}
			break;

		case AML_LGREATER_OP:	/* LGreater (Operand0, Operand1) */

			if (integer0 > integer1) {
				local_result = TRUE;
			}
			break;

		case AML_LLESS_OP:	/* LLess (Operand0, Operand1) */

			if (integer0 < integer1) {
				local_result = TRUE;
			}
			break;

		default:
			status = AE_AML_INTERNAL;
			break;
		}
	} else {
		/*
		 * 2) Both operands are Strings or both are Buffers
		 *    Note: Code below takes advantage of common Buffer/String
		 *          object fields. local_operand1 may have changed above. Use
		 *          memcmp to handle nulls in buffers.
		 */
		length0 = operand0->buffer.length;
		length1 = local_operand1->buffer.length;

		/* Lexicographic compare: compare the data bytes */

		compare = ACPI_MEMCMP(operand0->buffer.pointer,
				      local_operand1->buffer.pointer,
				      (length0 > length1) ? length1 : length0);

		switch (opcode) {
		case AML_LEQUAL_OP:	/* LEqual (Operand0, Operand1) */

			/* Length and all bytes must be equal */

			if ((length0 == length1) && (compare == 0)) {

				/* Length and all bytes match ==> TRUE */

				local_result = TRUE;
			}
			break;

		case AML_LGREATER_OP:	/* LGreater (Operand0, Operand1) */

			if (compare > 0) {
				local_result = TRUE;
				goto cleanup;	/* TRUE */
			}
			if (compare < 0) {
				goto cleanup;	/* FALSE */
			}

			/* Bytes match (to shortest length), compare lengths */

			if (length0 > length1) {
				local_result = TRUE;
			}
			break;

		case AML_LLESS_OP:	/* LLess (Operand0, Operand1) */

			if (compare > 0) {
				goto cleanup;	/* FALSE */
			}
			if (compare < 0) {
				local_result = TRUE;
				goto cleanup;	/* TRUE */
			}

			/* Bytes match (to shortest length), compare lengths */

			if (length0 < length1) {
				local_result = TRUE;
			}
			break;

		default:
			status = AE_AML_INTERNAL;
			break;
		}
	}

      cleanup:

	/* New object was created if implicit conversion performed - delete */

	if (local_operand1 != operand1) {
		acpi_ut_remove_reference(local_operand1);
	}

	/* Return the logical result and status */

	*logical_result = local_result;
	return_ACPI_STATUS(status);
}
