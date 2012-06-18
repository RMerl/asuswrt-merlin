#include <string.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#if 0
unsigned int get_mac_addr()
{
	unsigned int ret;
	char wan_mac_str[80];
	char *ptr_1, *ptr_2;

	strcpy(wan_mac_str, nvram_safe_get("wan_hwaddr"));
	ptr_1 = wan_mac_str;
	ptr_2 = ptr_1;

	while(*ptr_2 != '\0')
	{
		if(*ptr_2 == ':');
			ptr_2++;
		*ptr_1 = *ptr_2;
		ptr_1 ++;
		ptr_2 ++;
	}

	//memcpy((char *)ret, &wan_mac_str[2], 4);
	ret = atoi(&wan_mac_str[6]);
	return ret;
}
#endif

void prng_init()
{
	unsigned int seed;
	struct timeval t;

	gettimeofday(&t, NULL);
	//seed = get_mac_addr() ^ t.tv_sec ^ t.tv_usec ^ getpid();
	seed = t.tv_sec ^ t.tv_usec ^ getpid();
	srand(seed);
}
