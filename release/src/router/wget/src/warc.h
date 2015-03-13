/* Declarations of WARC helper methods. */
#ifndef WARC_H
#define WARC_H

#include "host.h"

void warc_init (void);
void warc_close (void);
void warc_timestamp (char *timestamp);
void warc_uuid_str (char *id_str);

FILE * warc_tempfile (void);

bool warc_write_request_record (char *url, char *timestamp_str,
  char *concurrent_to_uuid, ip_address *ip, FILE *body, off_t payload_offset);
bool warc_write_response_record (char *url, char *timestamp_str,
  char *concurrent_to_uuid, ip_address *ip, FILE *body, off_t payload_offset,
  char *mime_type, int response_code, char *redirect_location);
bool warc_write_resource_record (char *resource_uuid, const char *url,
  const char *timestamp_str, const char *concurrent_to_uuid, ip_address *ip,
  const char *content_type, FILE *body, off_t payload_offset);
bool warc_write_metadata_record (char *record_uuid, const char *url,
  const char *timestamp_str, const char *concurrent_to_uuid, ip_address *ip,
  const char *content_type, FILE *body, off_t payload_offset);

#endif /* WARC_H */
