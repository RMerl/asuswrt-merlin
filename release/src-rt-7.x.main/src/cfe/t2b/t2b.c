#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

/* convert console-log.txt(from flash -ndump) to bin	*
 * usage ./t2b [log.txt]				*/

int
AtoH(char *arg)
{
        int i, len = strlen(arg), ret=0;
        char *p, tmpstr[32];

        for(i=0; i<len; ++i)
        {
                p = &arg[i];
                if((*p>='0') && (*p<='9'))
                        *p = *p - '0';
                else if((*p>='a') && (*p<='f'))
                        *p = *p - 'a' + 0xA;
                else if((*p>='A') && (*p<='F'))
                        *p = *p - 'A' + 0xA;
        }
        return (arg[0] << 4 | arg[1]);
}

int
main(int argc, char *argv[])
{
	char dst_file[20], line[100], *p, xb[2];
	FILE *fp_s, *fp_d;
	unsigned char x, x6[16];
	int i=0;
	int j=0;

	if(!argv[1]){
		printf("no src file\n");
		return -1;
	} else{
		sprintf(dst_file, "%s_v.bin", argv[1]);
	}

	if(((fp_s = fopen(argv[1], "r")) == NULL) || ((fp_d = fopen(dst_file, "w+")) == NULL)){
		printf("open src/dst fail\n");
		return -1;
	}

	while(fgets(line, sizeof(line), fp_s)!=(char*)0){
		if(line[8]==':' && line[9]==' '){
			p = line;
			i=0;
			memset(x6, 0, sizeof(x6));
			while(p){
				p = strtok(p, "\r\n ");
				if(p==NULL)
					break;
				if(strlen(p) > 3){
					p+=strlen(p)+1;
					continue;
				}
				xb[0] = *p;
				xb[1] = *(p+1);
				x = AtoH(xb);
				x6[i++] = x;
				p+=strlen(p)+1;
			}
			/*
			for(i=0; i<16; ++i)
				printf("%2x ", x6[i]);
			printf(" (%d)\n", strlen(x6));
			if(j++ > 5)
				break;
			*/
			if(*x6)
				fwrite(x6, 1, sizeof(x6), fp_d);
		}

	}

	fclose(fp_s);
	fclose(fp_d);
	return 0;
}
