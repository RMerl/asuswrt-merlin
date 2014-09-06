/* -*- C -*- */
#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x501
#endif

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/library/asn1.h>
#include <net-snmp/library/snmp_impl.h>

#define TEST_CONSTANT(value, name, C)           \
    if (strEQ(name, #C)) {                      \
        *value = C;                             \
        return 0;                               \
    }

static int constant_ASN_O(double *value, const char *name, const int len)
{
    switch (len >= 5 ? name[5] : -1) {
    case 'B':
        TEST_CONSTANT(value, name, ASN_OBJECT_ID);
        break;
    case 'C':
        TEST_CONSTANT(value, name, ASN_OCTET_STR);
        break;
    case 'P':
        TEST_CONSTANT(value, name, ASN_OPAQUE);
        break;
    }
    return EINVAL;
}

static int constant_ASN_B(double *value, const char *name, const int len)
{
    switch (len >= 5 ? name[5] : -1) {
    case 'I':
        TEST_CONSTANT(value, name, ASN_BIT_STR);
        break;
    case 'O':
        TEST_CONSTANT(value, name, ASN_BOOLEAN);
        break;
    }
    return EINVAL;
}

static int constant_ASN_S(double *value, const char *name, const int len)
{
    switch (len >= 6 ? name[6] : -1) {
    case 'Q':
        TEST_CONSTANT(value, name, ASN_SEQUENCE);
        break;
    case 'T':
        TEST_CONSTANT(value, name, ASN_SET);
        break;
    }
    return EINVAL;
}

static int constant_ASN_C(double *value, const char *name, const int len)
{
    switch (len >= 11 ? name[11] : -1) {
    case '\0':
        TEST_CONSTANT(value, name, ASN_COUNTER);
        break;
    case '6':
        TEST_CONSTANT(value, name, ASN_COUNTER64);
        break;
    }
    return EINVAL;
}

static int constant_ASN_U(double *value, const char *name, const int len)
{
    switch (len >= 12 ? name[12] : -1) {
    case '\0':
	TEST_CONSTANT(value, name, ASN_UNSIGNED);
        break;
#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
    case '6':
	TEST_CONSTANT(value, name, ASN_UNSIGNED64);
        break;
#endif
    }
    return EINVAL;
}

static int constant_ASN_IN(double *value, const char *name, const int len)
{
    switch (len >= 11 ? name[11] : -1) {
    case '\0':
        TEST_CONSTANT(value, name, ASN_INTEGER);
        break;
#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
    case '6':
        TEST_CONSTANT(value, name, ASN_INTEGER64);
        break;
#endif
    }
    return EINVAL;
}

static int constant_ASN_I(double* value, const char *name, const int len)
{
    switch (len >= 5 ? name[5] : -1) {
    case 'N':
	return constant_ASN_IN(value, name, len);
    case 'P':
        TEST_CONSTANT(value, name, ASN_IPADDRESS);
        break;
    }
    return EINVAL;
}

static int constant(double *value, const char *const name, const int len)
{
    if (!strnEQ(name, "ASN_", 4))
        return EINVAL;

    switch (name[4]) {
    case 'A':
	TEST_CONSTANT(value, name, ASN_APPLICATION);
        break;
    case 'B':
	return constant_ASN_B(value, name, len);
    case 'C':
	return constant_ASN_C(value, name, len);
#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
    case 'D':
        TEST_CONSTANT(value, name, ASN_DOUBLE);
        break;
    case 'F':
        TEST_CONSTANT(value, name, ASN_FLOAT);
        break;
#endif
    case 'G':
        TEST_CONSTANT(value, name, ASN_GAUGE);
        break;
    case 'I':
	return constant_ASN_I(value, name, len);
    case 'N':
        TEST_CONSTANT(value, name, ASN_NULL);
        break;
    case 'O':
	return constant_ASN_O(value, name, len);
    case 'S':
	return constant_ASN_S(value, name, len);
    case 'T':
        TEST_CONSTANT(value, name, ASN_TIMETICKS);
        break;
    case 'U':
	return constant_ASN_U(value, name, len);
    }
    return EINVAL;
}


MODULE = NetSNMP::ASN		PACKAGE = NetSNMP::ASN		


void
constant(sv)
    PREINIT:
	STRLEN		len;
    INPUT:
	SV *		sv
	char *		s = SvPV(sv, len);
    INIT:
        int status;
        double value;
    PPCODE:
        value = 0;
        status = constant(&value, s, len);
        XPUSHs(sv_2mortal(newSVuv(status)));
        XPUSHs(sv_2mortal(newSVnv(value)));
