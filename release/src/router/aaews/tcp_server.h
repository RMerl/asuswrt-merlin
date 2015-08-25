#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__
typedef struct _CLINET_INFO{
	int		client_fd;
	int		srv_fd;
	int		read_data_size;
	char*	read_data;
	int		write_data_size;
	char*	write_data;
}CLIENT_INFO;

typedef int (*CB_RECV)(char* recv_data, int rec_size);
typedef int (*CB_SEND)(int send_fd);
//typedef int (*CB_SEND)(char* send_data, void* user_data);

typedef struct _CB_INFO
{
	CB_RECV		rec_func;
	CB_SEND		send_func;
}CB_INFO;

int start_tcp_server(CB_RECV cb_recv, CB_SEND cb_send);
#endif
