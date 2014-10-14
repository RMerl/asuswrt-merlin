/******************************************************************************
 *
 * Module Name: dsfield - Dispatcher field routines
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
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
#include "amlcode.h"
#include "acdispat.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "acparser.h"

#define _COMPONENT          ACPI_DISPATCHER
ACPI_MODULE_NAME("dsfield")

/* Local prototypes */
static acpi_status
acpi_ds_get_field_names(struct acpi_create_field_info *info,
			struct acpi_walk_state *walk_state,
			union acpi_parse_object *arg);

/*******************************************************************************
 *
 * FUNCTION:    acpi_ds_create_buffer_field
 *
 * PARAMETERS:  Op                  - Current parse op (create_xXField)
 *              walk_state          - Current state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute the create_field operators:
 *              create_bit_field_op,
 *              create_byte_field_op,
 *              create_word_field_op,
 *              create_dword_field_op,
 *              create_qword_field_op,
 *              create_field_op     (all of which define a field in a buffer)
 *
 ******************************************************************************/

acpi_status
acpi_ds_create_buffer_field(union acpi_parse_object *op,
			    struct acpi_walk_state *walk_state)
{
	union acpi_parse_object *arg;
	struct acpi_namespace_node *node;
	acpi_status status;
	union acpi_operand_object *obj_desc;
	union acpi_operand_object *second_desc = NULL;
	u32 flags;

	ACPI_FUNCTION_TRACE(ds_create_buffer_field);

	/*
	 * Get the name_string argument (name of the new buffer_field)
	 */
	if (op->common.aml_opcode == AML_CREATE_FIELD_OP) {

		/* For create_field, name is the 4th argument */

		arg = acpi_ps_get_arg(op, 3);
	} else {
		/* For all other create_xXXField operators, name is the 3rd argument */

		arg = acpi_ps_get_arg(op, 2);
	}

	if (!arg) {
		return_ACPI_STATUS(AE_AML_NO_OPERAND);
	}

	if (walk_state->deferred_node) {
		node = walk_state->deferred_node;
		status = AE_OK;
	} else {
		/* Execute flag should always be set when this function is entered */

		if (!(walk_state->parse_flags & ACPI_PARSE_EXECUTE)) {
			return_ACPI_STATUS(AE_AML_INTERNAL);
		}

		/* Creating new namespace node, should not already exist */

		flags = ACPI_NS_NO_UPSEARCH | ACPI_NS_DONT_OPEN_SCOPE |
		    ACPI_NS_ERROR_IF_FOUND;

		/*
		 * Mark node temporary if we are executing a normal control
		 * method. (Don't mark if this is a module-level code method)
		 */
		if (walk_state->method_node &&
		    !(walk_state->parse_flags & ACPI_PARSE_MODULE_LEVEL)) {
			flags |= ACPI_NS_TEMPORARY;
		}

		/* Enter the name_string into the namespace */

		status =
		    acpi_ns_lookup(walk_state->scope_info,
				   arg->common.value.string, ACPI_TYPE_ANY,
				   ACPI_IMODE_LOAD_PASS1, flags, walk_state,
				   &node);
		if (ACPI_FAILURE(status)) {
			ACPI_ERROR_NAMESPACE(arg->common.value.string, status);
			return_ACPI_STATUS(status);
		}
	}

	/*
	 * We could put the returned object (Node) on the object stack for later,
	 * but for now, we will put it in the "op" object that the parser uses,
	 * so we can get it again at the end of this scope.
	 */
	op->common.node = node;

	/*
	 * If there is no object attached to the node, this node was just created
	 * and we need to create the field object. Otherwise, this was a lookup
	 * of an existing node and we don't want to create the field object again.
	 */
	obj_desc = acpi_ns_get_attached_object(node);
	if (obj_desc) {
		return_ACPI_STATUS(AE_OK);
	}

	/*
	 * The Field definition is not fully parsed at this time.
	 * (We must save the address of the AML for the buffer and index operands)
	 */

	/* Create the buffer field object */

	obj_desc = acpi_ut_create_internal_object(ACPI_TYPE_BUFFER_FIELD);
	if (!obj_desc) {
		status = AE_NO_MEMORY;
		goto cleanup;
	}

	/*
	 * Remember location in AML stream of the field unit opcode and operands --
	 * since the buffer and index operands must be evaluated.
	 */
	second_desc = obj_desc->common.next_object;
	second_desc->extra.aml_start = op->named.data;
	second_desc->extra.aml_length = op->named.length;
	obj_desc->buffer_field.node = node;

	/* Attach constructed field descriptors to parent node */

	status = acpi_ns_attach_object(node, obj_desc, ACPI_TYPE_BUFFER_FIELD);
	if (ACPI_FAILURE(status)) {
		goto cleanup;
	}

      cleanup:

	/* Remove local reference to the object */

	acpi_ut_remove_reference(obj_desc);
	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ds_get_field_names
 *
 * PARAMETERS:  Info            - create_field info structure
 *  `           walk_state      - Current method state
 *              Arg             - First parser arg for the field name list
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Process all named fields in a field declaration.  Names are
 *              entered into the namespace.
 *
 ******************************************************************************/

static acpi_status
acpi_ds_get_field_names(struct acpi_create_field_info *info,
			struct acpi_walk_state *walk_state,
			union acpi_parse_object *arg)
{
	acpi_status status;
	u64 position;

	ACPI_FUNCTION_TRACE_PTR(ds_get_field_names, info);

	/* First field starts at bit zero */

	info->field_bit_position = 0;

	/* Process all elements in the field list (of parse nodes) */

	while (arg) {
		/*
		 * Three types of field elements are handled:
		 * 1) Offset - specifies a bit offset
		 * 2) access_as - changes the access mode
		 * 3) Name - Enters a new named field into the namespace
		 */
		switch (arg->common.aml_opcode) {
		case AML_INT_RESERVEDFIELD_OP:

			position = (u64) info->field_bit_position
			    + (u64) arg->common.value.size;

			if (position > ACPI_UINT32_MAX) {
				ACPI_ERROR((AE_INFO,
					    "Bit offset within field too large (> 0xFFFFFFFF)"));
				return_ACPI_STATUS(AE_SUPPORT);
			}

			info->field_bit_position = (u32) position;
			break;

		case AML_INT_ACCESSFIELD_OP:

			/*
			 * Get a new access_type and access_attribute -- to be used for all
			 * field units that follow, until field end or another access_as
			 * keyword.
			 *
			 * In field_flags, preserve the flag bits other than the
			 * ACCESS_TYPE bits
			 */
			info->field_flags = (u8)
			    ((info->
			      field_flags & ~(AML_FIELD_ACCESS_TYPE_MASK)) |
			     ((u8) ((u32) arg->common.value.integer >> 8)));

			info->attribute = (u8) (arg->common.value.integer);
			break;

		case AML_INT_NAMEDFIELD_OP:

			/* Lookup the name, it should already exist */

			status = acpi_ns_lookup(walk_state->scope_info,
						(char *)&arg->named.name,
						info->field_type,
						ACPI_IMODE_EXECUTE,
						ACPI_NS_DONT_OPEN_SCOPE,
						walk_state, &info->field_node);
			if (ACPI_FAILURE(status)) {
				ACPI_ERROR_NAMESPACE((char *)&arg->named.name,
						     status);
				return_ACPI_STATUS(status);
			} else {
				arg->common.node = info->field_node;
				info->field_bit_length = arg->common.value.size;

				/*
				 * If there is no object attached to the node, this node was
				 * just created and we need to create the field object.
				 * Otherwise, this was a lookup of an existing node and we
				 * don't want to create the field object again.
				 */
				if (!acpi_ns_get_attached_object
				    (info->field_node)) {
					status = acpi_ex_prep_field_value(info);
					if (ACPI_FAILURE(status)) {
						return_ACPI_STATUS(status);
					}
				}
			}

			/* Keep track of bit position for the next field */

			position = (u64) info->field_bit_position
			    + (u64) arg->common.value.size;

			if (position > ACPI_UINT32_MAX) {
				ACPI_ERROR((AE_INFO,
					    "Field [%4.4s] bit offset too large (> 0xFFFFFFFF)",
					    ACPI_CAST_PTR(char,
							  &info->field_node->
							  name)));
				return_ACPI_STATUS(AE_SUPPORT);
			}

			info->field_bit_position += info->field_bit_length;
			break;

		default:

			ACPI_ERROR((AE_INFO,
				    "Invalid opcode in field list: 0x%X",
				    arg->common.aml_opcode));
			return_ACPI_STATUS(AE_AML_BAD_OPCODE);
		}

		arg = arg->common.next;
	}

	return_ACPI_STATUS(AE_OK);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ds_create_field
 *
 * PARAMETERS:  Op              - Op containing the Field definition and args
 *              region_node     - Object for the containing Operation Region
 *  `           walk_state      - Current method state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a new field in the specified operation region
 *
 ******************************************************************************/

acpi_status
acpi_ds_create_field(union acpi_parse_object *op,
		     struct acpi_namespace_node *region_node,
		     struct acpi_walk_state *walk_state)
{
	acpi_status status;
	union acpi_parse_object *arg;
	struct acpi_create_field_info info;

	ACPI_FUNCTION_TRACE_PTR(ds_create_field, op);

	/* First arg is the name of the parent op_region (must already exist) */

	arg = op->common.value.arg;
	if (!region_node) {
		status =
		    acpi_ns_lookup(walk_state->scope_info,
				   arg->common.value.name, ACPI_TYPE_REGION,
				   ACPI_IMODE_EXECUTE, ACPI_NS_SEARCH_PARENT,
				   walk_state, &region_node);
		if (ACPI_FAILURE(status)) {
			ACPI_ERROR_NAMESPACE(arg->common.value.name, status);
			return_ACPI_STATUS(status);
		}
	}

	/* Second arg is the field flags */

	arg = arg->common.next;
	info.field_flags = (u8) arg->common.value.integer;
	info.attribute = 0;

	/* Each remaining arg is a Named Field */

	info.field_type = ACPI_TYPE_LOCAL_REGION_FIELD;
	info.region_node = region_node;

	status = acpi_ds_get_field_names(&info, walk_state, arg->common.next);

	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ds_init_field_objects
 *
 * PARAMETERS:  Op              - Op containing the Field definition and args
 *  `           walk_state      - Current method state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: For each "Field Unit" name in the argument list that is
 *              part of the field declaration, enter the name into the
 *              namespace.
 *
 ******************************************************************************/

acpi_status
acpi_ds_init_field_objects(union acpi_parse_object *op,
			   struct acpi_walk_state *walk_state)
{
	acpi_status status;
	union acpi_parse_object *arg = NULL;
	struct acpi_namespace_node *node;
	u8 type = 0;
	u32 flags;

	ACPI_FUNCTION_TRACE_PTR(ds_init_field_objects, op);

	/* Execute flag should always be set when this function is entered */

	if (!(walk_state->parse_flags & ACPI_PARSE_EXECUTE)) {
		if (walk_state->parse_flags & ACPI_PARSE_DEFERRED_OP) {

			/* bank_field Op is deferred, just return OK */

			return_ACPI_STATUS(AE_OK);
		}

		return_ACPI_STATUS(AE_AML_INTERNAL);
	}

	/*
	 * Get the field_list argument for this opcode. This is the start of the
	 * list of field elements.
	 */
	switch (walk_state->opcode) {
	case AML_FIELD_OP:
		arg = acpi_ps_get_arg(op, 2);
		type = ACPI_TYPE_LOCAL_REGION_FIELD;
		break;

	case AML_BANK_FIELD_OP:
		arg = acpi_ps_get_arg(op, 4);
		type = ACPI_TYPE_LOCAL_BANK_FIELD;
		break;

	case AML_INDEX_FIELD_OP:
		arg = acpi_ps_get_arg(op, 3);
		type = ACPI_TYPE_LOCAL_INDEX_FIELD;
		break;

	default:
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	/* Creating new namespace node(s), should not already exist */

	flags = ACPI_NS_NO_UPSEARCH | ACPI_NS_DONT_OPEN_SCOPE |
	    ACPI_NS_ERROR_IF_FOUND;

	/*
	 * Mark node(s) temporary if we are executing a normal control
	 * method. (Don't mark if this is a module-level code method)
	 */
	if (walk_state->method_node &&
	    !(walk_state->parse_flags & ACPI_PARSE_MODULE_LEVEL)) {
		flags |= ACPI_NS_TEMPORARY;
	}

	/*
	 * Walk the list of entries in the field_list
	 * Note: field_list can be of zero length. In this case, Arg will be NULL.
	 */
	while (arg) {
		/*
		 * Ignore OFFSET and ACCESSAS terms here; we are only interested in the
		 * field names in order to enter them into the namespace.
		 */
		if (arg->common.aml_opcode == AML_INT_NAMEDFIELD_OP) {
			status = acpi_ns_lookup(walk_state->scope_info,
						(char *)&arg->named.name, type,
						ACPI_IMODE_LOAD_PASS1, flags,
						walk_state, &node);
			if (ACPI_FAILURE(status)) {
				ACPI_ERROR_NAMESPACE((char *)&arg->named.name,
						     status);
				if (status != AE_ALREADY_EXISTS) {
					return_ACPI_STATUS(status);
				}

				/* Name already exists, just ignore this error */

				status = AE_OK;
			}

			arg->common.node = node;
		}

		/* Get the next field element in the list */

		arg = arg->common.next;
	}

	return_ACPI_STATUS(AE_OK);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ds_create_bank_field
 *
 * PARAMETERS:  Op              - Op containing the Field definition and args
 *              region_node     - Object for the containing Operation Region
 *              walk_state      - Current method state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a new bank field in the specified operation region
 *
 ******************************************************************************/

acpi_status
acpi_ds_create_bank_field(union acpi_parse_object *op,
			  struct acpi_namespace_node *region_node,
			  struct acpi_walk_state *walk_state)
{
	acpi_status status;
	union acpi_parse_object *arg;
	struct acpi_create_field_info info;

	ACPI_FUNCTION_TRACE_PTR(ds_create_bank_field, op);

	/* First arg is the name of the parent op_region (must already exist) */

	arg = op->common.value.arg;
	if (!region_node) {
		status =
		    acpi_ns_lookup(walk_state->scope_info,
				   arg->common.value.name, ACPI_TYPE_REGION,
				   ACPI_IMODE_EXECUTE, ACPI_NS_SEARCH_PARENT,
				   walk_state, &region_node);
		if (ACPI_FAILURE(status)) {
			ACPI_ERROR_NAMESPACE(arg->common.value.name, status);
			return_ACPI_STATUS(status);
		}
	}

	/* Second arg is the Bank Register (Field) (must already exist) */

	arg = arg->common.next;
	status =
	    acpi_ns_lookup(walk_state->scope_info, arg->common.value.string,
			   ACPI_TYPE_ANY, ACPI_IMODE_EXECUTE,
			   ACPI_NS_SEARCH_PARENT, walk_state,
			   &info.register_node);
	if (ACPI_FAILURE(status)) {
		ACPI_ERROR_NAMESPACE(arg->common.value.string, status);
		return_ACPI_STATUS(status);
	}

	/*
	 * Third arg is the bank_value
	 * This arg is a term_arg, not a constant
	 * It will be evaluated later, by acpi_ds_eval_bank_field_operands
	 */
	arg = arg->common.next;

	/* Fourth arg is the field flags */

	arg = arg->common.next;
	info.field_flags = (u8) arg->common.value.integer;

	/* Each remaining arg is a Named Field */

	info.field_type = ACPI_TYPE_LOCAL_BANK_FIELD;
	info.region_node = region_node;

	/*
	 * Use Info.data_register_node to store bank_field Op
	 * It's safe because data_register_node will never be used when create bank field
	 * We store aml_start and aml_length in the bank_field Op for late evaluation
	 * Used in acpi_ex_prep_field_value(Info)
	 *
	 * TBD: Or, should we add a field in struct acpi_create_field_info, like "void *ParentOp"?
	 */
	info.data_register_node = (struct acpi_namespace_node *)op;

	status = acpi_ds_get_field_names(&info, walk_state, arg->common.next);
	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ds_create_index_field
 *
 * PARAMETERS:  Op              - Op containing the Field definition and args
 *              region_node     - Object for the containing Operation Region
 *  `           walk_state      - Current method state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a new index field in the specified operation region
 *
 ******************************************************************************/

acpi_status
acpi_ds_create_index_field(union acpi_parse_object *op,
			   struct acpi_namespace_node *region_node,
			   struct acpi_walk_state *walk_state)
{
	acpi_status status;
	union acpi_parse_object *arg;
	struct acpi_create_field_info info;

	ACPI_FUNCTION_TRACE_PTR(ds_create_index_field, op);

	/* First arg is the name of the Index register (must already exist) */

	arg = op->common.value.arg;
	status =
	    acpi_ns_lookup(walk_state->scope_info, arg->common.value.string,
			   ACPI_TYPE_ANY, ACPI_IMODE_EXECUTE,
			   ACPI_NS_SEARCH_PARENT, walk_state,
			   &info.register_node);
	if (ACPI_FAILURE(status)) {
		ACPI_ERROR_NAMESPACE(arg->common.value.string, status);
		return_ACPI_STATUS(status);
	}

	/* Second arg is the data register (must already exist) */

	arg = arg->common.next;
	status =
	    acpi_ns_lookup(walk_state->scope_info, arg->common.value.string,
			   ACPI_TYPE_ANY, ACPI_IMODE_EXECUTE,
			   ACPI_NS_SEARCH_PARENT, walk_state,
			   &info.data_register_node);
	if (ACPI_FAILURE(status)) {
		ACPI_ERROR_NAMESPACE(arg->common.value.string, status);
		return_ACPI_STATUS(status);
	}

	/* Next arg is the field flags */

	arg = arg->common.next;
	info.field_flags = (u8) arg->common.value.integer;

	/* Each remaining arg is a Named Field */

	info.field_type = ACPI_TYPE_LOCAL_INDEX_FIELD;
	info.region_node = region_node;

	status = acpi_ds_get_field_names(&info, walk_state, arg->common.next);

	return_ACPI_STATUS(status);
}
