/* HEADER Testing ASN.1 encoding and decoding */

int i;

#if 0
snmp_set_do_debugging(TRUE);
debug_register_tokens("dumpv_recv,dumpv_send,asn");
#endif

{
    const long intval[] = {
        -0x80000000L,
        -0x7fffffffL,
        -0xffffL,
        -3,
        -1,
        0,
        1,
        3,
        0xffff,
        0x7fffffff,
    };
    for (i = 0; i < sizeof(intval)/sizeof(intval[0]); ++i) {
	u_char encoded[256];
	size_t encoded_length;
	u_char *build_result;
	size_t decoded_length;
	u_char decoded_type;
	long decoded_value = 0;
	u_char *parse_result;

	encoded_length = sizeof(encoded);
	build_result = asn_build_int(encoded, &encoded_length, ASN_INTEGER,
				     &intval[i], sizeof(intval[i]));
	OKF(build_result + encoded_length == encoded + sizeof(encoded),
	    ("asn_build_int(%ld)", intval[i]));
	decoded_length = sizeof(encoded) - encoded_length;
	parse_result = asn_parse_int(encoded, &decoded_length, &decoded_type,
				     &decoded_value, sizeof(decoded_value));
	OKF(parse_result == build_result && decoded_type == ASN_INTEGER
	    && decoded_value == intval[i],
	    ("asn_parse_int(asn_build_int(%ld)) %s; decoded type %d <> %d;"
	     " decoded value %ld",
	     intval[i], parse_result == build_result ? "succeeded" : "failed",
	     decoded_type, ASN_INTEGER, decoded_value));
    }
}

{
    const unsigned long intval[] = {
	0, 1, 3, 0xffff, 0x7fffffff, 0x80000000U, 0xffffffffU
    };
    for (i = 0; i < sizeof(intval)/sizeof(intval[0]); ++i) {
	u_char encoded[256];
	size_t encoded_length;
	u_char *build_result;
	size_t decoded_length;
	u_char decoded_type;
	unsigned long decoded_value = 0;
	u_char *parse_result;

	encoded_length = sizeof(encoded);
	build_result = asn_build_unsigned_int(encoded, &encoded_length,
					      ASN_UINTEGER,
					      &intval[i], sizeof(intval[i]));
	OKF(build_result + encoded_length == encoded + sizeof(encoded),
	    ("asn_build_unsigned_int(%lu)", intval[i]));
	decoded_length = sizeof(encoded) - encoded_length;
	parse_result = asn_parse_unsigned_int(encoded, &decoded_length,
					      &decoded_type, &decoded_value,
					      sizeof(decoded_value));
	OKF(parse_result && decoded_type == ASN_UINTEGER
	    && decoded_value == intval[i],
	    ("asn_parse_unsigned_int(asn_build_unsigned_int(%lu)) %s;"
	     " decoded type %d <> %d; decoded value %lu",
	     intval[i], parse_result == build_result ? "succeeded" : "failed",
	     decoded_type, ASN_UINTEGER, decoded_value));
    }
}

#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES

#define TOINT64(c) ((long long)(long)(c).high << 32 | (c).low)

{
    const struct counter64 intval[] = {
	{ 0x80000000U,          0U },
	{ 0x80000000U, 0xffffffffU },
	{ 0xffffffffU,           0 },
	{ 0xffffffffU, 0xffff0000U },
	{ 0xffffffffU, 0xfffffffcU },
	{ 0xffffffffU, 0xffffffffU },
	{           0,           0 },
	{           0,           1 },
	{           0,           3 },
	{           0,      0xffff },
	{           0,  0x7fffffff },
	{           1,           0 },
	{           1,  0xffffffff },
	{  0x7fffffff,           0 },
	{  0x7fffffff,  0xdeadbeef },
	{  0x7fffffff,  0xffffffff },
    };
    for (i = 0; i < sizeof(intval)/sizeof(intval[0]); ++i) {
	u_char encoded[256];
	size_t encoded_length;
	u_char *build_result;
	size_t decoded_length;
	u_char decoded_type;
	struct counter64 decoded_value = { };
	u_char *parse_result;

	encoded_length = sizeof(encoded);
	build_result = asn_build_signed_int64(encoded, &encoded_length,
					      ASN_OPAQUE_I64,
					      &intval[i], sizeof(intval[i]));
	OKF(build_result + encoded_length == encoded + sizeof(encoded),
	    ("asn_build_signed_int64(%lld)", TOINT64(intval[i])));
	decoded_length = sizeof(encoded) - encoded_length;
	parse_result = asn_parse_signed_int64(encoded, &decoded_length,
					      &decoded_type, &decoded_value,
					      sizeof(decoded_value));
	OKF(parse_result == build_result && decoded_type == ASN_OPAQUE_I64
	    && memcmp(&decoded_value, &intval[i], sizeof(decoded_value)) == 0,
	    ("asn_parse_signed_int64(asn_build_signed_int64(%lld)) %s;"
	     " decoded type %d <> %d; decoded value %lld",
	     TOINT64(intval[i]),
	     parse_result == build_result ? "succeeded" : "failed",
	     decoded_type, ASN_OPAQUE_I64, TOINT64(decoded_value)));
    }
}

#endif

#define TOUINT64(c) ((unsigned long long)(c).high << 32 | (c).low)

{
    const struct counter64 intval[] = {
	{          0,          0 },
	{          0,          1 },
	{          0,          3 },
	{          0,     0xffff },
	{          0, 0x7fffffff },
	{          0, 0x80000000 },
	{          0, 0xffffffff },
	{          1,          0 },
	{          1, 0xffffffff },
	{ 0x7fffffff,          0 },
	{ 0x7fffffff, 0xdeadbeef },
	{ 0x7fffffff, 0xffffffff },
	{ 0xffffffff,          0 },
	{ 0xffffffff, 0xdeadbeef },
	{ 0xffffffff, 0xffffffff },
    };
    for (i = 0; i < sizeof(intval)/sizeof(intval[0]); ++i) {
	u_char encoded[256];
	size_t encoded_length;
	u_char *build_result;
	size_t decoded_length;
	u_char decoded_type;
	struct counter64 decoded_value = { };
	u_char *parse_result;

	encoded_length = sizeof(encoded);
	build_result = asn_build_unsigned_int64(encoded, &encoded_length,
						ASN_COUNTER64,
						&intval[i], sizeof(intval[i]));
	OKF(build_result + encoded_length == encoded + sizeof(encoded),
	    ("asn_build_unsigned_int64(%llu)", TOUINT64(intval[i])));
	decoded_length = sizeof(encoded) - encoded_length;
	parse_result = asn_parse_unsigned_int64(encoded, &decoded_length,
						&decoded_type, &decoded_value,
						sizeof(decoded_value));
	OKF(parse_result && decoded_type == ASN_COUNTER64
	    && memcmp(&decoded_value, &intval[i], sizeof(decoded_value)) == 0,
	    ("asn_parse_unsigned_int64(asn_build_unsigned_int64(%llu)) %s;"
	     " decoded type %d <> %d; decoded value %llu",
	     TOUINT64(intval[i]),
	     parse_result == build_result ? "succeeded" : "failed",
	     decoded_type, ASN_COUNTER64, TOUINT64(decoded_value)));
    }
}
