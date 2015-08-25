#ifndef __M_QUEUE_H
#define __M_QUEUE_H
#if 0
#include <mqueue.h>
#include <errno.h>

#define QUEUE_NAME "/tunnel2"
#define MAX_SIZE 4096	
#define CHECK(x) \
   do{ \
		if(!(x)) { \
			fprintf(stderr, "%s:%d, errno =%d", __func__, __LINE__, errno); \
			perror(#x); \
			exit(-1); \
		} \
   } while(0)

int listen_msg(mqd_t* p_mq , void (*user_fn)(void* user_data) );
int push_msg(mqd_t* p_mq, void* user_data, size_t data_size);
#endif

int RemoveWatchQueue();
int InitWatchQueue(void* (*ProcessQueue)(void* user_data), 
			const char* queue_sem,
 			const char* data_sem);
int PostDataToQueue(void* data, size_t data_size);

#endif
