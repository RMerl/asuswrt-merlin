#ifndef __QMI_STRUCT_H
#define __QMI_STRUCT_H

struct qmux {
	uint16_t len;
	uint8_t flags;
	uint8_t service;
	uint8_t client;
} __packed;

struct tlv {
	uint8_t type;
	uint16_t len;
	uint8_t data[];
} __packed;

struct qmi_ctl {
	uint8_t transaction;
	uint16_t message;
	uint16_t tlv_len;
	struct tlv tlv[];
} __packed;

struct qmi_svc {
	uint16_t transaction;
	uint16_t message;
	uint16_t tlv_len;
	struct tlv tlv[];
} __packed;

struct qmi_msg {
	uint8_t marker;
	struct qmux qmux;
	uint8_t flags;
	union {
		struct qmi_ctl ctl;
		struct qmi_svc svc;
	};
} __packed;

#endif
