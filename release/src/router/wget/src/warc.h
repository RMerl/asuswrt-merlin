/* Declarations of WARC helper methods. */
#ifndef WARC_H
#define WARC_H

#include "host.h"

void warc_init (void);
void warc_close (void);
void warc_uuid_str (char *id_str);

char * warc_timestamp (char *timestamp, size_t timestamp_size);

FILE * warc_tempfile (void);

bool warc_write_request_record (const char *url, const char *timestamp_str,
  const char *concurrent_to_uuid, const ip_address *ip, FILE *body, off_t payload_offset);
bool warc_write_response_record (const char *url, const char *timestamp_str,
  const char *concurrent_to_uuid, const ip_address *ip, FILE *body, off_t payload_offset,
  const char *mime_type, int response_code, const char *redirect_location);
bool warc_write_resource_record (const char *resource_uuid, const char *url,
  const char *timestamp_str, const char *concurrent_to_uuid, const ip_address *ip,
  const char *content_type, FILE *body, off_t payload_offset);
bool warc_write_metadata_record (const char *record_uuid, const char *url,
  const char *timestamp_str, const char *concurrent_to_uuid, ip_address *ip,
  const char *content_type, FILE *body, off_t payload_offset);

#endif /* WARC_H */
