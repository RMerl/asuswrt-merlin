#ifdef RTCONFIG_DSL
	#define DSL_N55U 1
#ifdef RTCONFIG_DSL_ANNEX_B
	#define DSL_N55U_ANNEX_B 1
#endif
#else
	#error "no DSL defined, please review makefile"
#endif


#define MAX_IPC_MSG_BUF 20

typedef struct
{
    long mtype;
    char mtext[MAX_IPC_MSG_BUF];
} msgbuf;

// linux IPC number need to greater than 1
enum {
	IPC_CLIENT_MSG_Q_ID = 1,
	IPC_START_AUTO_DET,
	IPC_LINK_STATE,
	IPC_ADD_PVC,
	//IPC_DEL_PVC,
	IPC_DEL_ALL_PVC,
	IPC_STOP_AUTO_DET,
	IPC_ATE_ADSL_SHOW,
	IPC_ATE_SET_ADSL_MODE,
	IPC_ATE_ADSL_DISP_PVC,
	IPC_ATE_ADSL_QUIT_DRV,
	IPC_RELOAD_PVC,
	IPC_ATE_ADSL_RESTORE_DEFAULT /* Paul add 2012/8/7 */
};

