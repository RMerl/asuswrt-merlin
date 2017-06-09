#ifndef VSF_IPADDRPARSE_H
#define VSF_IPADDRPARSE_H

struct mystr;

/* Effectively doing the same sort of job as inet_pton. Since inet_pton does
 * a non-trivial amount of parsing, we'll do it ourselves for maximum security
 * and safety.
 */

const unsigned char* vsf_sysutil_parse_ipv6(const struct mystr* p_str);

const unsigned char* vsf_sysutil_parse_ipv4(const struct mystr* p_str);

const unsigned char* vsf_sysutil_parse_uchar_string_sep(
  const struct mystr* p_str, char sep, unsigned char* p_items,
  unsigned int items);

#endif /* VSF_IPADDRPARSE_H */

