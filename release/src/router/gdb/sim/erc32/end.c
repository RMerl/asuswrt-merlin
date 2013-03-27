#include <stdio.h>

int
main()
{

    unsigned int    u1;
    char           *c;
    double          d1;
    float          *f1;

    c = (char *) &u1;
    u1 = 0x0F;
    if (c[0] == 0x0F)
	puts("#define HOST_LITTLE_ENDIAN\n");
    else
	puts("#define HOST_BIG_ENDIAN\n");

    d1 = 1.0;
    f1 = (float *) &d1;
    if (*((int *) f1) != 0x3ff00000)
	puts("#define HOST_LITTLE_ENDIAN_FLOAT\n");
    else
	puts("#define HOST_BIG_ENDIAN_FLOAT\n");
    return 0;
}
