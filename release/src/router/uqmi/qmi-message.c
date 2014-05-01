#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "qmi-message.h"

static uint8_t buf[QMI_BUFFER_LEN];
static unsigned int buf_ofs;

uint8_t *__qmi_get_buf(unsigned int *ofs)
{
	*ofs = buf_ofs;
	return buf;
}

void __qmi_alloc_reset(void)
{
	buf_ofs = 0;
}

void *__qmi_alloc_static(unsigned int len)
{
	void *ret;

	if (buf_ofs + len > sizeof(buf)) {
		fprintf(stderr, "ERROR: static buffer for message data too small\n");
		abort();
	}

	ret = &buf[buf_ofs];
	buf_ofs += len;
	memset(ret, 0, len);
	return ret;
}

char *__qmi_copy_string(void *data, unsigned int len)
{
	char *res;

	res = (char *) &buf[buf_ofs];
	buf_ofs += len + 1;
	memcpy(res, data, len);
	res[len] = 0;
	return res;
}

struct tlv *tlv_get_next(void **buf, unsigned int *buflen)
{
	struct tlv *tlv = NULL;
	unsigned int tlv_len;

	if (*buflen < sizeof(*tlv))
		return NULL;

	tlv = *buf;
	tlv_len = le16_to_cpu(tlv->len) + sizeof(*tlv);
	if (tlv_len > *buflen)
		return NULL;

	*buflen -= tlv_len;
	*buf += tlv_len;
	return tlv;
}

static struct tlv *qmi_msg_next_tlv(struct qmi_msg *qm, int add)
{
	int tlv_len;
	void *tlv;

	if (qm->qmux.service == QMI_SERVICE_CTL) {
		tlv = qm->ctl.tlv;
		tlv_len = le16_to_cpu(qm->ctl.tlv_len);
		qm->ctl.tlv_len = cpu_to_le16(tlv_len + add);
	} else {
		tlv = qm->svc.tlv;
		tlv_len = le16_to_cpu(qm->svc.tlv_len);
		qm->svc.tlv_len = cpu_to_le16(tlv_len + add);
	}

	tlv += tlv_len;

	return tlv;
}

void tlv_new(struct qmi_msg *qm, uint8_t type, uint16_t len, void *data)
{
	struct tlv *tlv;

	tlv = qmi_msg_next_tlv(qm, sizeof(*tlv) + len);
	tlv->type = type;
	tlv->len = cpu_to_le16(len);
	memcpy(tlv->data, data, len);
}

void qmi_init_request_message(struct qmi_msg *qm, QmiService service)
{
	memset(qm, 0, sizeof(*qm));
	qm->marker = 1;
	qm->qmux.service = service;
}

int qmi_complete_request_message(struct qmi_msg *qm)
{
	void *tlv_end = qmi_msg_next_tlv(qm, 0);
	void *msg_start = &qm->qmux;

	qm->qmux.len = cpu_to_le16(tlv_end - msg_start);
	return tlv_end - msg_start + 1;
}

int qmi_check_message_status(void *tlv_buf, unsigned int len)
{
	struct tlv *tlv;
	struct {
		uint16_t status;
		uint16_t code;
	} __packed *status;

	while ((tlv = tlv_get_next(&tlv_buf, &len)) != NULL) {
		if (tlv->type != 2)
			continue;

		if (tlv_data_len(tlv) != sizeof(*status))
			return QMI_ERROR_INVALID_DATA;

		status = (void *) tlv->data;
		if (!status->status)
			return 0;

		return le16_to_cpu(status->code);
	}

	return QMI_ERROR_NO_DATA;
}

void *qmi_msg_get_tlv_buf(struct qmi_msg *qm, int *tlv_len)
{
	void *ptr;
	int len;

	if (qm->qmux.service == QMI_SERVICE_CTL) {
		ptr = qm->ctl.tlv;
		len = qm->ctl.tlv_len;
	} else {
		ptr = qm->svc.tlv;
		len = qm->svc.tlv_len;
	}

	if (tlv_len)
		*tlv_len = len;

	return ptr;
}


