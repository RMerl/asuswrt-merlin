#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "uqmi.h"
#include "qmi-errors.h"
#include "qmi-errors.c"

bool cancel_all_requests = false;

#define __qmi_service(_n) [__##_n] = _n
static const uint8_t qmi_services[__QMI_SERVICE_LAST] = {
	__qmi_services
};
#undef __qmi_service

static union {
	char buf[512];
	struct qmi_msg msg;
} msgbuf;

#ifdef DEBUG_PACKET
void dump_packet(const char *prefix, void *ptr, int len)
{
	unsigned char *data = ptr;
	int i;

	fprintf(stderr, "%s:", prefix);
	for (i = 0; i < len; i++)
		fprintf(stderr, " %02x", data[i]);
	fprintf(stderr, "\n");
}
#endif

static int
qmi_get_service_idx(QmiService svc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(qmi_services); i++)
		if (qmi_services[i] == svc)
			return i;

	return -1;
}

static void __qmi_request_complete(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg)
{
	void *tlv_buf;
	int tlv_len;

	if (!req->pending)
		return;

	req->pending = false;
	list_del(&req->list);

	if (msg) {
		tlv_buf = qmi_msg_get_tlv_buf(msg, &tlv_len);
		req->ret = qmi_check_message_status(tlv_buf, tlv_len);
		if (req->ret)
			msg = NULL;
	} else {
		req->ret = QMI_ERROR_CANCELLED;
	}

	if (req->cb && (msg || !req->no_error_cb))
		req->cb(qmi, req, msg);

	if (req->complete) {
		*req->complete = true;
		uloop_cancelled = true;
	}
}

static void qmi_process_msg(struct qmi_dev *qmi, struct qmi_msg *msg)
{
	struct qmi_request *req;
	uint16_t tid;

	if (msg->qmux.service == QMI_SERVICE_CTL)
		tid = msg->ctl.transaction;
	else
		tid = le16_to_cpu(msg->svc.transaction);

	list_for_each_entry(req, &qmi->req, list) {
		if (req->service != msg->qmux.service)
			continue;

		if (req->tid != tid)
			continue;

		__qmi_request_complete(qmi, req, msg);
		return;
	}
}

static void qmi_notify_read(struct ustream *us, int bytes)
{
	struct qmi_dev *qmi = container_of(us, struct qmi_dev, sf.stream);
	struct qmi_msg *msg;
	char *buf;
	int len, msg_len;

	while (1) {
		buf = ustream_get_read_buf(us, &len);
		if (!buf || !len)
			return;

		if (len < offsetof(struct qmi_msg, flags))
			return;

		msg = (struct qmi_msg *) buf;
		msg_len = le16_to_cpu(msg->qmux.len) + 1;
		if (len < msg_len)
			return;

		dump_packet("Received packet", msg, msg_len);
		qmi_process_msg(qmi, msg);
		ustream_consume(us, msg_len);
	}
}

int qmi_request_start(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, request_cb cb)
{
	int len = qmi_complete_request_message(msg);
	uint16_t tid;

	memset(req, 0, sizeof(*req));
	req->ret = -1;
	req->service = msg->qmux.service;
	if (req->service == QMI_SERVICE_CTL) {
		tid = qmi->ctl_tid++;
		msg->ctl.transaction = tid;
	} else {
		int idx = qmi_get_service_idx(req->service);

		if (idx < 0)
			return -1;

		tid = qmi->service_data[idx].tid++;
		msg->svc.transaction = cpu_to_le16(tid);
		msg->qmux.client = qmi->service_data[idx].client_id;
	}

	req->tid = tid;
	req->cb = cb;
	req->pending = true;
	list_add(&req->list, &qmi->req);

	dump_packet("Send packet", msg, len);
	ustream_write(&qmi->sf.stream, (void *) msg, len, false);
	return 0;
}

void qmi_request_cancel(struct qmi_dev *qmi, struct qmi_request *req)
{
	req->cb = NULL;
	__qmi_request_complete(qmi, req, NULL);
}

int qmi_request_wait(struct qmi_dev *qmi, struct qmi_request *req)
{
	bool complete = false;
	bool cancelled;

	if (!req->pending)
		return req->ret;

	if (req->complete)
		*req->complete = true;

	req->complete = &complete;
	while (!complete) {
		cancelled = uloop_cancelled;
		uloop_cancelled = false;
		uloop_run();

		if (cancel_all_requests)
			qmi_request_cancel(qmi, req);

		uloop_cancelled = cancelled;
	}

	if (req->complete == &complete)
		req->complete = NULL;

	return req->ret;
}

struct qmi_connect_request {
	struct qmi_request req;
	int cid;
};

static void qmi_connect_service_cb(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg)
{
	struct qmi_ctl_allocate_cid_response res;
	struct qmi_connect_request *creq = container_of(req, struct qmi_connect_request, req);

	if (!msg)
		return;

	qmi_parse_ctl_allocate_cid_response(msg, &res);
	creq->cid = res.data.allocation_info.cid;
}

int qmi_service_connect(struct qmi_dev *qmi, QmiService svc, int client_id)
{
	struct qmi_ctl_allocate_cid_request creq = {
		QMI_INIT(service, svc)
	};
	struct qmi_connect_request req;
	int idx = qmi_get_service_idx(svc);
	struct qmi_msg *msg = &msgbuf.msg;

	if (idx < 0)
		return -1;

	if (qmi->service_connected & (1 << idx))
		return 0;

	if (client_id < 0) {
		qmi_set_ctl_allocate_cid_request(msg, &creq);
		qmi_request_start(qmi, &req.req, msg, qmi_connect_service_cb);
		qmi_request_wait(qmi, &req.req);

		if (req.req.ret)
			return req.req.ret;

		client_id = req.cid;
	} else {
		qmi->service_keep_cid |= (1 << idx);
	}

	qmi->service_data[idx].connected = true;
	qmi->service_data[idx].client_id = client_id;
	qmi->service_data[idx].tid = 1;
	qmi->service_connected |= (1 << idx);

	return 0;
}

static void __qmi_service_disconnect(struct qmi_dev *qmi, int idx)
{
	int client_id = qmi->service_data[idx].client_id;
	struct qmi_ctl_release_cid_request creq = {
		QMI_INIT_SEQUENCE(release_info,
			.service = qmi_services[idx],
			.cid = client_id,
		)
	};
	struct qmi_request req;
	struct qmi_msg *msg = &msgbuf.msg;

	qmi->service_connected &= ~(1 << idx);
	qmi->service_data[idx].client_id = -1;
	qmi->service_data[idx].tid = 0;

	qmi_set_ctl_release_cid_request(msg, &creq);
	qmi_request_start(qmi, &req, msg, NULL);
	qmi_request_wait(qmi, &req);
}

int qmi_service_release_client_id(struct qmi_dev *qmi, QmiService svc)
{
	int idx = qmi_get_service_idx(svc);
	qmi->service_release_cid |= 1 << idx;
	return 0;
}

static void qmi_close_all_services(struct qmi_dev *qmi)
{
	uint32_t connected = qmi->service_connected;
	int idx;

	qmi->service_keep_cid &= ~qmi->service_release_cid;
	for (idx = 0; connected; idx++, connected >>= 1) {
		if (!(connected & 1))
			continue;

		if (qmi->service_keep_cid & (1 << idx))
			continue;

		__qmi_service_disconnect(qmi, idx);
	}
}

int qmi_service_get_client_id(struct qmi_dev *qmi, QmiService svc)
{
	int idx = qmi_get_service_idx(svc);

	if (idx < 0)
		return -1;

	qmi->service_keep_cid |= (1 << idx);
	return qmi->service_data[idx].client_id;
}

int qmi_device_open(struct qmi_dev *qmi, const char *path)
{
	struct ustream *us = &qmi->sf.stream;
	int fd;

	uloop_init();

	fd = open(path, O_RDWR | O_EXCL | O_NONBLOCK | O_NOCTTY);
	if (fd < 0)
		return -1;

	us->notify_read = qmi_notify_read;
	ustream_fd_init(&qmi->sf, fd);
	INIT_LIST_HEAD(&qmi->req);
	qmi->ctl_tid = 1;

	return 0;
}

void qmi_device_close(struct qmi_dev *qmi)
{
	struct qmi_request *req;

	qmi_close_all_services(qmi);
	ustream_free(&qmi->sf.stream);
	close(qmi->sf.fd.fd);

	while (!list_empty(&qmi->req)) {
		req = list_first_entry(&qmi->req, struct qmi_request, list);
		qmi_request_cancel(qmi, req);
	}
}

QmiService qmi_service_get_by_name(const char *str)
{
	static const struct {
		const char *name;
		QmiService svc;
	} services[] = {
		{ "dms", QMI_SERVICE_DMS },
		{ "nas", QMI_SERVICE_NAS },
		{ "pds", QMI_SERVICE_PDS },
		{ "wds", QMI_SERVICE_WDS },
		{ "wms", QMI_SERVICE_WMS },
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(services); i++) {
		if (!strcasecmp(str, services[i].name))
			return services[i].svc;
	}

	return -1;
}

const char *qmi_get_error_str(int code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(qmi_errors); i++) {
		if (qmi_errors[i].code == code)
			return qmi_errors[i].text;
	}

	return "Unknown error";
}
