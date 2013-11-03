#include <openssl/md5.h>
#include <unistd.h>
#include <ssl_api.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void get_md5_string(char* hwaddr, char* out_md5string)
{
	//unsigned char* hwaddr = "00:0c:29:62:72:68";	
	unsigned char md[MD_LEN]={0};
	int i =0;
	MD5((const unsigned char*)hwaddr, strlen((const char*)hwaddr), md);
	
	//char md5string[MD_STR_LEN+1]={0};
	//char* mmd5string = malloc(MD_STR_LEN+1);
	memset(out_md5string, 0 , MD_STR_LEN+1);

	for(i =0; i<MD_LEN; i++){
		char tmp[3]={0};
		sprintf(tmp,"%02x", md[i]);	
		strcat(out_md5string, tmp);				
	}
	printf(">>>>>md5stirng =%s", out_md5string);	
}
