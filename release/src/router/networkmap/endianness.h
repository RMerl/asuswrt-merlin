#if 1
/* macro to swap the bytes in a 16-bit variable */
#define swapbytes16(x) \
{ \
    unsigned short int data = *(unsigned short int*)&(x); \
    data = ((data & 0xff00) >> 8) |    \
           ((data & 0x00ff) << 8);     \
    *(unsigned short int*)&(x) = data;       \
}
/* macro to swap the bytes in a 32-bit variable */
#define swapbytes32(x) \
{ \
    unsigned int data = *(unsigned int*)&(x); \
    data = ((data & 0xff000000) >> 24) |    \
           ((data & 0x00ff0000) >>  8) |    \
           ((data & 0x0000ff00) <<  8) |    \
           ((data & 0x000000ff) << 24);     \
    *(unsigned int*)&(x) = data;            \
}
#else
#define endian_translate_s(A) ((((A) & 0xff00) >> 8) | \
                              ((A) & 0x00ff) << 8))
#define endian_translate_l(A) ((((A) & 0xff000000) >> 24) | \
                              (((A) & 0x00ff0000) >> 8) | \
                              (((A) & 0x0000ff00) << 8) | \
                              (((A) & 0x000000ff) << 24))
#endif

