/** @file tlv_common.c
 * 
 * OpenLLDP TLV Common Routines
 * 
 * See LICENSE file for more info.
 * 
 * Authors: Terry Simons (terry.simons@gmail.com)
 *
 */

#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "tlv.h"
#include "tlv_common.h"
#include "../lldp_debug.h"

#ifdef WIN32
#include <Winsock2.h>
#endif // WIN32

// A helper function to flatten an exploded TLV.
struct lldp_flat_tlv *flatten_tlv(struct lldp_tlv *tlv) {
	uint16_t type_and_length       = 0;
	struct lldp_flat_tlv *flat_tlv = NULL;

    if(!tlv) {
        return NULL;
    }

    // Munge our type and length into a single container.
    type_and_length = tlv->type;

    /*debug_printf(DEBUG_TLV, "[FLATTEN] Flatting TLV Type: %d\n", tlv->type);
      debug_hex_dump(DEBUG_TLV, tlv->info_string, tlv->length);
      debug_printf(DEBUG_TLV, "Flattening TLV: \n");
      debug_printf(DEBUG_TLV, "\tTLV Type: %d\n", tlv->type);
      debug_printf(DEBUG_TLV, "\tTLV Length: %d\n", tlv->length);*/

    type_and_length = type_and_length << 9;

    type_and_length |= tlv->length;

    //debug_printf(DEBUG_TLV, "Before Network Byte Order: ");
    //debug_hex_printf(DEBUG_TLV, (uint8_t *)&type_and_length, sizeof(type_and_length));

    // Convert it to network byte order...
    type_and_length = htons(type_and_length);

    //debug_printf(DEBUG_TLV, "After Network Byte Order: ");
    //debug_hex_printf(DEBUG_TLV, (uint8_t *)&type_and_length, sizeof(type_and_length));

    // Now cram all of the bits into our flat TLV container.
    flat_tlv = malloc(sizeof(struct lldp_flat_tlv));

    if(flat_tlv) {
        // We malloc for the size of the entire TLV, plus the 2 bytes for type and length.
        flat_tlv->size = tlv->length + 2;

        flat_tlv->tlv = malloc(flat_tlv->size);
        memset(&flat_tlv->tlv[0], 0x0, flat_tlv->size);

        //debug_printf(DEBUG_TLV, "Flattened TLV: ");
        //debug_hex_dump(DEBUG_TLV, flat_tlv->tlv, flat_tlv->size);

        // First, copy in the type and length
        memcpy(&flat_tlv->tlv[0], &type_and_length, sizeof(type_and_length));

        //debug_printf(DEBUG_TLV, "Flattened TLV: ");
        //debug_hex_dump(DEBUG_TLV, flat_tlv->tlv, flat_tlv->size);

        // Then copy in the info string, for the size of tlv->length
        memcpy(&flat_tlv->tlv[sizeof(type_and_length)], tlv->info_string, tlv->length);

#ifndef WIN32
#warning Do a full dump of the TLV here, so we can visually inspect it...
#endif

        // We're done. ;)

    } else {
        debug_printf(DEBUG_NORMAL, "[ERROR] Unable to malloc buffer in %s() at line: %d!\n", __FUNCTION__, __LINE__);
    }

    //debug_printf(DEBUG_TLV, "Flattened TLV: ");
    //debug_hex_dump(DEBUG_TLV, flat_tlv->tlv, flat_tlv->size);

    return flat_tlv;
}

void destroy_tlv(struct lldp_tlv **tlv) {
	if((tlv != NULL && (*tlv) != NULL))
	{
		if((*tlv)->info_string != NULL)
		{
			free((*tlv)->info_string);
			((*tlv)->info_string) = NULL;
		}

		free(*tlv);
		(*tlv) = NULL;
	}
}

void destroy_flattened_tlv(struct lldp_flat_tlv **tlv) {
	if((tlv != NULL && (*tlv) != NULL))
	{
		if((*tlv)->tlv != NULL)
		{
			free((*tlv)->tlv);
			(*tlv)->tlv = NULL;

			free(*tlv);
			(*tlv) = NULL;
		}
	}
}

/** */
void destroy_tlv_list(struct lldp_tlv_list **tlv_list) {
    struct lldp_tlv_list *current  = *tlv_list;

    debug_printf(DEBUG_TLV, "Destroy TLV List\n");

    if(current == NULL) {
        debug_printf(DEBUG_NORMAL, "[WARNING] Asked to delete empty list!\n");
    }

    debug_printf(DEBUG_TLV, "Entering Destroy loop\n");

    while(current != NULL) {   

      debug_printf(DEBUG_TLV, "current = %X\n", current);
   
      debug_printf(DEBUG_TLV, "current->next = %X\n", current->next);

        current = current->next;

	debug_printf(DEBUG_TLV, "Freeing TLV Info String.\n");

        free((*tlv_list)->tlv->info_string);

	debug_printf(DEBUG_TLV, "Freeing TLV.\n");
        free((*tlv_list)->tlv);

	debug_printf(DEBUG_TLV, "Freeing TLV List Node.\n");
        free(*tlv_list);

        (*tlv_list) = current;
    }
}

void add_tlv(struct lldp_tlv *tlv, struct lldp_tlv_list **tlv_list) {
  struct lldp_tlv_list *tail = NULL;
  struct lldp_tlv_list *tmp = *tlv_list;

  if(tlv != NULL) {
    tail = malloc(sizeof(struct lldp_tlv_list));

    tail->tlv  = tlv;
    tail->next = NULL;

    if((*tlv_list) == NULL) {
        (*tlv_list) = tail;
    } else {
        while(tmp->next != NULL) {
            tmp = tmp->next;      
        }    

	debug_printf(DEBUG_TLV, "Setting temp->next to %X\n", tail);

        tmp->next = tail;
    }
  }

}
