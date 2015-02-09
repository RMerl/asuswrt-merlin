
/******************************************************************************
 *
 * Module Name: hwgpe - Low level GPE enable/disable/clear functions
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
#include "acevents.h"

#define _COMPONENT          ACPI_HARDWARE
ACPI_MODULE_NAME("hwgpe")

/* Local prototypes */
static acpi_status
acpi_hw_enable_wakeup_gpe_block(struct acpi_gpe_xrupt_info *gpe_xrupt_info,
				struct acpi_gpe_block_info *gpe_block,
				void *context);

/******************************************************************************
 *
 * FUNCTION:	acpi_hw_get_gpe_register_bit
 *
 * PARAMETERS:	gpe_event_info	    - Info block for the GPE
 *		gpe_register_info   - Info block for the GPE register
 *
 * RETURN:	Status
 *
 * DESCRIPTION:	Compute GPE enable mask with one bit corresponding to the given
 *		GPE set.
 *
 ******************************************************************************/

u32 acpi_hw_get_gpe_register_bit(struct acpi_gpe_event_info *gpe_event_info,
			     struct acpi_gpe_register_info *gpe_register_info)
{
	return (u32)1 << (gpe_event_info->gpe_number -
				gpe_register_info->base_gpe_number);
}

/******************************************************************************
 *
 * FUNCTION:	acpi_hw_low_set_gpe
 *
 * PARAMETERS:	gpe_event_info	    - Info block for the GPE to be disabled
 *		action		    - Enable or disable
 *
 * RETURN:	Status
 *
 * DESCRIPTION: Enable or disable a single GPE in its enable register.
 *
 ******************************************************************************/

acpi_status
acpi_hw_low_set_gpe(struct acpi_gpe_event_info *gpe_event_info, u8 action)
{
	struct acpi_gpe_register_info *gpe_register_info;
	acpi_status status;
	u32 enable_mask;
	u32 register_bit;

	ACPI_FUNCTION_ENTRY();

	/* Get the info block for the entire GPE register */

	gpe_register_info = gpe_event_info->register_info;
	if (!gpe_register_info) {
		return (AE_NOT_EXIST);
	}

	/* Get current value of the enable register that contains this GPE */

	status = acpi_hw_read(&enable_mask, &gpe_register_info->enable_address);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	/* Set ot clear just the bit that corresponds to this GPE */

	register_bit = acpi_hw_get_gpe_register_bit(gpe_event_info,
						gpe_register_info);
	switch (action) {
	case ACPI_GPE_COND_ENABLE:
		if (!(register_bit & gpe_register_info->enable_for_run))
			return (AE_BAD_PARAMETER);

	case ACPI_GPE_ENABLE:
		ACPI_SET_BIT(enable_mask, register_bit);
		break;

	case ACPI_GPE_DISABLE:
		ACPI_CLEAR_BIT(enable_mask, register_bit);
		break;

	default:
		ACPI_ERROR((AE_INFO, "Invalid action\n"));
		return (AE_BAD_PARAMETER);
	}

	/* Write the updated enable mask */

	status = acpi_hw_write(enable_mask, &gpe_register_info->enable_address);
	return (status);
}

/******************************************************************************
 *
 * FUNCTION:    acpi_hw_clear_gpe
 *
 * PARAMETERS:  gpe_event_info      - Info block for the GPE to be cleared
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Clear the status bit for a single GPE.
 *
 ******************************************************************************/

acpi_status acpi_hw_clear_gpe(struct acpi_gpe_event_info * gpe_event_info)
{
	struct acpi_gpe_register_info *gpe_register_info;
	acpi_status status;
	u32 register_bit;

	ACPI_FUNCTION_ENTRY();

	/* Get the info block for the entire GPE register */

	gpe_register_info = gpe_event_info->register_info;
	if (!gpe_register_info) {
		return (AE_NOT_EXIST);
	}

	register_bit = acpi_hw_get_gpe_register_bit(gpe_event_info,
						gpe_register_info);

	/*
	 * Write a one to the appropriate bit in the status register to
	 * clear this GPE.
	 */
	status = acpi_hw_write(register_bit,
			       &gpe_register_info->status_address);

	return (status);
}

/******************************************************************************
 *
 * FUNCTION:    acpi_hw_get_gpe_status
 *
 * PARAMETERS:  gpe_event_info      - Info block for the GPE to queried
 *              event_status        - Where the GPE status is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Return the status of a single GPE.
 *
 ******************************************************************************/

acpi_status
acpi_hw_get_gpe_status(struct acpi_gpe_event_info * gpe_event_info,
		       acpi_event_status * event_status)
{
	u32 in_byte;
	u32 register_bit;
	struct acpi_gpe_register_info *gpe_register_info;
	acpi_status status;
	acpi_event_status local_event_status = 0;

	ACPI_FUNCTION_ENTRY();

	if (!event_status) {
		return (AE_BAD_PARAMETER);
	}

	/* Get the info block for the entire GPE register */

	gpe_register_info = gpe_event_info->register_info;

	/* Get the register bitmask for this GPE */

	register_bit = acpi_hw_get_gpe_register_bit(gpe_event_info,
						gpe_register_info);

	/* GPE currently enabled? (enabled for runtime?) */

	if (register_bit & gpe_register_info->enable_for_run) {
		local_event_status |= ACPI_EVENT_FLAG_ENABLED;
	}

	/* GPE enabled for wake? */

	if (register_bit & gpe_register_info->enable_for_wake) {
		local_event_status |= ACPI_EVENT_FLAG_WAKE_ENABLED;
	}

	/* GPE currently active (status bit == 1)? */

	status = acpi_hw_read(&in_byte, &gpe_register_info->status_address);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	if (register_bit & in_byte) {
		local_event_status |= ACPI_EVENT_FLAG_SET;
	}

	/* Set return value */

	(*event_status) = local_event_status;
	return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    acpi_hw_disable_gpe_block
 *
 * PARAMETERS:  gpe_xrupt_info      - GPE Interrupt info
 *              gpe_block           - Gpe Block info
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Disable all GPEs within a single GPE block
 *
 ******************************************************************************/

acpi_status
acpi_hw_disable_gpe_block(struct acpi_gpe_xrupt_info *gpe_xrupt_info,
			  struct acpi_gpe_block_info *gpe_block, void *context)
{
	u32 i;
	acpi_status status;

	/* Examine each GPE Register within the block */

	for (i = 0; i < gpe_block->register_count; i++) {

		/* Disable all GPEs in this register */

		status =
		    acpi_hw_write(0x00,
				  &gpe_block->register_info[i].enable_address);
		if (ACPI_FAILURE(status)) {
			return (status);
		}
	}

	return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    acpi_hw_clear_gpe_block
 *
 * PARAMETERS:  gpe_xrupt_info      - GPE Interrupt info
 *              gpe_block           - Gpe Block info
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Clear status bits for all GPEs within a single GPE block
 *
 ******************************************************************************/

acpi_status
acpi_hw_clear_gpe_block(struct acpi_gpe_xrupt_info *gpe_xrupt_info,
			struct acpi_gpe_block_info *gpe_block, void *context)
{
	u32 i;
	acpi_status status;

	/* Examine each GPE Register within the block */

	for (i = 0; i < gpe_block->register_count; i++) {

		/* Clear status on all GPEs in this register */

		status =
		    acpi_hw_write(0xFF,
				  &gpe_block->register_info[i].status_address);
		if (ACPI_FAILURE(status)) {
			return (status);
		}
	}

	return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    acpi_hw_enable_runtime_gpe_block
 *
 * PARAMETERS:  gpe_xrupt_info      - GPE Interrupt info
 *              gpe_block           - Gpe Block info
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enable all "runtime" GPEs within a single GPE block. Includes
 *              combination wake/run GPEs.
 *
 ******************************************************************************/

acpi_status
acpi_hw_enable_runtime_gpe_block(struct acpi_gpe_xrupt_info *gpe_xrupt_info,
				 struct acpi_gpe_block_info *gpe_block, void *context)
{
	u32 i;
	acpi_status status;

	/* NOTE: assumes that all GPEs are currently disabled */

	/* Examine each GPE Register within the block */

	for (i = 0; i < gpe_block->register_count; i++) {
		if (!gpe_block->register_info[i].enable_for_run) {
			continue;
		}

		/* Enable all "runtime" GPEs in this register */

		status =
		    acpi_hw_write(gpe_block->register_info[i].enable_for_run,
				  &gpe_block->register_info[i].enable_address);
		if (ACPI_FAILURE(status)) {
			return (status);
		}
	}

	return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    acpi_hw_enable_wakeup_gpe_block
 *
 * PARAMETERS:  gpe_xrupt_info      - GPE Interrupt info
 *              gpe_block           - Gpe Block info
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enable all "wake" GPEs within a single GPE block. Includes
 *              combination wake/run GPEs.
 *
 ******************************************************************************/

static acpi_status
acpi_hw_enable_wakeup_gpe_block(struct acpi_gpe_xrupt_info *gpe_xrupt_info,
				struct acpi_gpe_block_info *gpe_block,
				void *context)
{
	u32 i;
	acpi_status status;

	/* Examine each GPE Register within the block */

	for (i = 0; i < gpe_block->register_count; i++) {
		if (!gpe_block->register_info[i].enable_for_wake) {
			continue;
		}

		/* Enable all "wake" GPEs in this register */

		status =
		    acpi_hw_write(gpe_block->register_info[i].enable_for_wake,
				  &gpe_block->register_info[i].enable_address);
		if (ACPI_FAILURE(status)) {
			return (status);
		}
	}

	return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    acpi_hw_disable_all_gpes
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Disable and clear all GPEs in all GPE blocks
 *
 ******************************************************************************/

acpi_status acpi_hw_disable_all_gpes(void)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(hw_disable_all_gpes);

	status = acpi_ev_walk_gpe_list(acpi_hw_disable_gpe_block, NULL);
	status = acpi_ev_walk_gpe_list(acpi_hw_clear_gpe_block, NULL);
	return_ACPI_STATUS(status);
}

/******************************************************************************
 *
 * FUNCTION:    acpi_hw_enable_all_runtime_gpes
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enable all "runtime" GPEs, in all GPE blocks
 *
 ******************************************************************************/

acpi_status acpi_hw_enable_all_runtime_gpes(void)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(hw_enable_all_runtime_gpes);

	status = acpi_ev_walk_gpe_list(acpi_hw_enable_runtime_gpe_block, NULL);
	return_ACPI_STATUS(status);
}

/******************************************************************************
 *
 * FUNCTION:    acpi_hw_enable_all_wakeup_gpes
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enable all "wakeup" GPEs, in all GPE blocks
 *
 ******************************************************************************/

acpi_status acpi_hw_enable_all_wakeup_gpes(void)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(hw_enable_all_wakeup_gpes);

	status = acpi_ev_walk_gpe_list(acpi_hw_enable_wakeup_gpe_block, NULL);
	return_ACPI_STATUS(status);
}
