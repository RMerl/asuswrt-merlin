/* HEADER Testing read_config_read_octet_string_const(). */

struct read_config_testcase {
    /*
     * inputs 
     */
    const char     *(*pf) (const char * readfrom, u_char ** str,
                           size_t * len);
    const char     *readfrom;
    size_t          obuf_len;

    /*
     * expected outputs 
     */
    size_t          expected_offset;
    const u_char   *expected_output;
    size_t          expected_len;
};

static const u_char obuf1[] = { 1, 0, 2 };
static const u_char obuf2[] = { 'a', 'b', 'c', 0 };

static const struct read_config_testcase test_input[] = {
    { &read_config_read_octet_string_const, "",           1, -1, NULL,  0 },
    { &read_config_read_octet_string_const, "0x0",        1, -1, NULL,  1 },
    { &read_config_read_octet_string_const, "0x0 0",      1, -1, NULL,  1 },

    { &read_config_read_octet_string_const, "0x010002",   1, -1, NULL,  0 },
    { &read_config_read_octet_string_const, "0x010002",   2, -1, NULL,  0 },
    { &read_config_read_octet_string_const, "0x010002",   3, -1, obuf1, 0 },
    { &read_config_read_octet_string_const, "0x010002",   4, -1, obuf1, 3 },
    { &read_config_read_octet_string_const, "0x010002 0", 4,  9, obuf1, 3 },
    { &read_config_read_octet_string_const, "0x010002",   0, -1, obuf1, 3 },

    { &read_config_read_octet_string_const, "abc",        1, -1, NULL,  0 },
    { &read_config_read_octet_string_const, "abc z",      1,  4, NULL,  0 },
    { &read_config_read_octet_string_const, "abc",        2, -1, NULL,  1 },
    { &read_config_read_octet_string_const, "abc",        3, -1, obuf2, 2 },
    { &read_config_read_octet_string_const, "abc",        4, -1, obuf2, 3 },
    { &read_config_read_octet_string_const, "abc z",      4,  4, obuf2, 3 },
    { &read_config_read_octet_string_const, "abc",        0, -1, obuf2, 3 },
};

unsigned int i, j, ok;

for (i = 0; i < sizeof(test_input) / sizeof(test_input[0]); i++) {
    const struct read_config_testcase *const p = &test_input[i];
    size_t          len = p->obuf_len;
    u_char         *str = len > 0 ? malloc(len) : NULL;
    const char     *result;
    size_t          offset;

    fflush(stdout);
    result = (p->pf) (p->readfrom, &str, &len);
    offset = result ? result - p->readfrom : -1;
    OKF(offset == p->expected_offset,
        ("test %d: expected offset %zd, got offset %" NETSNMP_PRIz "d",
         i, p->expected_offset, offset));
    if (offset == p->expected_offset) {
        OKF(len == p->expected_len,
            ("test %d: expected length %" NETSNMP_PRIz "d, got length %"
             NETSNMP_PRIz "d", i, p->expected_len, len));
        if (len == p->expected_len) {
            ok = len < 0 || !p->expected_output
                || memcmp(str, p->expected_output, len) == 0
                || p->expected_output[len] != 0;
            OKF(ok, ("test %d: output buffer mismatch", i));
            if (!ok) {
                printf("Expected: ");
                for (j = 0; j < p->expected_len; ++j)
                    printf("%02x ", p->expected_output[j]);
                printf("\nActual:   ");
                for (j = 0; j < len; ++j)
                    printf("%02x ", str[j]);
                printf("\n");
            }
        }
    }

    if (str)
        free(str);
}
