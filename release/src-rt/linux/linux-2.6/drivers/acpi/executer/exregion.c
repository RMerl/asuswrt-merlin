
/******************************************************************************
 *
 * Module Name: exregion - ACPI default op_region (address space) handlers
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2007, R. Byron Moore
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
#include <acpi/acinterp.h>

#define _COMPONENT          ACPI_EXECUTER
ACPI_MODULE_NAME("exregion")

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_system_memory_space_handler
 *
 * PARAMETERS:  Function            - Read or Write operation
 *              Address             - Where in the space to read or write
 *              bit_width           - Field width in bits (8, 16, or 32)
 *              Value               - Pointer to in or out value
 *              handler_context     - Pointer to Handler's context
 *              region_context      - Pointer to context specific to the
 *                                    accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the System Memory address space (Op Region)
 *
 ******************************************************************************/
acpi_status
acpi_ex_system_memory_space_handler(u32 function,
				    acpi_physical_address address,
				    u32 bit_width,
				    acpi_integer * value,
				    void *handler_context, void *region_context)
{
	acpi_status status = AE_OK;
	void *logical_addr_ptr = NULL;
	struct acpi_mem_space_context *mem_info = region_context;
	u32 length;
	acpi_size window_size;
#ifdef ACPI_MISALIGNMENT_NOT_SUPPORTED
	u32 remainder;
#endif

	ACPI_FUNCTION_TRACE(ex_system_memory_space_handler);

	/* Validate and translate the bit width */

	switch (bit_width) {
	case 8:
		length = 1;
		break;

	case 16:
		length = 2;
		break;

	case 32:
		length = 4;
		break;

	case 64:
		length = 8;
		break;

	default:
		ACPI_ERROR((AE_INFO, "Invalid SystemMemory width %d",
			    bit_width));
		return_ACPI_STATUS(AE_AML_OPERAND_VALUE);
	}

#ifdef ACPI_MISALIGNMENT_NOT_SUPPORTED
	/*
	 * Hardware does not support non-aligned data transfers, we must verify
	 * the request.
	 */
	(void)acpi_ut_short_divide((acpi_integer) address, length, NULL,
				   &remainder);
	if (remainder != 0) {
		return_ACPI_STATUS(AE_AML_ALIGNMENT);
	}
#endif

	/*
	 * Does the request fit into the cached memory mapping?
	 * Is 1) Address below the current mapping? OR
	 *    2) Address beyond the current mapping?
	 */
	if ((address < mem_info->mapped_physical_address) ||
	    (((acpi_integer) address + length) > ((acpi_integer)
						  mem_info->
						  mapped_physical_address +
						  mem_info->mapped_length))) {
		/*
		 * The request cannot be resolved by the current memory mapping;
		 * Delete the existing mapping and create a new one.
		 */
		if (mem_info->mapped_length) {

			/* Valid mapping, delete it */

			acpi_os_unmap_memory(mem_info->mapped_logical_address,
					     mem_info->mapped_length);
		}

		/*
		 * Don't attempt to map memory beyond the end of the region, and
		 * constrain the maximum mapping size to something reasonable.
		 */
		window_size = (acpi_size)
		    ((mem_info->address + mem_info->length) - address);

		if (window_size > ACPI_SYSMEM_REGION_WINDOW_SIZE) {
			window_size = ACPI_SYSMEM_REGION_WINDOW_SIZE;
		}

		/* Create a new mapping starting at the address given */

		mem_info->mapped_logical_address =
		    acpi_os_map_memory((acpi_native_uint) address, window_size);
		if (!mem_info->mapped_logical_address) {
			ACPI_ERROR((AE_INFO,
				    "Could not map memory at %8.8X%8.8X, size %X",
				    ACPI_FORMAT_UINT64(address),
				    (u32) window_size));
			mem_info->mapped_length = 0;
			return_ACPI_STATUS(AE_NO_MEMORY);
		}

		/* Save the physical address and mapping size */

		mem_info->mapped_physical_address = address;
		mem_info->mapped_length = window_size;
	}

	/*
	 * Generate a logical pointer corresponding to the address we want to
	 * access
	 */
	logical_addr_ptr = mem_info->mapped_logical_address +
	    ((acpi_integer) address -
	     (acpi_integer) mem_info->mapped_physical_address);

	ACPI_DEBUG_PRINT((ACPI_DB_INFO,
			  "System-Memory (width %d) R/W %d Address=%8.8X%8.8X\n",
			  bit_width, function, ACPI_FORMAT_UINT64(address)));

	/*
	 * Perform the memory read or write
	 *
	 * Note: For machines that do not support non-aligned transfers, the target
	 * address was checked for alignment above.  We do not attempt to break the
	 * transfer up into smaller (byte-size) chunks because the AML specifically
	 * asked for a transfer width that the hardware may require.
	 */
	switch (function) {
	case ACPI_READ:

		*value = 0;
		switch (bit_width) {
		case 8:
			*value = (acpi_integer) ACPI_GET8(logical_addr_ptr);
			break;

		case 16:
			*value = (acpi_integer) ACPI_GET16(logical_addr_ptr);
			break;

		case 32:
			*value = (acpi_integer) ACPI_GET32(logical_addr_ptr);
			break;

		case 64:
			*value = (acpi_integer) ACPI_GET64(logical_addr_ptr);
			break;

		default:
			/* bit_width was already validated */
			break;
		}
		break;

	case ACPI_WRITE:

		switch (bit_width) {
		case 8:
			ACPI_SET8(logical_addr_ptr) = (u8) * value;
			break;

		case 16:
			ACPI_SET16(logical_addr_ptr) = (u16) * value;
			break;

		case 32:
			ACPI_SET32(logical_addr_ptr) = (u32) * value;
			break;

		case 64:
			ACPI_SET64(logical_addr_ptr) = (u64) * value;
			break;

		default:
			/* bit_width was already validated */
			break;
		}
		break;

	default:
		status = AE_BAD_PARAMETER;
		break;
	}

	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_system_io_space_handler
 *
 * PARAMETERS:  Function            - Read or Write operation
 *              Address             - Where in the space to read or write
 *              bit_width           - Field width in bits (8, 16, or 32)
 *              Value               - Pointer to in or out value
 *              handler_context     - Pointer to Handler's context
 *              region_context      - Pointer to context specific to the
 *                                    accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the System IO address space (Op Region)
 *
 ******************************************************************************/

acpi_status
acpi_ex_system_io_space_handler(u32 function,
				acpi_physical_address address,
				u32 bit_width,
				acpi_integer * value,
				void *handler_context, void *region_context)
{
	acpi_status status = AE_OK;
	u32 value32;

	ACPI_FUNCTION_TRACE(ex_system_io_space_handler);

	ACPI_DEBUG_PRINT((ACPI_DB_INFO,
			  "System-IO (width %d) R/W %d Address=%8.8X%8.8X\n",
			  bit_width, function, ACPI_FORMAT_UINT64(address)));

	/* Decode the function parameter */

	switch (function) {
	case ACPI_READ:

		status = acpi_os_read_port((acpi_io_address) address,
					   &value32, bit_width);
		*value = value32;
		break;

	case ACPI_WRITE:

		status = acpi_os_write_port((acpi_io_address) address,
					    (u32) * value, bit_width);
		break;

	default:
		status = AE_BAD_PARAMETER;
		break;
	}

	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_pci_config_space_handler
 *
 * PARAMETERS:  Function            - Read or Write operation
 *              Address             - Where in the space to read or write
 *              bit_width           - Field width in bits (8, 16, or 32)
 *              Value               - Pointer to in or out value
 *              handler_context     - Pointer to Handler's context
 *              region_context      - Pointer to context specific to the
 *                                    accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the PCI Config address space (Op Region)
 *
 ******************************************************************************/

acpi_status
acpi_ex_pci_config_space_handler(u32 function,
				 acpi_physical_address address,
				 u32 bit_width,
				 acpi_integer * value,
				 void *handler_context, void *region_context)
{
	acpi_status status = AE_OK;
	struct acpi_pci_id *pci_id;
	u16 pci_register;

	ACPI_FUNCTION_TRACE(ex_pci_config_space_handler);

	/*
	 *  The arguments to acpi_os(Read|Write)pci_configuration are:
	 *
	 *  pci_segment is the PCI bus segment range 0-31
	 *  pci_bus     is the PCI bus number range 0-255
	 *  pci_device  is the PCI device number range 0-31
	 *  pci_function is the PCI device function number
	 *  pci_register is the Config space register range 0-255 bytes
	 *
	 *  Value - input value for write, output address for read
	 *
	 */
	pci_id = (struct acpi_pci_id *)region_context;
	pci_register = (u16) (u32) address;

	ACPI_DEBUG_PRINT((ACPI_DB_INFO,
			  "Pci-Config %d (%d) Seg(%04x) Bus(%04x) Dev(%04x) Func(%04x) Reg(%04x)\n",
			  function, bit_width, pci_id->segment, pci_id->bus,
			  pci_id->device, pci_id->function, pci_register));

	switch (function) {
	case ACPI_READ:

		*value = 0;
		status = acpi_os_read_pci_configuration(pci_id, pci_register,
							value, bit_width);
		break;

	case ACPI_WRITE:

		status = acpi_os_write_pci_configuration(pci_id, pci_register,
							 *value, bit_width);
		break;

	default:

		status = AE_BAD_PARAMETER;
		break;
	}

	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_cmos_space_handler
 *
 * PARAMETERS:  Function            - Read or Write operation
 *              Address             - Where in the space to read or write
 *              bit_width           - Field width in bits (8, 16, or 32)
 *              Value               - Pointer to in or out value
 *              handler_context     - Pointer to Handler's context
 *              region_context      - Pointer to context specific to the
 *                                    accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the CMOS address space (Op Region)
 *
 ******************************************************************************/

acpi_status
acpi_ex_cmos_space_handler(u32 function,
			   acpi_physical_address address,
			   u32 bit_width,
			   acpi_integer * value,
			   void *handler_context, void *region_context)
{
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ex_cmos_space_handler);

	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_pci_bar_space_handler
 *
 * PARAMETERS:  Function            - Read or Write operation
 *              Address             - Where in the space to read or write
 *              bit_width           - Field width in bits (8, 16, or 32)
 *              Value               - Pointer to in or out value
 *              handler_context     - Pointer to Handler's context
 *              region_context      - Pointer to context specific to the
 *                                    accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the PCI bar_target address space (Op Region)
 *
 ******************************************************************************/

acpi_status
acpi_ex_pci_bar_space_handler(u32 function,
			      acpi_physical_address address,
			      u32 bit_width,
			      acpi_integer * value,
			      void *handler_context, void *region_context)
{
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ex_pci_bar_space_handler);

	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_data_table_space_handler
 *
 * PARAMETERS:  Function            - Read or Write operation
 *              Address             - Where in the space to read or write
 *              bit_width           - Field width in bits (8, 16, or 32)
 *              Value               - Pointer to in or out value
 *              handler_context     - Pointer to Handler's context
 *              region_context      - Pointer to context specific to the
 *                                    accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the Data Table address space (Op Region)
 *
 ******************************************************************************/

acpi_status
acpi_ex_data_table_space_handler(u32 function,
				 acpi_physical_address address,
				 u32 bit_width,
				 acpi_integer * value,
				 void *handler_context, void *region_context)
{
	ACPI_FUNCTION_TRACE(ex_data_table_space_handler);

	/* Perform the memory read or write */

	switch (function) {
	case ACPI_READ:

		ACPI_MEMCPY(ACPI_CAST_PTR(char, value),
			    ACPI_PHYSADDR_TO_PTR(address),
			    ACPI_DIV_8(bit_width));
		break;

	case ACPI_WRITE:
	default:

		return_ACPI_STATUS(AE_SUPPORT);
	}

	return_ACPI_STATUS(AE_OK);
}
