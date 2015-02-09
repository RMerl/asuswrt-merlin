/*
 *  drivers/s390/cio/qdio_debug.h
 *
 *  Copyright IBM Corp. 2008
 *
 *  Author: Jan Glauber (jang@linux.vnet.ibm.com)
 */
#ifndef QDIO_DEBUG_H
#define QDIO_DEBUG_H

#include <asm/debug.h>
#include <asm/qdio.h>
#include "qdio.h"

/* that gives us 15 characters in the text event views */
#define QDIO_DBF_LEN	16

extern debug_info_t *qdio_dbf_setup;
extern debug_info_t *qdio_dbf_error;

/* sort out low debug levels early to avoid wasted sprints */
static inline int qdio_dbf_passes(debug_info_t *dbf_grp, int level)
{
	return (level <= dbf_grp->level);
}

#define DBF_ERR		3	/* error conditions	*/
#define DBF_WARN	4	/* warning conditions	*/
#define DBF_INFO	6	/* informational	*/

#undef DBF_EVENT
#undef DBF_ERROR
#undef DBF_DEV_EVENT

#define DBF_EVENT(text...) \
	do { \
		char debug_buffer[QDIO_DBF_LEN]; \
		snprintf(debug_buffer, QDIO_DBF_LEN, text); \
		debug_text_event(qdio_dbf_setup, DBF_ERR, debug_buffer); \
	} while (0)

#define DBF_HEX(addr, len) \
	do { \
		debug_event(qdio_dbf_setup, DBF_ERR, (void*)(addr), len); \
	} while (0)

#define DBF_ERROR(text...) \
	do { \
		char debug_buffer[QDIO_DBF_LEN]; \
		snprintf(debug_buffer, QDIO_DBF_LEN, text); \
		debug_text_event(qdio_dbf_error, DBF_ERR, debug_buffer); \
	} while (0)

#define DBF_ERROR_HEX(addr, len) \
	do { \
		debug_event(qdio_dbf_error, DBF_ERR, (void*)(addr), len); \
	} while (0)


#define DBF_DEV_EVENT(level, device, text...) \
	do { \
		char debug_buffer[QDIO_DBF_LEN]; \
		if (qdio_dbf_passes(device->debug_area, level)) { \
			snprintf(debug_buffer, QDIO_DBF_LEN, text); \
			debug_text_event(device->debug_area, level, debug_buffer); \
		} \
	} while (0)

#define DBF_DEV_HEX(level, device, addr, len) \
	do { \
		debug_event(device->debug_area, level, (void*)(addr), len); \
	} while (0)

void qdio_allocate_dbf(struct qdio_initialize *init_data,
		       struct qdio_irq *irq_ptr);
void qdio_setup_debug_entries(struct qdio_irq *irq_ptr,
			      struct ccw_device *cdev);
void qdio_shutdown_debug_entries(struct qdio_irq *irq_ptr,
				 struct ccw_device *cdev);
int qdio_debug_init(void);
void qdio_debug_exit(void);

#endif
