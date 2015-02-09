/*
 * net/9p/protocol.c
 *
 * 9P Protocol Support Code
 *
 *  Copyright (C) 2008 by Eric Van Hensbergen <ericvh@gmail.com>
 *
 *  Base on code from Anthony Liguori <aliguori@us.ibm.com>
 *  Copyright (C) 2008 by IBM, Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 *  Free Software Foundation
 *  51 Franklin Street, Fifth Floor
 *  Boston, MA  02111-1301  USA
 *
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <net/9p/9p.h>
#include <net/9p/client.h>
#include "protocol.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef offset_of
#define offset_of(type, memb) \
	((unsigned long)(&((type *)0)->memb))
#endif
#ifndef container_of
#define container_of(obj, type, memb) \
	((type *)(((char *)obj) - offset_of(type, memb)))
#endif

static int
p9pdu_writef(struct p9_fcall *pdu, int proto_version, const char *fmt, ...);

#ifdef CONFIG_NET_9P_DEBUG
void
p9pdu_dump(int way, struct p9_fcall *pdu)
{
	int i, n;
	u8 *data = pdu->sdata;
	int datalen = pdu->size;
	char buf[255];
	int buflen = 255;

	i = n = 0;
	if (datalen > (buflen-16))
		datalen = buflen-16;
	while (i < datalen) {
		n += scnprintf(buf + n, buflen - n, "%02x ", data[i]);
		if (i%4 == 3)
			n += scnprintf(buf + n, buflen - n, " ");
		if (i%32 == 31)
			n += scnprintf(buf + n, buflen - n, "\n");

		i++;
	}
	n += scnprintf(buf + n, buflen - n, "\n");

	if (way)
		P9_DPRINTK(P9_DEBUG_PKT, "[[[(%d) %s\n", datalen, buf);
	else
		P9_DPRINTK(P9_DEBUG_PKT, "]]](%d) %s\n", datalen, buf);
}
#else
void
p9pdu_dump(int way, struct p9_fcall *pdu)
{
}
#endif
EXPORT_SYMBOL(p9pdu_dump);

void p9stat_free(struct p9_wstat *stbuf)
{
	kfree(stbuf->name);
	kfree(stbuf->uid);
	kfree(stbuf->gid);
	kfree(stbuf->muid);
	kfree(stbuf->extension);
}
EXPORT_SYMBOL(p9stat_free);

static size_t pdu_read(struct p9_fcall *pdu, void *data, size_t size)
{
	size_t len = MIN(pdu->size - pdu->offset, size);
	memcpy(data, &pdu->sdata[pdu->offset], len);
	pdu->offset += len;
	return size - len;
}

static size_t pdu_write(struct p9_fcall *pdu, const void *data, size_t size)
{
	size_t len = MIN(pdu->capacity - pdu->size, size);
	memcpy(&pdu->sdata[pdu->size], data, len);
	pdu->size += len;
	return size - len;
}

static size_t
pdu_write_u(struct p9_fcall *pdu, const char __user *udata, size_t size)
{
	size_t len = MIN(pdu->capacity - pdu->size, size);
	int err = copy_from_user(&pdu->sdata[pdu->size], udata, len);
	if (err)
		printk(KERN_WARNING "pdu_write_u returning: %d\n", err);

	pdu->size += len;
	return size - len;
}

/*
	b - int8_t
	w - int16_t
	d - int32_t
	q - int64_t
	s - string
	S - stat
	Q - qid
	D - data blob (int32_t size followed by void *, results are not freed)
	T - array of strings (int16_t count, followed by strings)
	R - array of qids (int16_t count, followed by qids)
	A - stat for 9p2000.L (p9_stat_dotl)
	? - if optional = 1, continue parsing
*/

static int
p9pdu_vreadf(struct p9_fcall *pdu, int proto_version, const char *fmt,
	va_list ap)
{
	const char *ptr;
	int errcode = 0;

	for (ptr = fmt; *ptr; ptr++) {
		switch (*ptr) {
		case 'b':{
				int8_t *val = va_arg(ap, int8_t *);
				if (pdu_read(pdu, val, sizeof(*val))) {
					errcode = -EFAULT;
					break;
				}
			}
			break;
		case 'w':{
				int16_t *val = va_arg(ap, int16_t *);
				__le16 le_val;
				if (pdu_read(pdu, &le_val, sizeof(le_val))) {
					errcode = -EFAULT;
					break;
				}
				*val = le16_to_cpu(le_val);
			}
			break;
		case 'd':{
				int32_t *val = va_arg(ap, int32_t *);
				__le32 le_val;
				if (pdu_read(pdu, &le_val, sizeof(le_val))) {
					errcode = -EFAULT;
					break;
				}
				*val = le32_to_cpu(le_val);
			}
			break;
		case 'q':{
				int64_t *val = va_arg(ap, int64_t *);
				__le64 le_val;
				if (pdu_read(pdu, &le_val, sizeof(le_val))) {
					errcode = -EFAULT;
					break;
				}
				*val = le64_to_cpu(le_val);
			}
			break;
		case 's':{
				char **sptr = va_arg(ap, char **);
				int16_t len;
				int size;

				errcode = p9pdu_readf(pdu, proto_version,
								"w", &len);
				if (errcode)
					break;

				size = MAX(len, 0);

				*sptr = kmalloc(size + 1, GFP_KERNEL);
				if (*sptr == NULL) {
					errcode = -EFAULT;
					break;
				}
				if (pdu_read(pdu, *sptr, size)) {
					errcode = -EFAULT;
					kfree(*sptr);
					*sptr = NULL;
				} else
					(*sptr)[size] = 0;
			}
			break;
		case 'Q':{
				struct p9_qid *qid =
				    va_arg(ap, struct p9_qid *);

				errcode = p9pdu_readf(pdu, proto_version, "bdq",
						      &qid->type, &qid->version,
						      &qid->path);
			}
			break;
		case 'S':{
				struct p9_wstat *stbuf =
				    va_arg(ap, struct p9_wstat *);

				memset(stbuf, 0, sizeof(struct p9_wstat));
				stbuf->n_uid = stbuf->n_gid = stbuf->n_muid =
									-1;
				errcode =
				    p9pdu_readf(pdu, proto_version,
						"wwdQdddqssss?sddd",
						&stbuf->size, &stbuf->type,
						&stbuf->dev, &stbuf->qid,
						&stbuf->mode, &stbuf->atime,
						&stbuf->mtime, &stbuf->length,
						&stbuf->name, &stbuf->uid,
						&stbuf->gid, &stbuf->muid,
						&stbuf->extension,
						&stbuf->n_uid, &stbuf->n_gid,
						&stbuf->n_muid);
				if (errcode)
					p9stat_free(stbuf);
			}
			break;
		case 'D':{
				int32_t *count = va_arg(ap, int32_t *);
				void **data = va_arg(ap, void **);

				errcode =
				    p9pdu_readf(pdu, proto_version, "d", count);
				if (!errcode) {
					*count =
					    MIN(*count,
						pdu->size - pdu->offset);
					*data = &pdu->sdata[pdu->offset];
				}
			}
			break;
		case 'T':{
				int16_t *nwname = va_arg(ap, int16_t *);
				char ***wnames = va_arg(ap, char ***);

				errcode = p9pdu_readf(pdu, proto_version,
								"w", nwname);
				if (!errcode) {
					*wnames =
					    kmalloc(sizeof(char *) * *nwname,
						    GFP_KERNEL);
					if (!*wnames)
						errcode = -ENOMEM;
				}

				if (!errcode) {
					int i;

					for (i = 0; i < *nwname; i++) {
						errcode =
						    p9pdu_readf(pdu,
								proto_version,
								"s",
								&(*wnames)[i]);
						if (errcode)
							break;
					}
				}

				if (errcode) {
					if (*wnames) {
						int i;

						for (i = 0; i < *nwname; i++)
							kfree((*wnames)[i]);
					}
					kfree(*wnames);
					*wnames = NULL;
				}
			}
			break;
		case 'R':{
				int16_t *nwqid = va_arg(ap, int16_t *);
				struct p9_qid **wqids =
				    va_arg(ap, struct p9_qid **);

				*wqids = NULL;

				errcode =
				    p9pdu_readf(pdu, proto_version, "w", nwqid);
				if (!errcode) {
					*wqids =
					    kmalloc(*nwqid *
						    sizeof(struct p9_qid),
						    GFP_KERNEL);
					if (*wqids == NULL)
						errcode = -ENOMEM;
				}

				if (!errcode) {
					int i;

					for (i = 0; i < *nwqid; i++) {
						errcode =
						    p9pdu_readf(pdu,
								proto_version,
								"Q",
								&(*wqids)[i]);
						if (errcode)
							break;
					}
				}

				if (errcode) {
					kfree(*wqids);
					*wqids = NULL;
				}
			}
			break;
		case 'A': {
				struct p9_stat_dotl *stbuf =
				    va_arg(ap, struct p9_stat_dotl *);

				memset(stbuf, 0, sizeof(struct p9_stat_dotl));
				errcode =
				    p9pdu_readf(pdu, proto_version,
					"qQdddqqqqqqqqqqqqqqq",
					&stbuf->st_result_mask,
					&stbuf->qid,
					&stbuf->st_mode,
					&stbuf->st_uid, &stbuf->st_gid,
					&stbuf->st_nlink,
					&stbuf->st_rdev, &stbuf->st_size,
					&stbuf->st_blksize, &stbuf->st_blocks,
					&stbuf->st_atime_sec,
					&stbuf->st_atime_nsec,
					&stbuf->st_mtime_sec,
					&stbuf->st_mtime_nsec,
					&stbuf->st_ctime_sec,
					&stbuf->st_ctime_nsec,
					&stbuf->st_btime_sec,
					&stbuf->st_btime_nsec,
					&stbuf->st_gen,
					&stbuf->st_data_version);
			}
			break;
		case '?':
			if ((proto_version != p9_proto_2000u) &&
				(proto_version != p9_proto_2000L))
				return 0;
			break;
		default:
			BUG();
			break;
		}

		if (errcode)
			break;
	}

	return errcode;
}

int
p9pdu_vwritef(struct p9_fcall *pdu, int proto_version, const char *fmt,
	va_list ap)
{
	const char *ptr;
	int errcode = 0;

	for (ptr = fmt; *ptr; ptr++) {
		switch (*ptr) {
		case 'b':{
				int8_t val = va_arg(ap, int);
				if (pdu_write(pdu, &val, sizeof(val)))
					errcode = -EFAULT;
			}
			break;
		case 'w':{
				__le16 val = cpu_to_le16(va_arg(ap, int));
				if (pdu_write(pdu, &val, sizeof(val)))
					errcode = -EFAULT;
			}
			break;
		case 'd':{
				__le32 val = cpu_to_le32(va_arg(ap, int32_t));
				if (pdu_write(pdu, &val, sizeof(val)))
					errcode = -EFAULT;
			}
			break;
		case 'q':{
				__le64 val = cpu_to_le64(va_arg(ap, int64_t));
				if (pdu_write(pdu, &val, sizeof(val)))
					errcode = -EFAULT;
			}
			break;
		case 's':{
				const char *sptr = va_arg(ap, const char *);
				int16_t len = 0;
				if (sptr)
					len = MIN(strlen(sptr), USHRT_MAX);

				errcode = p9pdu_writef(pdu, proto_version,
								"w", len);
				if (!errcode && pdu_write(pdu, sptr, len))
					errcode = -EFAULT;
			}
			break;
		case 'Q':{
				const struct p9_qid *qid =
				    va_arg(ap, const struct p9_qid *);
				errcode =
				    p9pdu_writef(pdu, proto_version, "bdq",
						 qid->type, qid->version,
						 qid->path);
			} break;
		case 'S':{
				const struct p9_wstat *stbuf =
				    va_arg(ap, const struct p9_wstat *);
				errcode =
				    p9pdu_writef(pdu, proto_version,
						 "wwdQdddqssss?sddd",
						 stbuf->size, stbuf->type,
						 stbuf->dev, &stbuf->qid,
						 stbuf->mode, stbuf->atime,
						 stbuf->mtime, stbuf->length,
						 stbuf->name, stbuf->uid,
						 stbuf->gid, stbuf->muid,
						 stbuf->extension, stbuf->n_uid,
						 stbuf->n_gid, stbuf->n_muid);
			} break;
		case 'D':{
				int32_t count = va_arg(ap, int32_t);
				const void *data = va_arg(ap, const void *);

				errcode = p9pdu_writef(pdu, proto_version, "d",
									count);
				if (!errcode && pdu_write(pdu, data, count))
					errcode = -EFAULT;
			}
			break;
		case 'U':{
				int32_t count = va_arg(ap, int32_t);
				const char __user *udata =
						va_arg(ap, const void __user *);
				errcode = p9pdu_writef(pdu, proto_version, "d",
									count);
				if (!errcode && pdu_write_u(pdu, udata, count))
					errcode = -EFAULT;
			}
			break;
		case 'T':{
				int16_t nwname = va_arg(ap, int);
				const char **wnames = va_arg(ap, const char **);

				errcode = p9pdu_writef(pdu, proto_version, "w",
									nwname);
				if (!errcode) {
					int i;

					for (i = 0; i < nwname; i++) {
						errcode =
						    p9pdu_writef(pdu,
								proto_version,
								 "s",
								 wnames[i]);
						if (errcode)
							break;
					}
				}
			}
			break;
		case 'R':{
				int16_t nwqid = va_arg(ap, int);
				struct p9_qid *wqids =
				    va_arg(ap, struct p9_qid *);

				errcode = p9pdu_writef(pdu, proto_version, "w",
									nwqid);
				if (!errcode) {
					int i;

					for (i = 0; i < nwqid; i++) {
						errcode =
						    p9pdu_writef(pdu,
								proto_version,
								 "Q",
								 &wqids[i]);
						if (errcode)
							break;
					}
				}
			}
			break;
		case 'I':{
				struct p9_iattr_dotl *p9attr = va_arg(ap,
							struct p9_iattr_dotl *);

				errcode = p9pdu_writef(pdu, proto_version,
							"ddddqqqqq",
							p9attr->valid,
							p9attr->mode,
							p9attr->uid,
							p9attr->gid,
							p9attr->size,
							p9attr->atime_sec,
							p9attr->atime_nsec,
							p9attr->mtime_sec,
							p9attr->mtime_nsec);
			}
			break;
		case '?':
			if ((proto_version != p9_proto_2000u) &&
				(proto_version != p9_proto_2000L))
				return 0;
			break;
		default:
			BUG();
			break;
		}

		if (errcode)
			break;
	}

	return errcode;
}

int p9pdu_readf(struct p9_fcall *pdu, int proto_version, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = p9pdu_vreadf(pdu, proto_version, fmt, ap);
	va_end(ap);

	return ret;
}

static int
p9pdu_writef(struct p9_fcall *pdu, int proto_version, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = p9pdu_vwritef(pdu, proto_version, fmt, ap);
	va_end(ap);

	return ret;
}

int p9stat_read(char *buf, int len, struct p9_wstat *st, int proto_version)
{
	struct p9_fcall fake_pdu;
	int ret;

	fake_pdu.size = len;
	fake_pdu.capacity = len;
	fake_pdu.sdata = buf;
	fake_pdu.offset = 0;

	ret = p9pdu_readf(&fake_pdu, proto_version, "S", st);
	if (ret) {
		P9_DPRINTK(P9_DEBUG_9P, "<<< p9stat_read failed: %d\n", ret);
		p9pdu_dump(1, &fake_pdu);
	}

	return ret;
}
EXPORT_SYMBOL(p9stat_read);

int p9pdu_prepare(struct p9_fcall *pdu, int16_t tag, int8_t type)
{
	return p9pdu_writef(pdu, 0, "dbw", 0, type, tag);
}

int p9pdu_finalize(struct p9_fcall *pdu)
{
	int size = pdu->size;
	int err;

	pdu->size = 0;
	err = p9pdu_writef(pdu, 0, "d", size);
	pdu->size = size;

#ifdef CONFIG_NET_9P_DEBUG
	if ((p9_debug_level & P9_DEBUG_PKT) == P9_DEBUG_PKT)
		p9pdu_dump(0, pdu);
#endif

	P9_DPRINTK(P9_DEBUG_9P, ">>> size=%d type: %d tag: %d\n", pdu->size,
							pdu->id, pdu->tag);

	return err;
}

void p9pdu_reset(struct p9_fcall *pdu)
{
	pdu->offset = 0;
	pdu->size = 0;
}

int p9dirent_read(char *buf, int len, struct p9_dirent *dirent,
						int proto_version)
{
	struct p9_fcall fake_pdu;
	int ret;
	char *nameptr;

	fake_pdu.size = len;
	fake_pdu.capacity = len;
	fake_pdu.sdata = buf;
	fake_pdu.offset = 0;

	ret = p9pdu_readf(&fake_pdu, proto_version, "Qqbs", &dirent->qid,
			&dirent->d_off, &dirent->d_type, &nameptr);
	if (ret) {
		P9_DPRINTK(P9_DEBUG_9P, "<<< p9dirent_read failed: %d\n", ret);
		p9pdu_dump(1, &fake_pdu);
		goto out;
	}

	strcpy(dirent->d_name, nameptr);

out:
	return fake_pdu.offset;
}
EXPORT_SYMBOL(p9dirent_read);
