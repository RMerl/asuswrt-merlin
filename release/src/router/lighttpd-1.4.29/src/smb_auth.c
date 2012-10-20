#include "base.h"
#include "smb.h"
#include "libsmbclient.h"
#include "log.h"
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlinklist.h>
#include <signal.h>

#ifdef USE_OPENSSL
#include <openssl/md5.h>
#else
#include "md5.h"

#if EMBEDDED_EANBLE
#include "disk_share.h"
#endif

typedef li_MD5_CTX MD5_CTX;
#define MD5_Init li_MD5_Init
#define MD5_Update li_MD5_Update
#define MD5_Final li_MD5_Final
#endif

#define DBE 0

const char base64_pad = '=';

/* "A-Z a-z 0-9 + /" maps to 0-63 */
const short base64_reverse_table[256] = {
/*	 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0x00 - 0x0F */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0x10 - 0x1F */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, /* 0x20 - 0x2F */
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, /* 0x30 - 0x3F */
	-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, /* 0x40 - 0x4F */
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, /* 0x50 - 0x5F */
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, /* 0x60 - 0x6F */
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, /* 0x70 - 0x7F */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0x80 - 0x8F */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0x90 - 0x9F */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0xA0 - 0xAF */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0xB0 - 0xBF */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0xC0 - 0xCF */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0xD0 - 0xDF */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0xE0 - 0xEF */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0xF0 - 0xFF */
};


unsigned char * base64_decode(buffer *out, const char *in) {
	unsigned char *result;
	int ch, j = 0, k;
	size_t i;

	size_t in_len = strlen(in);
	
	buffer_prepare_copy(out, in_len);

	result = (unsigned char *)out->ptr;

	ch = in[0];
	/* run through the whole string, converting as we go */
	for (i = 0; i < in_len; i++) {
		ch = in[i];

		if (ch == '\0') break;

		if (ch == base64_pad) break;

		ch = base64_reverse_table[ch];
		if (ch < 0) continue;

		switch(i % 4) {
		case 0:
			result[j] = ch << 2;
			break;
		case 1:
			result[j++] |= ch >> 4;
			result[j] = (ch & 0x0f) << 4;
			break;
		case 2:
			result[j++] |= ch >>2;
			result[j] = (ch & 0x03) << 6;
			break;
		case 3:
			result[j++] |= ch;
			break;
		}
	}
	k = j;
	/* mop things up if we ended on a boundary */
	if (ch == base64_pad) {
		switch(i % 4) {
		case 0:
		case 1:
			return NULL;
		case 2:
			k++;
		case 3:
			result[k++] = 0;
		}
	}
	result[k] = '\0';

	out->used = k;

	return result;
}

char* get_mac_address(const char* iface, char* mac){
	int fd;
    	struct ifreq ifr;
    	fd = socket(AF_INET, SOCK_DGRAM, 0);
	ifr.ifr_addr.sa_family = AF_INET;	
    	strncpy(ifr.ifr_name, iface, IFNAMSIZ-1);		
    	ioctl(fd, SIOCGIFHWADDR, &ifr);
	close(fd);
    	sprintf(mac,"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
       	(unsigned char)ifr.ifr_hwaddr.sa_data[0],
         	(unsigned char)ifr.ifr_hwaddr.sa_data[1],
         	(unsigned char)ifr.ifr_hwaddr.sa_data[2],
         	(unsigned char)ifr.ifr_hwaddr.sa_data[3],
         	(unsigned char)ifr.ifr_hwaddr.sa_data[4],
         	(unsigned char)ifr.ifr_hwaddr.sa_data[5]);
	return mac;
}

char *replace_str(char *st, char *orig, char *repl, char* buff) {  
	char *ch;
	if (!(ch = strstr(st, orig)))
		return st;
	strncpy(buff, st, ch-st);  
	buff[ch-st] = 0;
	sprintf(buff+(ch-st), "%s%s", repl, ch+strlen(orig));  

	return buff;
}

void  getSubStr( char *str, char *substr, int start, int end )  
{  
	int substrlen = end - start + 1;
    	start = startposizition( str, start );  
    	end   = endposizition( str, end );  
	getStr( str, substr, substrlen, start, end );     
}  
  
void getStr( char *str, char *substr, int substrlen, int start, int end )  
{  
	int  i=0;	
	char* temp;
	temp = (char*)malloc(substrlen);
	memset(temp,0,sizeof(temp));  

	for( start; start<=end; start++ )  
    	{  
       	temp[i]=str[start];  
        	i++;  
    	}  
    	temp[i]='\0';  
	strcpy(substr,temp);  

	free(temp); 
}  

//- 判斷末端取值位置
int endposizition( char *str, int end )  
{  
	int i=0;            //-用於計數
	int posizition=0;   //- 返回位置
	int tempposi=end; 
	while(str[tempposi]<0)  
	{ 
		i++;  
	       tempposi--;  
	}  
 
	if(i%2==0 && i!=0)  
		posizition=end;   
	else  
	       posizition=end-1;     

 	return posizition;  
} 

//- 判斷開始取值位置 
int startposizition( char *str, int start )  
{  
    	int i=0;            //-用於計數
    	int posizition=0;   //- 返回位置
    	int tempposi=start;    
    	while(str[tempposi]<0)  
    	{  
       	i++;  
        	tempposi--;  
    	}  
  
    	if(i%2==0 && i!=0)  
       	posizition=start+1;   
    	else   
           	posizition=start;     
    	return posizition;  
} 

const char *get_filename_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

int smbc_wrapper_opendir(connection* con, const char *durl)
{
	if(con->mode== SMB_BASIC){		
		return smbc_opendir(durl);
	}
	else if(con->mode == SMB_NTLM){
		return smbc_cli_opendir(con->smb_info->cli, durl);
	}
	
	return -1;
}

int smbc_wrapper_opensharedir(connection* con, const char *durl)
{
	if(con->mode== SMB_BASIC){
		return smbc_opendir(durl);
	}
	else if(con->mode == SMB_NTLM){
		char turi[512];
		sprintf(turi, "%s\\IPC$", durl);
		return smbc_cli_open_share(con->smb_info->cli, turi);
	}
		
	return -1;
}

struct smbc_dirent* smbc_wrapper_readdir(connection* con, unsigned int dh)
{
	if(con->mode== SMB_BASIC)
		return smbc_readdir(dh);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_readdir(dh);

	return NULL;
}

int smbc_wrapper_closedir(connection* con, int dh)
{
	if(con->mode== SMB_BASIC)
		return smbc_closedir(dh);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_closedir(dh);

	return -1;
}

int smbc_wrapper_stat(connection* con, const char *url, struct stat *st)
{
	if(con->mode== SMB_BASIC){
		return smbc_stat(url, st);
	}
	else if(con->mode == SMB_NTLM)
		return smbc_cli_stat(con->smb_info->cli, url, st);

	return -1;
	
}

int smbc_wrapper_unlink(connection* con, const char *furl)
{
	if(con->mode== SMB_BASIC)
		return smbc_unlink(furl);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_unlink(con->smb_info->cli, furl, 0);

	return -1;
}

uint32_t smbc_wrapper_rmdir(connection* con, const char *dname)
{
	if(con->mode== SMB_BASIC)
		return smbc_rmdir(dname);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_rmdir(con->smb_info->cli, dname);

	return 0;
}

uint32_t smbc_wrapper_mkdir(connection* con, const char *fname, mode_t mode)
{
	if(con->mode== SMB_BASIC)
		return smbc_mkdir(fname, mode);
	else if(con->mode == SMB_NTLM){
		smb_file_t *fp;
		
		if((fp = smbc_cli_ntcreate(con->smb_info->cli, fname,
						FILE_READ_DATA | FILE_WRITE_DATA, FILE_CREATE, FILE_DIRECTORY_FILE)) == NULL)
		{
			return -1;
		}
		else{
			smbc_cli_close(con->smb_info->cli, fp);
		}
	}
	
	return 0;
}

uint32_t smbc_wrapper_rename(connection* con, char *src, char *dst)
{
	if(con->mode== SMB_BASIC)
		return smbc_rename(src, dst);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_rename(con->smb_info->cli, src, dst);

	return 0;
}

int smbc_wrapper_open(connection* con, const char *furl, int flags, mode_t mode)
{
	if(con->mode== SMB_BASIC)
		return smbc_open(furl, flags, mode);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_open(con->smb_info->cli, furl, flags);

	return -1;
}

int smbc_wrapper_create(connection* con, const char *furl, int flags, mode_t mode)
{
	if(con->mode== SMB_BASIC)
		return smbc_creat(furl, mode);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_ntcreate(con->smb_info->cli, furl, flags, FILE_CREATE, mode);

	return -1;
}

int smbc_wrapper_close(connection* con, int fd)
{
	if(con->mode== SMB_BASIC)
		return smbc_close(fd);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_close(con->smb_info->cli, fd);
	
	return -1;
}

size_t smbc_wrapper_read(connection* con, int fd, void *buf, size_t bufsize)
{
	if(con->mode== SMB_BASIC)
		return smbc_read(fd, buf, bufsize);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_read(con->smb_info->cli, fd, buf, bufsize);
	
	return -1;
}

size_t smbc_wrapper_write(connection* con, int fd, const void *buf, size_t bufsize, uint16_t write_mode )
{
	if(con->mode== SMB_BASIC)
		return smbc_write(fd, buf, bufsize);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_write(con->smb_info->cli, fd, write_mode, buf, bufsize);
	
	return -1;
}

off_t
smbc_wrapper_lseek(connection* con, int fd, off_t offset, int whence)
{
   if(con->mode== SMB_BASIC)
		return smbc_lseek(fd, offset,whence);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_lseek(con->smb_info->cli, fd, offset,whence);
	
	return -1;
}
int smbc_wrapper_parse_path(connection* con, char *pWorkgroup, char *pServer, char *pShare, char *pPath){
	if(con->mode== SMB_BASIC||con->mode== SMB_NTLM){ 
		smbc_parse_path(con->physical.path->ptr, pWorkgroup, pServer, pShare, pPath);
		
		//- Jerry add: replace '\\' to '/'
		do{
			char buff[4096];
			strcpy( pPath, replace_str(&pPath[0],"\\","/", (char *)&buff[0]) );
		}while(strstr(pPath,"\\")!=NULL);
	}
	
	return 1;
}

int smbc_wrapper_parse_path2(connection* con, char *pWorkgroup, char *pServer, char *pShare, char *pPath){	 
	if(con->mode== SMB_BASIC||con->mode== SMB_NTLM){

		smbc_parse_path(con->physical_auth_url->ptr, pWorkgroup, pServer, pShare, pPath);

		int len = strlen(pPath)+1;
		
		char* buff = (char*)malloc(len);
		memset(buff, '\0', len);
		
		do{	
			strcpy( pPath, replace_str(&pPath[0],"\\","/", buff) );
		}while(strstr(pPath,"\\")!=NULL);
		
		free(buff);
	}

	return 1;
}

int smbc_parse_mnt_path(
	const char* physical_path,
	const char* mnt_path,
       int mnt_path_len,
       char** usbdisk_path,
       char** usbdisk_share_folder){

	char* aa = physical_path + mnt_path_len;
	//Cdbg(1, "physical_path = %s, aa=%s", physical_path ,aa);
	char* bb = strstr(aa, "/");
	char* cc = NULL;
	//Cdbg(1, "bb=%s", bb);
	int len = 0;
	int len2 = 0;
	if(bb) {
		len=bb - aa;
		//Cdbg(1, "bb=%s", bb);
		cc = strstr(bb+1, "/");
		//Cdbg(1, "cc=%s", cc);
		if(cc) len2 = cc - bb;
		else len2 = strlen(bb);
	}
	else 
		len = ( physical_path + strlen(physical_path) ) - aa;
	//Cdbg(1, "strlen(physical_path)=%d", strlen(physical_path));
	*usbdisk_path = (char*)malloc(len+mnt_path_len+1);	
	memset( *usbdisk_path, '\0', len+mnt_path_len+1);	
	strncpy( *usbdisk_path, physical_path, len+mnt_path_len);
	//Cdbg(1, "usbdisk_path=%s, len=%d, mnt_path_len=%d", *usbdisk_path, len, mnt_path_len);
	if(bb){
		*usbdisk_share_folder= (char*)malloc(len2+1);
		memset( *usbdisk_share_folder, '\0', len2+1);
		strncpy(*usbdisk_share_folder, bb+1, len2-1);
	}
	return 1;
}

void smbc_wrapper_response_401(server *srv, connection *con)
{
	data_string *ds = (data_string *)array_get_element(con->request.headers, "user-Agent");

	//- Browser response
	if( ds && (strstr( ds->value->ptr, "Mozilla" )||strstr( ds->value->ptr, "Opera" )) ){
		if(con->mode == SMB_BASIC||con->mode == DIRECT){
			Cdbg(DBE, "con->mode == SMB_BASIC -> return 401");
			con->http_status = 401;
			return;
		}
	}
	
	Cdbg(DBE, "smbc_wrapper_response_401 -> return 401");
	
	char str[50];

	UNUSED(srv);

	buffer* tmp_buf = buffer_init();	
	if(con->mode == SMB_BASIC){
		//sprintf(str, "Basic realm=\"%s\"", "smbdav");
		
		if(con->smb_info&&con->smb_info->server->used)
			sprintf(str, "Basic realm=\"smb://%s\"", con->smb_info->server->ptr);
		else
			sprintf(str, "Basic realm=\"%s\"", "webdav");
	}
	else if(con->mode == SMB_NTLM)
		sprintf(str, "NTLM");
	else
		sprintf(str, "Basic realm=\"%s\"", "webdav");
	
	buffer_copy_string(tmp_buf, str);
	
	response_header_insert(srv, con, CONST_STR_LEN("WWW-Authenticate"), CONST_BUF_LEN(tmp_buf));
	con->http_status = 401;
		
	buffer_free(tmp_buf);
}

void smbc_wrapper_response_realm_401(server *srv, connection *con)
{
/*
	if(con->mode == SMB_BASIC){
		if(con->smb_info&&con->smb_info->server->used){
			Cdbg(DBE, "sssssssss");
			con->http_status = 401;
		}
		return;
	}
	*/
	char str[50];

	UNUSED(srv);

	buffer* tmp_buf = buffer_init();	
	if(con->mode == SMB_BASIC){
		//sprintf(str, "Basic realm=\"%s\"", "smbdav");
		
		if(con->smb_info&&con->smb_info->server->used)
			sprintf(str, "Basic realm=\"smb://%s\"", con->smb_info->server->ptr);
		else
			sprintf(str, "Basic realm=\"%s\"", "webdav");
		
	}
	else if(con->mode == SMB_NTLM)
		sprintf(str, "NTLM");
	else
		sprintf(str, "Basic realm=\"%s\"", "webdav");
	
	buffer_copy_string(tmp_buf, str);
	
	response_header_insert(srv, con, CONST_STR_LEN("WWW-Authenticate"), CONST_BUF_LEN(tmp_buf));
	con->http_status = 401;
		
	buffer_free(tmp_buf);
}

void smbc_wrapper_response_404(server *srv, connection *con)
{
	char str[50];

	UNUSED(srv);
	/*
	buffer* tmp_buf = buffer_init();

	if(con->mode == SMB_BASIC)
		sprintf(str, "Basic realm=\"%s\"", "smbdav");
	else if(con->mode == SMB_NTLM)
		sprintf(str, "NTLM");
		
	buffer_copy_string(tmp_buf, str);
	response_header_insert(srv, con, CONST_STR_LEN("WWW-Authenticate"), CONST_BUF_LEN(tmp_buf));
	*/
	con->http_status = 404;
	//buffer_free(tmp_buf);
}

buffer* smbc_wrapper_physical_url_path(server *srv, connection *con)
{	
	if(con->mode==SMB_BASIC||con->mode==SMB_NTLM){
		return con->url.path;
	}
	return con->physical.path;	
}

int smbc_get_usbdisk_permission(const char* user_name, const char* usbdisk_rel_sub_path, const char* usbdisk_sub_share_folder)
{
	int permission = -1;
	
	#if EMBEDDED_EANBLE

	int st_samba_mode = nvram_get_st_samba_mode();
	
	if( (st_samba_mode==1) ||
	    (user_name!=NULL && strncmp(user_name, "guest", 5)==0))
		permission = 3;
	else{
		permission = get_permission( user_name,
								  usbdisk_rel_sub_path,
								  usbdisk_sub_share_folder,
								  "cifs");
	}
	Cdbg(DBE, "usbdisk_rel_sub_path=%s, usbdisk_sub_share_folder=%s, permission=%d, user_name=%s", 
				usbdisk_rel_sub_path, usbdisk_sub_share_folder, permission, user_name);				
	
	#endif
	
	return permission;
}
int hex2ten(const char* in_str )
{	
	int ret;

	if( in_str == (char*)'A' || in_str==(char*)'a' )
		ret = 10;
	else if( in_str==(char*)'B' || in_str==(char*)'b' )
		ret = 11;
	else if( in_str==(char*)'C' || in_str==(char*)'c' )
		ret = 12;
	else if( in_str==(char*)'D' || in_str==(char*)'d' )
		ret = 13;
	else if( in_str==(char*)'E' || in_str==(char*)'e' )
		ret = 14;
	else if( in_str==(char*)'F' || in_str==(char*)'f' )
		ret = 15;
	else
		ret = atoi( (const char *)&in_str );
	
	//Cdbg(1, "12121212, in_str=%c, ret=%d", in_str, ret);
	return ret;
}

void ten2bin(int ten, char** out)
{
	char _array[4]="\0";
	_array[0] = '0';    	
	_array[1] = '0';    	
	_array[2] = '0';    	
	_array[3] = '0'; 
	int _count = 3;

	while( ten > 0 ){
		//Cdbg(1, "_count=%d", _count);
		
		if( ( ten%2 ) == 0 )
			_array[_count] = '0';
		else
			_array[_count] = '1';
		_count--;	    
		ten = ten/2;
	}

	*out = (char*)malloc(4);
	memset(*out, '\0', 4);
	strcpy(*out, _array);
}

void substr(char *dest, const char* src, unsigned int start, unsigned int cnt)
{
   	strncpy(dest, src + start, cnt);
   	dest[cnt] = 0;
}

void hexstr2binstr(const char* in_str, char** out_str)
{
	char *tmp;
	buffer* ret = buffer_init();
	for (tmp = in_str; *tmp; ++tmp){
		
		int ten = hex2ten( (char*)(*tmp) );
				
		char* binStr;
		ten2bin(ten, &binStr);
		//Cdbg(1, "444, binStr=%s", binStr);
		
		buffer_append_string_len(ret, binStr, 4);

		free(binStr);
	}

	*out_str = (char*)malloc(ret->size+1);
	strcpy(*out_str, ret->ptr);
	buffer_free(ret);
	
}

char* ten2hex(int  _v )
{	
	char ret[4]="\0";		
	if( _v >=0 && _v <= 9 )		
		//ret = _v.toString();	
		sprintf(ret,"%d", _v);
	else if( _v == 10 )		
		sprintf(ret,"%s", "a");//ret = "a";	
	else if( _v == 11 )	
		sprintf(ret,"%s", "b");//ret = "b";	
	else if( _v == 12 )		
		sprintf(ret,"%s", "c");//ret = "c";	
	else if( _v == 13 )		
		sprintf(ret,"%s", "d");//ret = "d";	
	else if( _v == 14 )		
		sprintf(ret,"%s", "e");//ret = "e";	
	else if( _v == 15 )		
		sprintf(ret,"%s", "f");//ret = "f";		
	return ret;
}

void binstr2hexstr( const char* in_str, char** out_str )
{	
	buffer* ret = buffer_init();	
	if( 0 != strlen(in_str) % 4 )	{
		buffer_free(ret);
		return;	
	}				
	int ic = strlen(in_str) / 4;				
	for( int i = 0; i < ic; i++ )
	{		
		char str[4]="\0";
		substr(str, in_str, i*4, 4);

		int uc = bin2ten( str );

		buffer_append_string(ret, ten2hex(uc));
		
		//var substr = in_src.substr( i*4, 4 );				
		//ret += ten2hex( bin2ten( substr ) );	
	}		

	*out_str = (char*)malloc(ret->size+1);
	strcpy(*out_str, ret->ptr);
	buffer_free(ret);
}

void deinterleave( char* src, int charcnt, char** out )
{	
	int src_len = strlen(src);
	int ic = src_len / charcnt;	
	buffer* ret = buffer_init();

	for( int i = 0; i < charcnt; i++ )	
	{	
		for( int j = 0; j < ic; j++ )		
		{			
			int x = (j*charcnt) + i;
			
			char tmps[8]="\0";
			sprintf(tmps, "%c", *(src+x));

			//Cdbg(1, "tmps =  %s", tmps);
			buffer_append_string(ret, tmps);
		}	
		
	}

	*out = (char*)malloc(ret->size+1);
	strcpy(*out, ret->ptr);

	buffer_free(ret);
}

void interleave( char* src, int charcnt, char** out )
{	
	int src_len = strlen(src);
	int ic = src_len / charcnt;	
	buffer* ret = buffer_init();

	for( int i = 0; i < ic; i++ )	{		
		for( int j = 0; j < charcnt; j++ ) {			
			//ret += src.charAt( (j * ic) + i );

			int x = (j * ic) + i;
			
			char tmps[8]="\0";
			sprintf(tmps, "%c", *(src+x));

			//Cdbg(DBE, "tmps =  %s", tmps);
			buffer_append_string(ret, tmps);
		}	
	}
	
	*out = (char*)malloc(ret->size+1);
	strcpy(*out, ret->ptr);

	buffer_free(ret);
	//Cdbg(DBE, "end interleave");
	/*
	// example: input string "ABCDEFG", charcnt = 3 --> "ADGBE0CF0"	
	var ret = "";	
	var ic = Math.ceil( src.length / charcnt );	
	for( var i = 0; i < ic; i++ )	{		
		for( var j = 0; j < charcnt; j++ )		{			
			ret += src.charAt( (j * ic) + i );		
		}	
	}	
	return ret;
	*/
}

void str2hexstr(char* str, char* hex)
{
	const char* cHex = "0123456789ABCDEF";
	int i=0;
	for(int j =0; j < strlen(str); j++)
	{
		unsigned int a = (unsigned int) str[j];
		hex[i++] = cHex[(a & 0xf0) >> 4];
		hex[i++] = cHex[(a & 0x0f)];
	}
	hex = '\0';
}



int sumhexstr( const char* str )
{		
	int ret = 0;		
	int ic = strlen(str) / 4.0;
	for( int i = 0; i < ic; i++ )	
	{		
		char t[4]="\0";
		substr(t, str, i*4, 4);

		int j=0;
		int y = 0;
		char *tmp;	
		for (tmp = t; *tmp; ++tmp){
			int pow = 1;
			for( int k = 0; k < strlen(t) - j - 1; k++ )
				pow *=16;
			
			int x = hex2ten( *tmp ) * pow;
			y += x;
			j++;
		}
	
		ret += y;	
	}			
	
	return ret;
}

int getshiftamount( const char* seed_str, const char* orignaldostr )
{	
	char seed_hex_str[100]="\0";
	str2hexstr( seed_str, &seed_hex_str );
	
	//sum hexstring, every 4digit (e.g "495a5c" => 0x495a + 0x5c => 0x49b6 => 18870;	
	int sumhex = sumhexstr( seed_hex_str );		
	
	//limit shift value to lenght of seed	
	int shiftamount = sumhex % strlen(orignaldostr);
	//Cdbg(1, "sumhex=%d, getshiftamount=%d", sumhex, strlen(orignaldostr));
	//ensure there is a shiftamount;	
	if( shiftamount == 0 )		
		shiftamount = 15;	
	return shiftamount;
}

void strleftshift( const char* str, int shiftamount, char** out )
{	
	// e.g strleftshift("abcdef", 2) => "cdefab"
	char* aa = (char*)malloc(strlen(str)-shiftamount+1);
	memset(aa, "\0", strlen(str)-shiftamount+1);
	substr(aa, str, shiftamount, strlen(str)-shiftamount);
	
	char *bb = (char*)malloc(shiftamount+1);
	memset(bb, "\0", shiftamount+1);
	substr(bb, str, 0, shiftamount);

	*out = (char*)malloc(strlen(str)+1);
	memset(*out, "\0", strlen(str)+1);
	strcpy(*out , aa);
	strcat(*out, bb);

	free(aa);
	free(bb);
}

void strrightshift( const char* str, int shiftamount, char** out )
{	
	// e.g strrightshift("abcdef", 2) => "efabcd"
	int len = strlen(str);
	//Cdbg(DBE, "11 len=%d, shiftamount=%d", len, shiftamount);
	
	char* aa = (char*)malloc(shiftamount+1);
	memset(aa, "\0", shiftamount+1);
	
	char *bb = (char*)malloc(len-shiftamount+1);
	memset(bb, "\0", len-shiftamount+1);
	
	substr(aa, str, len-shiftamount, shiftamount);
	//Cdbg(DBE, "33 aa=%d -> %d", len-shiftamount, shiftamount);
		
	substr(bb, str, 0, len-shiftamount);
	//Cdbg(DBE, "44 bb=%d -> %d", 0, len-shiftamount);
	
	*out = (char*)malloc(len+1);	
	memset(*out, "\0", strlen(str)+1);
	
	strcpy(*out , aa);
	//Cdbg(DBE, "55 out=%s", *out);
	strcat(*out, bb);
	//Cdbg(DBE, "66 out=%s", *out);
	
	free(aa);	
	free(bb);
}

int bin2ten( const char* _v )
{	
	int ret = 0;
	for( int j = 0; j < strlen(_v); j++ )	{	
		int pow = 1;
		for( int k = 0; k < strlen(_v) - j - 1; k++ )
			pow *=2;

		char tmp[8]="\0";
		strncpy(tmp, _v+j, 1);

		ret += atoi(tmp)*pow;
	}	
	return ret;
}

void binstr2str( const char* inbinstr, char** out )
{	
	//前面可能要補0	
	if( 0 != strlen(inbinstr) % 8 )
	{		
		return;
	}
	
	int ic = strlen(inbinstr) / 8;	
	int k = 0;	

	*out = (char*)malloc(ic);
	memset(*out , '\0', ic);
	
	for( int i = 0; i < strlen(inbinstr); i+=8 )	
	{				
		//前四位元	
		char* substrupper = (char*)malloc(4);
		substr(substrupper, inbinstr, i, 4);
		int uc = bin2ten( substrupper );
		
		//後四位元
		char* substrlowwer = (char*)malloc(4);
		substr(substrlowwer, inbinstr,  i + 4, 4);
		int lc = bin2ten( substrlowwer );

		int v = (uc << 4) | lc;	
		
		char out_char[8];
		sprintf(out_char, "%c", v);
		
		strcat(*out, out_char);											
		k++;
	}

}

void dec2Bin(int dec, char** out) {
	
	char _array[8] = "00000000";
	int _count = 7;

	while( dec > 0 ){
		//Cdbg(1, "_count=%d", _count);
		
		if( ( dec%2 ) == 0 )
			_array[_count] = '0';
		else
			_array[_count] = '1';
		_count--;	    
		dec = dec/2;
	}

	*out = (char*)malloc(8);
	memset(*out, '\0', 8);
	strncpy(*out, _array, 8);
}

void str2binstr(const char* instr, char** out)
{	
	buffer* b = buffer_init();
	int last = strlen(instr);	
	for (int i = 0; i < last; i++) {

		char tmp[8]="\0";
		strncpy(tmp, instr + i, 1);
		
		int acsii_code = ((int)*tmp) & 0x000000ff;
		if (acsii_code < 128){
			char* binStr;
			dec2Bin(acsii_code, &binStr);

			//Cdbg(1, "%s -> %d %d, binStr=%s", tmp, *tmp, acsii_code, binStr);
			
			buffer_append_string_len(b, binStr, 8);
			//Cdbg(1, "b=%s", b->ptr);

			free(binStr);
		}
		
		
	}		

	//Cdbg(1, "11111111111111 %d %d %d", b->used, last*8, b->size+1);

	int len = last*8 + 1;
	*out = (char*)malloc(len);
	memset(*out, '\0', len);
	strcpy(*out, b->ptr);
	buffer_free(b);
}

char* x123_decode(const char* in_str, const char* key, char* out_str)
{
	Cdbg(DBE, "in_str=%s, key=%s", in_str, key);

	char* ibinstr;
	hexstr2binstr( in_str, &ibinstr );
	//Cdbg(DBE, "ibinstr=%s", ibinstr);

	char* binstr;
	deinterleave( ibinstr, 8, &binstr );
	//Cdbg(DBE, "deinterleave, binstr=%s", binstr);

	int shiftamount = getshiftamount( key, binstr );
	//Cdbg(DBE, "shiftamount %d %s", shiftamount, key);

	char* unshiftbinstr;
	strleftshift( binstr, shiftamount, &unshiftbinstr );
	//Cdbg(DBE, "unshiftbinstr %s", unshiftbinstr);

	binstr2str(unshiftbinstr, &out_str);
	Cdbg(DBE, "out_str %s", out_str);

	free(ibinstr);
	free(binstr);
	free(unshiftbinstr);
	
	return out_str;
}

char* x123_encode(const char* in_str, const char* key, char* out_str)
{
	//Cdbg(DBE, "in_str=%s, key=%s", in_str, key);

	char* ibinstr;
	str2binstr( in_str, &ibinstr );
	//Cdbg(DBE, "ibinstr = %s", ibinstr);

	int shiftamount = getshiftamount( key, ibinstr );
	//Cdbg(DBE, "shiftamount = %d", shiftamount);

	char* unshiftbinstr = NULL;
	strrightshift( ibinstr, shiftamount, &unshiftbinstr );
	//Cdbg(DBE, "unshiftbinstr = %s", unshiftbinstr);

	char* binstr;
	interleave( unshiftbinstr, 8, &binstr );
	//Cdbg(DBE, "binstr = %s", binstr);

	char* hexstr;
	binstr2hexstr(binstr, &hexstr);
	//Cdbg(DBE, "hexstr = %s", hexstr);

	int out_len = strlen(hexstr)+9;
	//Cdbg(DBE, "out_len = %d, %d", out_len, strlen(hexstr));
	
	out_str = (char*)malloc(out_len);
	memset(out_str , '\0', out_len);
	strcpy(out_str, "ASUSHARE");
	strcat(out_str, hexstr);

	free(unshiftbinstr);
	free(ibinstr);
	free(binstr);
	free(hexstr);
	
	return out_str;
}

void md5String(const char* input1, const char* input2, char** out)
{
	int i;

	buffer* b = buffer_init();
	unsigned char signature[16];
	MD5_CTX ctx;
	MD5_Init(&ctx);

	if(input1){
		//Cdbg(1, "input1 %s", input1);
		MD5_Update(&ctx, input1, strlen(input1));	
	}
	
	if(input2){
		//Cdbg(1, "input2 %s", input2);
		MD5_Update(&ctx, input2, strlen(input2));	
	}
	
	MD5_Final(signature, &ctx);
	char tempstring[2];

	for(i=0; i<16; i++){
		int x = signature[i]/16;
		sprintf(tempstring, "%x", x);
		buffer_append_string(b, tempstring);
		
		int y = signature[i]%16;
		sprintf(tempstring, "%x", y);
		buffer_append_string(b, tempstring);		
	}

	//Cdbg(1, "result %s", b->ptr);
	
	int len = b->used + 1;
	*out = (char*)malloc(len);
	memset(*out, '\0', len);
	strcpy(*out, b->ptr);
	buffer_free(b);
}

int smbc_parser_basic_authentication(server *srv, connection* con, char** username,  char** password){
	
	if (con->mode != SMB_BASIC&&con->mode != DIRECT) return 0;

	data_string* ds = (data_string *)array_get_element(con->request.headers, "Authorization");
	
	if(con->share_link_basic_auth->used){
		Cdbg(1, "Use for Sharelink, con->share_link_basic_auth=[%s]", con->share_link_basic_auth->ptr);
		ds = data_string_init();
		buffer_copy_string_buffer( ds->value, con->share_link_basic_auth );
		buffer_reset(con->share_link_basic_auth);

		con->is_share_link = 1;
	}
	else
		con->is_share_link = 0;
	
	if (ds != NULL) {
		char *http_authorization = NULL;
		http_authorization = ds->value->ptr;
		
		if(strncmp(http_authorization, "Basic ", 6) == 0) {
			buffer *basic_msg = buffer_init();
			buffer *user = buffer_init();
			buffer *pass = buffer_init();
					
			if (!base64_decode(basic_msg, &http_authorization[6])) {
				log_error_write(srv, __FILE__, __LINE__, "sb", "decodeing base64-string failed", basic_msg);
				buffer_free(basic_msg);
				free(user);
				free(pass);							
				return 0;
			}
			char *s, bmsg[1024] = {0};

			//fetech the username and password from credential
			memcpy(bmsg, basic_msg->ptr, basic_msg->used);
			s = strchr(bmsg, ':');
			bmsg[s-bmsg] = '\0';

			buffer_copy_string(user, bmsg);
			buffer_copy_string(pass, s+1);
			
			*username = (char*)malloc(user->used);
			strcpy(*username, user->ptr);

			*password = (char*)malloc(pass->used);
			strcpy(*password, pass->ptr);
			
			buffer_free(basic_msg);
			free(user);
			free(pass);

			Cdbg(DBE, "base64_decode=[%s][%s]", *username, *password);
			
			return 1;
		}
	}
	else
		Cdbg(DBE, "smbc_parser_basic_authentication: ds == NULL");
	
	return 0;
}

int smbc_aidisk_account_authentication(connection* con, const char *username, const char *password){

	char *nvram_acc_list;
	int st_samba_mode = 1;
	
	if (con->mode != SMB_BASIC&&con->mode != DIRECT) return -1;
#if EMBEDDED_EANBLE
	char *a = nvram_get_acc_list();
	if(a==NULL) return -1;
	int l = strlen(a);
	nvram_acc_list = (char*)malloc(l+1);
	strncpy(nvram_acc_list, a, l);
	nvram_acc_list[l] = '\0';

	st_samba_mode = nvram_get_st_samba_mode();
#else
	int i = 100;
	nvram_acc_list = (char*)malloc(100);
	strcpy(nvram_acc_list, "admin<admin>jerry<jerry");
#endif

	int account_right = -1;

	//- Share All, use guest account
	if(st_samba_mode==1){
		buffer_copy_string(con->aidisk_username, "guest");
		buffer_copy_string(con->aidisk_passwd, "");
		return 1;
	}
	
	char * pch;
	pch = strtok(nvram_acc_list, "<>");	

	while(pch!=NULL){
		char *name;
		char *pass;
		int len;
		
		//- User Name
		len = strlen(pch);
		name = (char*)malloc(len+1);
		strncpy(name, pch, len);
		name[len] = '\0';
				
		//- User Password
		pch = strtok(NULL,"<>");
		len = strlen(pch);
		pass = (char*)malloc(len+1);
		strncpy(pass, pch, len);
		pass[len] = '\0';
		
		if( strcmp(username, name) == 0 &&
		    strcmp(password, pass) == 0 ){
			
			buffer_copy_string(con->aidisk_username, username);
			buffer_copy_string(con->aidisk_passwd, password);
				
			account_right = 1;
				
			free(name);
			free(pass);
			
			break;
		}
		
		free(name);
		free(pass);

		pch = strtok(NULL,"<>");
	}
	
	free(nvram_acc_list);
		
	return account_right;
}

const char* g_temp_sharelink_file = "/tmp/sharelink";

void save_sharelink_list(){

#if EMBEDDED_EANBLE

	share_link_info_t* c;
	
	buffer* sharelink_list = buffer_init();
	buffer_copy_string(sharelink_list, "");

	for (c = share_link_info_list; c; c = c->next) {

		if(c->toshare == 0)
			continue;
		
		buffer* temp = buffer_init();

		buffer_copy_string_buffer(temp, c->shortpath);
		buffer_append_string(temp, "<");
		buffer_append_string_buffer(temp, c->realpath);
		buffer_append_string(temp, "<");
		buffer_append_string_buffer(temp, c->filename);
		buffer_append_string(temp, "<");
		buffer_append_string_buffer(temp, c->auth);
		buffer_append_string(temp, "<");

		char strTime[25] = {0};
		sprintf(strTime, "%lu", c->expiretime);		
		buffer_append_string(temp, strTime);

		buffer_append_string(temp, "<");
		
		char strTime2[25] = {0};
		sprintf(strTime2, "%lu", c->createtime);		
		buffer_append_string(temp, strTime2);
		
		buffer_append_string(temp, ">");
			
		buffer_append_string_buffer(sharelink_list, temp);
		
		buffer_free(temp);
	}
	
	nvram_set_sharelink_str(sharelink_list->ptr);		
	buffer_free(sharelink_list);
#else
	unlink(g_temp_sharelink_file);

	smb_srv_info_t* c;
	
	char mybuffer[100];
	FILE* fp = fopen(g_temp_sharelink_file, "w");

	if(fp!=NULL){
		share_link_info_t* c;
	
		for (c = share_link_info_list; c; c = c->next) {
			if(c->toshare == 0)
				continue;
			
			fprintf(fp, "%s<%s<%s<%s<%lu<%lu\n", c->shortpath->ptr, c->realpath->ptr, c->filename->ptr, c->auth->ptr, c->expiretime, c->createtime);
		}
		
		fclose(fp);
	}
#endif
}

void free_share_link_info(share_link_info_t *smb_sharelink_info){
	buffer_free(smb_sharelink_info->shortpath);
	buffer_free(smb_sharelink_info->realpath);
	buffer_free(smb_sharelink_info->filename);
	buffer_free(smb_sharelink_info->auth);
}

void read_sharelink_list(){
#if EMBEDDED_EANBLE
	char* aa = nvram_get_sharelink_str();

	if(aa==NULL){
		return;
	}
	
	char* str_sharelink_list = (char*)malloc(strlen(aa)+1);
	strcpy(str_sharelink_list, aa);
		
	if(str_sharelink_list!=NULL){
		char * pch;
		pch = strtok(str_sharelink_list, "<>");
		
		while(pch!=NULL){

			int b_addto_list = 1;
			
			share_link_info_t *smb_sharelink_info;
			smb_sharelink_info = (share_link_info_t *)calloc(1, sizeof(share_link_info_t));
			smb_sharelink_info->toshare = 1;
			
			//- Share Path
			if(pch){
				Cdbg(DBE, "share path=%s", pch);
				smb_sharelink_info->shortpath = buffer_init();
				buffer_copy_string_len(smb_sharelink_info->shortpath, pch, strlen(pch));
			}
			
			//- Real Path
			pch = strtok(NULL,"<>");
			if(pch){
				Cdbg(DBE, "real path=%s", pch);
				smb_sharelink_info->realpath = buffer_init();
				buffer_copy_string_len(smb_sharelink_info->realpath, pch, strlen(pch));
			}
			
			//- File Name
			pch = strtok(NULL,"<>");
			if(pch){
				Cdbg(DBE, "file name=%s", pch);
				smb_sharelink_info->filename = buffer_init();
				buffer_copy_string_len(smb_sharelink_info->filename, pch, strlen(pch));
			}
			
			//- Auth
			pch = strtok(NULL,"<>");
			if(pch){
				Cdbg(DBE, "auth=%s", pch);
				smb_sharelink_info->auth = buffer_init();
				buffer_copy_string_len(smb_sharelink_info->auth, pch, strlen(pch));
			}

			//- Expire Time
			pch = strtok(NULL,"<>");
			if(pch){				
				smb_sharelink_info->expiretime = atoi(pch);	
				time_t cur_time = time(NULL);				
				double offset = difftime(smb_sharelink_info->expiretime, cur_time);					
				if( smb_sharelink_info->expiretime !=0 && offset < 0.0 ){
					free_share_link_info(smb_sharelink_info);
					free(smb_sharelink_info);
					b_addto_list = 0;
				}
			}
			
			//- Create Time
			pch = strtok(NULL,"<>");
			if(pch){				
				smb_sharelink_info->createtime = atoi(pch);
			}
			
			if(b_addto_list==1)
				DLIST_ADD(share_link_info_list, smb_sharelink_info);
			
			//- Next
			pch = strtok(NULL,"<>");
		}
	}

#else
	size_t j;
	int length, filesize;
	FILE* fp = fopen(g_temp_sharelink_file, "r");
	if(fp!=NULL){		
		
		char str[1024];

		while(fgets(str,sizeof(str),fp) != NULL)
		{
			char * pch;
			int b_addto_list = 1;
			
      			// strip trailing '\n' if it exists
      			int len = strlen(str)-1;
      			if(str[len] == '\n') 
         			str[len] = 0;

			share_link_info_t *smb_sharelink_info;
			smb_sharelink_info = (share_link_info_t *)calloc(1, sizeof(share_link_info_t));
			smb_sharelink_info->toshare = 1;
			
			//- Share Path
			pch = strtok(str,"<");
			if(pch){
				smb_sharelink_info->shortpath = buffer_init();			
				buffer_copy_string_len(smb_sharelink_info->shortpath, pch, strlen(pch));
			}
			
			//- Real Path
			pch = strtok(NULL,"<");
			if(pch){
				smb_sharelink_info->realpath = buffer_init();			
				buffer_copy_string_len(smb_sharelink_info->realpath, pch, strlen(pch));
			}
			
			//- File Name
			pch = strtok(NULL,"<");
			if(pch){
				smb_sharelink_info->filename = buffer_init();			
				buffer_copy_string_len(smb_sharelink_info->filename, pch, strlen(pch));
			}

			//- Auth
			pch = strtok(NULL,"<");
			if(pch){
				smb_sharelink_info->auth = buffer_init();			
				buffer_copy_string_len(smb_sharelink_info->auth, pch, strlen(pch));
			}

			//- Expire Time
			pch = strtok(NULL,"<");
			if(pch){
				
				smb_sharelink_info->expiretime = atoi(pch);	
				time_t cur_time = time(NULL);				
				double offset = difftime(smb_sharelink_info->expiretime, cur_time);					
				if( smb_sharelink_info->expiretime !=0 && offset < 0.0 ){
					free_share_link_info(smb_sharelink_info);
					free(smb_sharelink_info);
					b_addto_list = 0;
				}
			}

			//- Create Time
			pch = strtok(NULL,"<");
			if(pch){
				smb_sharelink_info->createtime = atoi(pch);
			}
			
			if(b_addto_list==1)
				DLIST_ADD(share_link_info_list, smb_sharelink_info);
		}
			
		fclose(fp);
	}
#endif
}

void start_arpping_process(const char* scan_interface)
{
#if EMBEDDED_EANBLE
	if(pids("lighttpd-arpping"))
		return;
#else
	if(system("pidof lighttpd-arpping") == 0)
		return;
#endif

	char cmd[BUFSIZ]="\0";
	/*
	char mybuffer[BUFSIZ]="\0";
	FILE* fp = popen("pwd", "r");
	if(fp){
		int len = fread(mybuffer, sizeof(char), BUFSIZ, fp);
		mybuffer[len-1]="\0";
		pclose(fp);

		sprintf(cmd, ".%s/_inst/sbin/lighttpd-arpping -f %s &", mybuffer, scan_interface);
		Cdbg(DBE, "%s, len=%d", cmd, len);
		system(cmd);
	}
	*/	
#if EMBEDDED_EANBLE
	sprintf(cmd, "lighttpd-arpping -f %s &", scan_interface);
#else
	sprintf(cmd, "./_inst/sbin/lighttpd-arpping -f %s &", scan_interface);
#endif
	system(cmd);
}

void stop_arpping_process()
{
	/*
	char mybuffer[BUFSIZ]="\0";
	char cmd[BUFSIZ]="\0";
	int chars_read;
	FILE* read_fp = popen("pidof lighttpd-arpping", "r");
	if(read_fp!=NULL){
		chars_read = fread(mybuffer, sizeof(char), BUFSIZ, read_fp);
		if(chars_read>0){
			sprintf(cmd, "kill -9 %s", mybuffer);
			Cdbg(DBE, "%s", cmd);
			system(cmd);
		}	
		pclose(read_fp);
	}
	*/
	
	FILE *fp;
    	char buf[256];
    	pid_t pid = 0;
    	int n;

	if ((fp = fopen("/tmp/lighttpd/lighttpd-arpping.pid", "r")) != NULL) {
		if (fgets(buf, sizeof(buf), fp) != NULL)
	            pid = strtoul(buf, NULL, 0);
	        fclose(fp);
	}
	
	if (pid > 1 && kill(pid, SIGTERM) == 0) {
		n = 10;	       
	       while ((kill(pid, SIGTERM) == 0) && (n-- > 0)) {
	            Cdbg(DBE,"Mod_smbdav: %s: waiting pid=%d n=%d\n", __FUNCTION__, pid, n);
	            usleep(100 * 1000);
	        }
	}

	unlink("/tmp/lighttpd/lighttpd-arpping.pid");
}

int in_the_same_folder(buffer *src, buffer *dst) 
{
	char *sp;

	char *smem = (char *)malloc(src->used);
	memcpy(smem, src->ptr, src->used);
		
	char *dmem = (char *)malloc(dst->used);	
	memcpy(dmem, dst->ptr, dst->used);
		
	sp = strrchr(smem, '/');
	int slen = sp - smem;
	sp = strrchr(dmem, '/');
	int dlen = sp - dmem;

	smem[slen] = '\0';
	dmem[dlen] = '\0';
	
	int res = memcmp(smem, dmem, (slen>dlen) ? slen : dlen);		
	Cdbg(DBE, "smem=%s,dmem=%s, slen=%d, dlen=%d", smem, dmem, slen, dlen);
	free(smem);
	free(dmem);

	return (res) ? 0 : 1;
}

#if 0
int smbc_host_account_authentication(connection* con, const char *username, const char *password){

	char *nvram_acc_list, *nvram_acc_webdavproxy;	
	int st_samba_mode = 1;
	if (con->mode != SMB_BASIC&&con->mode != DIRECT) return -1;
#if EMBEDDED_EANBLE
	char *a = nvram_get_acc_list();
	if(a==NULL) return -1;
	int l = strlen(a);
	nvram_acc_list = (char*)malloc(l+1);
	strncpy(nvram_acc_list, a, l);
	nvram_acc_list[l] = '\0';

	char *b = nvram_get_acc_webdavproxy();
	if(b==NULL) return -1;
	l = strlen(b);
	nvram_acc_webdavproxy = (char*)malloc(l+1);
	strncpy(nvram_acc_webdavproxy, b, l);
	nvram_acc_webdavproxy[l] = '\0';

	st_samba_mode = nvram_get_st_samba_mode();
#else
	int i = 100;
	nvram_acc_list = (char*)malloc(100);
	strcpy(nvram_acc_list, "admin<admin>jerry<jerry");

	nvram_acc_webdavproxy = (char*)malloc(100);
	strcpy(nvram_acc_webdavproxy, "admin<1>jerry<0");
#endif

	int found_account = 0, account_right = -1;

	//- Share All, use guest account
	if(st_samba_mode==0){
		buffer_copy_string(con->router_user, "guest");
		account_right = 1;
		return account_right;
	}
		

	char * pch;
	pch = strtok(nvram_acc_list, "<>");	

	while(pch!=NULL){
		char *name;
		char *pass;
		int len;
		
		//- User Name
		len = strlen(pch);
		name = (char*)malloc(len+1);
		strncpy(name, pch, len);
		name[len] = '\0';
				
		//- User Password
		pch = strtok(NULL,"<>");
		len = strlen(pch);
		pass = (char*)malloc(len+1);
		strncpy(pass, pch, len);
		pass[len] = '\0';
		
		if( strcmp(username, name) == 0 &&
		    strcmp(password, pass) == 0 ){
			
			//Cdbg(1, "22222222222 name = %s, pass=%s, right=%d", username, password, right);
			
			int len2;
			char  *pch2, *name2;
			pch2 = strtok(nvram_acc_webdavproxy, "<>");	

			while(pch2!=NULL){
				//- User Name
				len2 = strlen(pch2);
				name2 = (char*)malloc(len2+1);
				strncpy(name2, pch2, len2);
				name2[len2] = '\0';

				//- User Right
				pch2 = strtok(NULL,"<>");
				account_right = atoi(pch2);
		
				if( strcmp(username, name2) == 0 ){
					free(name2);
					
					if(con->smb_info)
						con->smb_info->auth_right = account_right;
					
					//- Copy user name to router_user
					buffer_copy_string(con->router_user, username);

					found_account = 1;
					
					break;
				}

				free(name2);
				pch2 = strtok(NULL,"<>");
			}

			free(name);
			free(pass);
			
			if(found_account==1)
				break;
		}
		
		free(name);
		free(pass);

		pch = strtok(NULL,"<>");
	}
	
	free(nvram_acc_list);
	free(nvram_acc_webdavproxy);
	
	return account_right;
}
#endif

