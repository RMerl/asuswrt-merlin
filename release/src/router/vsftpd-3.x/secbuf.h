#ifndef VSF_SECBUF_H
#define VSF_SECBUF_H

/* vsf_secbuf_alloc()
 * PURPOSE
 * Allocate a "secure buffer". A secure buffer is one which will attempt to
 * catch out of bounds accesses by crashing the program (rather than
 * corrupting memory). It works by using UNIX memory protection. It isn't
 * foolproof.
 * PARAMETERS
 * p_ptr        - pointer to a pointer which is to contain the secure buffer.
 *                Any previous buffer pointed to is freed.
 * size         - size in bytes required for the secure buffer.
 */
void vsf_secbuf_alloc(char** p_ptr, unsigned int size);

/* vsf_secbuf_free()
 * PURPOSE
 * Frees a "secure buffer".
 * PARAMETERS
 * p_ptr        - pointer to a pointer containing the buffer to be freed. The
 *                buffer pointer is nullified by this call.
 */
void vsf_secbuf_free(char** p_ptr);

#endif /* VSF_SECBUF_H */

