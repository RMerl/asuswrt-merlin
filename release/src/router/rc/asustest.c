#include <rc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>



#if defined(RTCONFIG_DSL)

int asustest_command(const char *command, const char *value) {
	char buffer[512] = {0};

	if(!strcmp(command, "save")) {
		nvram_commit();
		printf("save...\n");
	}
	else if(!strcmp(command, "get_vdtxpwrtestmode")) {
		printf("%s\n", nvram_safe_get("dslx_vdtxpwrtestmode"));
	}
	else if(!strcmp(command, "set_vdtxpwrtestmode")) {
		if(!value)
		{
			printf("asustest:parameter error!\n");
			return 1;
		}
		else
		{
			snprintf(buffer, sizeof(buffer), "%s", value);
			nvram_set("dslx_vdtxpwrtestmode", buffer);
			printf("%s to %s\n", command, buffer);
		}
	}
	else {
		printf("The command: %s is wrong, please check again!!!\n", command);
	}
	return 1;
}

#endif /* RTCONFIG_DSL */

