#include "phsocket.h"
#include "phglobal.h"

#ifndef WIN32
#include "sys/ioctl.h"
#include "net/if_arp.h"
#include "net/if.h"
#include "netinet/in.h"
#endif

typedef struct
{
	BOOL bNewIni;
	BOOL bDaemon;
	BOOL bAppointIni;
	BOOL bUser;
	BOOL bFirstRun;
	int  nicNumber;

	char szconfig[255];
	char szuserName[255];
	char nicName[255];

	char logfile[1024];
}PH_parameter;

int checkparameter(int argc,char** argv,PHGlobal *pglobal,PH_parameter *parameter);
void HelpPrint();
int InitIni( PHGlobal *pglobal,PH_parameter *parameter );
void BindNic(  PHGlobal *pglobal,PH_parameter *parameter );
int ParameterAnalysis(int argc,char *argv[],PHGlobal *pglobal,PH_parameter *parameter);
int NewIni(char* save_file,PHGlobal *pglobal,PH_parameter *parameter);
void NewHost(PHGlobal *global);
void NewUserID(PHGlobal *global);
void NewUserPWD(PHGlobal *global);
void NewnicName(PHGlobal *pglobal,PH_parameter *parameter);
int SaveFile(char* save_file,PHGlobal *pglobal,PH_parameter *parameter);
int LoadFile(PHGlobal *global,PH_parameter *parameter);
int InDaemon(void);
void ShowNic( PHGlobal *pglobal,PH_parameter *parameter );
void SetValue(char* root,char* attribute,char* value,PHGlobal *pglobal,PH_parameter *parameter );
int MyWriteFile(char *filename,PHGlobal *pglobal,PH_parameter *parameter);
int MyReadFile(char* filename,PHGlobal *pglobal,PH_parameter *parameter);
char* trim(char* string);
int GetValue(char* root,char* attribute,char* value,PHGlobal *pglobal,PH_parameter *parameter);
int init_parameter(PH_parameter *parameter);
void NewFile(PH_parameter *parameter);