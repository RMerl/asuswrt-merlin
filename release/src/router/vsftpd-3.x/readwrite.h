#ifndef VSF_READWRITE_H
#define VSF_READWRITE_H

struct vsf_session;
struct mystr;

enum EVSFRWTarget
{
  kVSFRWControl = 1,
  kVSFRWData
};

int ftp_write_str(const struct vsf_session* p_sess, const struct mystr* p_str,
                  enum EVSFRWTarget target);
int ftp_read_data(struct vsf_session* p_sess, char* p_buf, unsigned int len);
int ftp_write_data(const struct vsf_session* p_sess, const char* p_buf,
                   unsigned int len);
int ftp_getline(struct vsf_session* p_sess, struct mystr* p_str, char* p_buf);

#endif /* VSF_READWRITE_H */

