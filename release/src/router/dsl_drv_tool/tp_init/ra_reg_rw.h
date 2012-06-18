
int ra3052_reg_write(int offset, int value);
int switch_init(void);
void switch_fini(void);
int switch_rcv_okt(unsigned char* prcv_buf, unsigned short max_rcv_buf_size, unsigned short* prcv_buf_len);
#define MAX_TC_RESP_BUF_LEN 1000
