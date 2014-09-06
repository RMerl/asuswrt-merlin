/* HEADER testing SCAPI hashing functions */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <stdio.h>

void print_hash(const char *label, u_char *MAC, size_t MAC_LEN);


int
main(int argc, char **argv) {
    u_char buf[] = "wes hardaker";
    u_char MAC[20];
    size_t MAC_LEN = 20;
    u_char sha1key[20] = "55555555555555555555";
    u_char md5key[16] = "5555555555555555";

    u_char sha1proper[20] = { 0x4a, 0x55, 0x2f, 0x65, 0x79, 0x3a, 0x49, 0x35,
                              0x37, 0x91, 0x51, 0x1d,
                              0xa0, 0x8c, 0x7a, 0x45, 0x50, 0x34, 0xd4, 0x23};
    u_char md5proper[16] = { 0xe5, 0x92, 0xfa, 0x4b, 0x06, 0xe6, 0x27, 0xd7,
                             0xc8, 0x18, 0xfa, 0x7a,
                             0xd0, 0x82, 0xeb, 0x66};
    int result;
    
    printf("1..2\n");

    memset(MAC, 0, MAC_LEN);
    sc_hash(usmHMACSHA1AuthProtocol, OID_LENGTH(usmHMACSHA1AuthProtocol),
            buf, sizeof(buf)-1,
            MAC, &MAC_LEN);

    print_hash("sha1 hash", MAC, MAC_LEN);

    memset(MAC, 0, MAC_LEN);
    sc_hash(usmHMACMD5AuthProtocol, OID_LENGTH(usmHMACMD5AuthProtocol),
            buf, sizeof(buf)-1,
            MAC, &MAC_LEN);

    print_hash("md5 hash", MAC, MAC_LEN);

    MAC_LEN = 20;
    sc_generate_keyed_hash(usmHMACSHA1AuthProtocol,
                           OID_LENGTH(usmHMACSHA1AuthProtocol),
                           sha1key, sizeof(sha1key),
                           buf, sizeof(buf)-1,
                           MAC, &MAC_LEN);

    print_hash("sha1 keyed", MAC, MAC_LEN);

    result =
        sc_check_keyed_hash(usmHMACSHA1AuthProtocol,
                            OID_LENGTH(usmHMACSHA1AuthProtocol),
                            sha1key, sizeof(sha1key),
                            buf, sizeof(buf)-1,
                            sha1proper, 12);
    if (0 == result) {
        printf("ok: 1 - sha1 keyed compare was equal\n");
    } else {
        printf("not ok: 1 - sha1 keyed compare was not equal\n");
    }
    

    MAC_LEN = 16;
    sc_generate_keyed_hash(usmHMACMD5AuthProtocol,
                           OID_LENGTH(usmHMACMD5AuthProtocol),
                           md5key, sizeof(md5key),
                           buf, sizeof(buf)-1,
                           MAC, &MAC_LEN);

    print_hash("md5 keyed", MAC, MAC_LEN);


    MAC_LEN = 16;
    result =
        sc_check_keyed_hash(usmHMACMD5AuthProtocol,
                            OID_LENGTH(usmHMACMD5AuthProtocol),
                            md5key, sizeof(md5key),
                            buf, sizeof(buf)-1,
                            md5proper, 12);

    if (0 == result) {
        printf("ok: 2 - md5 keyed compare was equal\n");
    } else {
        printf("not ok: 2 - md5 keyed compare was not equal\n");
    }

    MAC_LEN = 20;
    memset(MAC, 0, MAC_LEN);
    generate_Ku(usmHMACSHA1AuthProtocol,
                OID_LENGTH(usmHMACSHA1AuthProtocol),
                buf, sizeof(buf)-1,
                MAC, &MAC_LEN);
    print_hash("sha1 Ku", MAC, MAC_LEN);

    MAC_LEN = 16;
    memset(MAC, 0, MAC_LEN);
    generate_Ku(usmHMACMD5AuthProtocol,
                OID_LENGTH(usmHMACMD5AuthProtocol),
                buf, sizeof(buf)-1,
                MAC, &MAC_LEN);
    print_hash("md5 Ku", MAC, MAC_LEN);

    /* XXX: todo: compare results and ensure they're always the same
       values; the algorithms aren't time-dependent. */
    return (0);
}

void
print_hash(const char *label, u_char *MAC, size_t MAC_LEN) {
    int i;
    printf("# %-10s %" NETSNMP_PRIz "u:\n", label, MAC_LEN);
    for(i=0; i < MAC_LEN; i++) {
        printf("# %02x ", MAC[i]);
    }
    printf("\n");
}    
