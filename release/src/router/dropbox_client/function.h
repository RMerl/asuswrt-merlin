//#include "api.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
/*basic function */
#define getb(type) (type*)malloc(sizeof(type))
#define NORMALSIZE 512
#define my_free(x)  free(x);x=NULL;



extern char *parse_name_from_path(const char *path);
extern int is_server_exist();
extern char *serverpath_to_localpath(char *from_serverpath,int index);
extern int test_if_dir(const char *dir);
//extern int add_action_item(const char *action,const char *path,action_item *head);

char *oauth_encode_base64(int size, const unsigned char *src);
int oauth_decode_base64(unsigned char *dest, const char *src);
char *oauth_url_escape(const char *string);
char *oauth_url_unescape(const char *string, size_t *olen);
char *oauth_sign_hmac_sha1 (const char *m, const char *k);
char *oauth_gen_nonce();

char* MD5_string(char *string);


char *oauth_sign_plaintext (const char *m, const char *k);
char *my_str_malloc(size_t len);
void my_mkdir_r(char *path);
void my_mkdir(char *path);
//int create_sync_list();
int test_if_dir_empty(char *dir);
char *my_nstrchr(const char chr,char *str,int n);
char *strlwr(char *s);
char *get_confilicted_name(const char *fullname,int isfolder);
int is_number(char *str);
void deal_big_low_conflcit(char *action,char *oldname,char *newname,char *newname_r,int index);
char *get_confilicted_name_first(const char *fullname,int isfolder);
char *get_confilicted_name_second(const char *fullname,int isfolder,char *prefix_name);
char *get_prefix_name(const char *fullname,int isfolder);
char *get_confilicted_name_case(const char *fullname,int is_folder);
