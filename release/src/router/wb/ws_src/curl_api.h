#ifndef __CURL_API_H__
#define __CURL_API_H__
// READ_CB use for READ DATA from memory to curl socket 
typedef size_t 		(*READ_CB)(void *ptr, size_t size, size_t nmemb, void *userp);
// WRITE_CB use for WRITE DATA from scocket to memory
typedef size_t 		(*WRITE_CB)	(char *in, size_t size, size_t nmemb, void* fn_struct);

struct input_info {
  const char *readptr;
  int sizeleft;
};
struct write_info{
   void* write_data;
   void* curlptr;
};
typedef struct _RWCB{
	READ_CB 	read_cb;
	WRITE_CB	write_cb;
	struct		input_info*		pInput;		
	void*		write_data;
//	struct		write_info*		pWriteInfo;
//	void*		write_data;	
//	void*		curlptr;
}RWCB, *PRWCB;

int	curl_io(const char*	web_path, const	char* custom_header[], PRWCB pRWCB);
/*
int	curl_io(const char*	web_path,
		const char* 	custom_header[],
		READ_CB		pReadCb,
		void*		pReadData,
		WRITE_CB	,
		void*		pWriteData
	);
*/
#endif
