#include "phruncall.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef WIN32
#include <io.h>
#else
#include "termios.h"
#include <unistd.h>
#endif

#include "log.h"
#include "phglobal.h"

#define PHDDNS_REVISION	"$Revision: 32841 $$Date: 2014-04-09 10:14:34 +0800 $$Author: skyvense $"
#define ISSPACE(x) ((x)==' '||(x)=='\r'||(x)=='\n'||(x)=='\f'||(x)=='\b'||(x)=='\t')

int checkparameter(int argc,char** argv,PHGlobal *pglobal,PH_parameter *parameter)
{
	int i = 0,repma = 0;

#ifndef WIN32
	strcpy(parameter->szconfig,"/etc/phlinux.conf");
#else
	strcpy(parameter->szconfig,"d:\phlinux.ini");
#endif
	for( i=1;i<argc;i++ ) {
		if(strcmp(argv[i],"-i")==0||strcmp(argv[i],"--interact")==0) {
/*			if(parameter->bAppointIni) {
				if(parameter->bAppointIni)
				{
					HelpPrint();
					return -1;
				}
				parameter->bNewIni=1;
			}*/
			parameter->bNewIni=1;
		}
		else if(strcmp(argv[i],"-d")==0||strcmp(argv[i],"--daemon")==0)
		{
			parameter->bDaemon=1;
		}
		else if(strcmp(argv[i],"-c")==0||strcmp(argv[i],"--config")==0)
		{
			parameter->bAppointIni=1;
			if(++i<argc) {
				if(parameter->bNewIni || strchr(argv[i],'-')!=NULL) {
					HelpPrint();
					return -1;
				}
				memset(parameter->szconfig,0,sizeof(parameter->szconfig));
				strcpy(parameter->szconfig,argv[i]);
			}
			else
			{
				printf("Please appoint configuration file!\n");
				return -1;
			}
		}
		else if(strcmp(argv[i],"-u")==0||strcmp(argv[i],"--user")==0)
		{
			parameter->bUser=1;
			if(++i<argc)
			{
				if(strchr(argv[i],'-')!=NULL)
				{
					HelpPrint();
					return -1;
				}
				strcpy(parameter->szuserName,argv[i]);
			}
			else
			{
				printf("Please appoint user name!\n");
				return -1;
			}
		}
		else if(strcmp(argv[i],"-f")==0||strcmp(argv[i],"--first-run")==0)
		{
			parameter->bFirstRun=1;
		}
		else
		{
			HelpPrint();
			return -1;
		}
	}
	if ( i == 1)
	{
		InitIni(pglobal,parameter);
		BindNic(pglobal,parameter);
	}

	repma = ParameterAnalysis(argc,argv,pglobal,parameter);
	if( repma !=0 )
		return -1;

	return 0;
}

void HelpPrint()
{
	printf("Peanuthull Linux, Copyright (c) 2006-2014\n");;
	printf("%s\n",PHDDNS_REVISION);
	printf("--first-run\n");
	printf("\t-f, run for the first time\n");
	printf("--interact\n");
	printf("\t-i, run as interactive mode\n");
	printf("\t\tprogram will request for necessary parameters.\n");
	printf("\t\tthis mode will automatically enabled at first running,\n");
	printf("\t\tor your configuration file has been lost.\n");
	printf("--daemon\n");
	printf("\t-d, run as a daemon\n");
	printf("\t\tprogram will quit after put itself to background,\n");
	printf("\t\tand continue running even you logout,\n");
	printf("\t\tyou can use kill -9 <PID> to terminate.\n");
	printf("--config\n");
	printf("\t-c, run with configuration file\n");
	printf("\t\tprogram will run with the file\n");
	printf("--user\n");
	printf("\t-u, run as the user \n");
	printf("\t\tprogram will run as the user\n");
	printf("--help\n");
	printf("\t-h, print this screen.\n");
	printf("Please visit http://www.oray.com for detail.\n");
}

int InitIni( PHGlobal *pglobal,PH_parameter *parameter )
{
#ifndef WIN32
	memset(parameter->szconfig,0,sizeof(parameter->szconfig));
	strcpy(parameter->szconfig,"/etc/phlinux.conf");
	if( LoadFile(pglobal,parameter) != 0 )
		return -1;
#else
	memset(parameter->szconfig,0,sizeof(parameter->szconfig));
	strcpy(parameter->szconfig,"D:\\phlinux.ini");
	if( LoadFile(pglobal,parameter) != 0 )
		return -1;
#endif
	return 0;
}

void BindNic(  PHGlobal *pglobal,PH_parameter *parameter )
{
#ifndef WIN32
	if( strlen(parameter->nicName) == 0 )
		return;

	register int fd, interface;
	struct arpreq arp;
	struct ifconf ifc;
	struct ifreq buf[16];
	char mac[32] = "";
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0) {
		ifc.ifc_len = sizeof buf;
		ifc.ifc_buf = (caddr_t) buf;
		if (!ioctl(fd, SIOCGIFCONF, (char *) &ifc)) {
			interface = ifc.ifc_len / sizeof(struct ifreq);
			parameter->nicNumber=interface;
			while(parameter->nicNumber-->0)
			{
				if(strcmp(buf[parameter->nicNumber].ifr_name,parameter->nicName)==0)
				{
					if (!(ioctl(fd, SIOCGIFADDR, (char *) &buf[parameter->nicNumber])))
					{
						strcpy(pglobal->szBindAddress,inet_ntoa(((struct sockaddr_in*) (&buf[parameter->nicNumber].ifr_addr))->sin_addr));
						printf("%s\n",inet_ntoa(((struct sockaddr_in*) (&buf[parameter->nicNumber].ifr_addr))->sin_addr));
						printf("NIC bind success\n");
					}
				}
			}
		}
	}
	return ;
#endif
}

int ParameterAnalysis(int argc,char *argv[],PHGlobal *pglobal,PH_parameter *parameter)
{
	BOOL rin;
	
	if(parameter->bFirstRun)
	{
		if(parameter->bNewIni||parameter->bDaemon||parameter->bAppointIni||parameter->bUser)
		{
			HelpPrint();
			return -1;
		}
		else
		{
			NewIni(parameter->szconfig,pglobal,parameter);
			exit (0);
		}
	}
	if(parameter->bUser)
	{	
#ifndef WIN32
		char command[1024];
		int i = 0;
		memset(command,0,sizeof(command));
		sprintf(command,"su %s -c \"",parameter->szuserName);
		for( i = 0;i < argc; i++ ) {
			if( !strcmp( argv[i], "-u" ) )
				continue;
			else if( i>0 && !strcmp( argv[i-1], "-u"  ) )
				continue;
			sprintf(command,"%s %s",command,argv[i]);
		}
		sprintf(command,"%s \"",command);
		system(command);
/*		ostringstream ostr;
		ostr<<"su "<<userName<<" -c "<<"\""<<argv[0];
		for(m_comIterator=m_comList.begin(); m_comIterator!=m_comList.end();++m_comIterator)
			ostr<<" "<<*m_comIterator;
		ostr<<"\"";
		std::string gstr=ostr.str();
		system(gstr.c_str());*/
#endif
		return -1;
	}  	
	if(parameter->bNewIni)
	{
		memset(parameter->szconfig,0,sizeof(parameter->szconfig));
		if( strlen(parameter->szconfig) == 0 )
#ifndef WIN32
			strcpy(parameter->szconfig,"/etc/phlinux.conf");
#else
			strcpy(parameter->szconfig,"D:\\phlinux.ini");
#endif
		if( NewIni(parameter->szconfig,pglobal,parameter) != 0 )
			return -1;
		BindNic( pglobal,parameter );
	}
	if(parameter->bAppointIni)
	{
		if( LoadFile( pglobal,parameter ) != 0 )
			return -1;
		BindNic( pglobal,parameter );
	}
	if(parameter->bDaemon)
	{
		if(!parameter->bNewIni && !parameter->bAppointIni)
		{
			if( InitIni(pglobal,parameter) != 0 )
				return -1;
			BindNic( pglobal,parameter );
		}
//		LOG(1)(" bNewIni[%d] and bAppointIni[%d] has an unexpected error,please check it.",parameter->bNewIni,parameter->bAppointIni);
//		rin=InDaemon();
//		if(!rin)
//			return -1;	
	}

	return 0;
}

int NewIni(char* save_file,PHGlobal *global,PH_parameter *parameter)
{
	NewHost(global);
	NewUserID(global);
	NewUserPWD(global);
	NewnicName(global,parameter);
	NewFile(parameter);
	if( SaveFile(save_file,global,parameter) != 0 )
		return -1;
	return 0;
}

void NewFile(PH_parameter *parameter)
{
#ifndef WIN32
	printf("Log to use(default /var/log/phddns.log):");
#else
	printf("Log to use(default d:\\phclientlog.log):");
#endif

	memset(parameter->logfile,0,sizeof(parameter->logfile));
	fgets(parameter->logfile,100,stdin);
	if( strlen(trim(parameter->logfile)) == 0 ) {
#ifndef WIN32
		strcpy(parameter->logfile,"/var/log/phddns.log");
#else
		strcpy(parameter->logfile,"d:\\phclientlog.log");
#endif
	}
	printf("%s\n",parameter->logfile);
	return;
}

void NewHost(PHGlobal *pglobal)
{
	while(1)
	{
		printf("Enter server address(press ENTER use phddns60.oray.net):");
		memset(pglobal->szHost,0,sizeof(pglobal->szHost));
		fgets(pglobal->szHost,100,stdin);
		if( strlen(trim(pglobal->szHost)) == 0 )
		{
			strcpy(pglobal->szHost,"phddns60.oray.net");
			break;
		}
		else
		{
			if(strchr(pglobal->szHost,'.')==NULL)
			{
				continue;
			}
			else
				break;
		}
	}
//	SetValue("settings","szHost",pglobal->szHost,pglobal);
}

void NewUserID(PHGlobal *pglobal)
{
	while(1)
	{
		printf("Enter your Oray account:");
		memset(pglobal->szUserID,0,sizeof(pglobal->szUserID));
		fgets(pglobal->szUserID,100,stdin);
		if( strlen(trim(pglobal->szUserID)) == 0 )
			continue;
		else 
			break;
	}
//	SetValue("settings","szUserID",pglobal->szUserID,pglobal);
}

void NewUserPWD(PHGlobal *pglobal)
{
	while(1)
	{
		char c[1024];
		int i=0;
		printf("Password:");
#ifndef WIN32
		while((c[i++]=getch_c())!='\n' ) 	
		{
			if(c[i-1]==127 && i==1)
			{  c[i-1]='\0'; i=0;}
			if(c[i-1]==127 && i-2>=0)
			{  c[i-2]='\0';i=i-2;}
		}
#else
		while((c[i++]=getch())!='\r' )
		{
			if(c[i-1]=='\b'&&i==1)
			{  c[i-1]='\0'; i=0;}
			if(c[i-1]=='\b'&& i-2>=0)
			{  c[i-2]='\0';i=i-2;}
		}
#endif
		c[i-1]='\0';
		printf("\n");
		strcpy(pglobal->szUserPWD,c);
		if( strlen(pglobal->szUserPWD) == 0 )
		{
			continue;
		}
		else
		{
			break;
		}
	}
//	SetValue("settings","szUserPWD",pglobal->szUserPWD,pglobal);
}

void NewnicName(PHGlobal *pglobal,PH_parameter *parameter)
{
	ShowNic( pglobal,parameter );
	printf("Choose one(default eth0):");
	fgets(parameter->nicName,100,stdin);
	if(strlen(trim(parameter->nicName)) == 0)
		strcpy(parameter->nicName,"eth0");
//	SetValue("settings","nicName",parameter->nicName,pglobal);
}

int SaveFile(char* save_file,PHGlobal *pglobal,PH_parameter *parameter)
{
	while(1)
	{
		char com[10];
		char enter[6];
		memset(enter,0,sizeof(enter));
		memset(com,0,sizeof(com));
		printf("Save to configuration file (%s",save_file);
		printf(")?(yes/no/other):");
		fgets(com,100,stdin);
		if( strlen(trim(com)) == 0 )
		{
			continue;
		}
		else
		{
			if(strcmp(com,"y")==0||strcmp(com,"yes")==0||strcmp(com,"Y")==0)
			{

				if( MyWriteFile(save_file,pglobal,parameter) != 0 )
				{
					printf("Write %s failed!\n",save_file);
					printf("press ENTER any key to exit");
					getchar();
					exit(0);
				}
				else
					break;
			}
			else {
				if(strcmp(com,"n")==0||strcmp(com,"no")==0||strcmp(com,"N")==0)
					break;
				else {
					if(strcmp(com,"other")==0)
					{
						char command[100];
						memset(command,0,sizeof(command));
						fflush(stdin);
#ifndef WIN32
						printf("Enter configuration filename(/etc/phlinux.conf):");
#else
						printf("Enter configuration filename(d://phlinux.ini):");
#endif
						fgets(command,100,stdin);
						if( strlen(trim(command)) == 0 )
						{
							if( MyWriteFile(command,pglobal,parameter) != 0 )
							{
								printf("Write %s failed!\n",command);
								printf("press ENTER any key to exit");
								getchar();
								exit(0);
							}
							else 
								break;
						}
						else
						{
							if( MyWriteFile(command,pglobal,parameter) != 0 )
							{
								printf("Write %s failed!\n",command);
								printf("press ENTER any key to exit");
								getchar();
								exit(0);
							}
							else
								break;
						}
					}
					else
						continue;
				}
			}
		}
	}

	return 0;
}

int LoadFile(PHGlobal *global,PH_parameter *parameter)
{
	if( access(parameter->szconfig,0) >=  0 && MyReadFile(parameter->szconfig,global,parameter) == 0 )
	{					
		int flag = -1;
//		memset(global->szHost,0,sizeof(global->szHost));
//		GetValue("settings","szHost",global->szHost,global);
		if( strlen(global->szHost) == 0 )
		{
			printf("no appointing szHost\n");
			NewHost(global);
			flag=1;
		}

//		memset(global->szUserID,0,sizeof(global->szUserID));
//		GetValue("settings","szUserID",&global->szUserID,global);
		if( strlen(global->szUserID) == 0 )
		{
			printf("no appointing szUserID\n");
			NewUserID(global);
			flag=1;
		}

//		memset(global->szUserPWD,0,sizeof(global->szUserPWD));
//		GetValue("settings","szUserPWD",&global->szUserPWD,global);
		if( strlen(global->szUserPWD) == 0 )
		{
			printf("no appointing szUserPWD\n");
			NewUserPWD(global);
			flag=1;
		}

//		memset(global->nicName,0,sizeof(global->nicName));
//		GetValue("settings","nicName",&global->nicName,global);
		if( strlen(parameter->nicName) == 0 )
		{
			printf("no appointing nicName\n");
			NewnicName(global,parameter);
			flag=1;
		}

		if( strlen(parameter->logfile) == 0 )
		{
			printf("no appointing logfile\n");
			NewFile(parameter);
			flag=1;
		}

		if(flag == 1)
			if( SaveFile(parameter->szconfig,global,parameter) != 0 )
				return -1;
	}
	else 
	{
		if( NewIni(parameter->szconfig,global,parameter) != 0 )
			return -1;
	}

	return 0;
}

int InDaemon(void)
{
#ifndef WIN32
	printf("phlinux started as daemon!\n");
	if(0 != daemon(0,0))
	{
		printf("daemon(0,0) failed\n");
		return -1;
	}
#else
	printf("phlinux started daemon failed\n");

#endif
	return 0;
}

void ShowNic( PHGlobal *pglobal,PH_parameter *parameter )
{
#ifndef WIN32
	register int fd, interface;
	struct arpreq arp;
	struct ifconf ifc;
	struct ifreq buf[16];
	char mac[32] = ""; 
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0) {
		ifc.ifc_len = sizeof buf;
		ifc.ifc_buf = (caddr_t) buf;
		if (!ioctl(fd, SIOCGIFCONF, (char *) &ifc)) {
			interface = ifc.ifc_len / sizeof(struct ifreq);
			parameter->nicNumber=interface;
			printf("Network interface(s):\n");
			while (interface-- > 0) {
				if (!(ioctl(fd, SIOCGIFADDR, (char *) &buf[interface]))) {
					 printf("[%s] = [IP:%s]\n",buf[interface].ifr_name,\
							inet_ntoa(((struct sockaddr_in *)(&buf[interface].ifr_addr)) -> sin_addr));

				} else 
					perror("cpm: ioctl device");
			}
		} else
			perror("cpm: ioctl");
	} else
		perror("cpm: socket");
	close(fd);
#endif
}

void SetValue(char* root,char* attribute,char* value,PHGlobal *pglobal,PH_parameter *parameter )
{
	FILE *fp;
	char buffer[100];

	if(access(parameter->szconfig,0) < 0 ) {
		if( (fp = fopen(parameter->szconfig,"w")) == NULL ) {
			printf("Modify configration file failed\n");
			return;
		}

		memset(buffer,0,sizeof(buffer));
		sprintf(buffer,"[%s]",root);
		fputs(buffer,fp);
		memset(buffer,0,sizeof(buffer));
		sprintf(buffer,"%s = %s",attribute,value);
		fputs(buffer,fp);

		fclose(fp);
	}
	else {
		if( (fp = fopen(parameter->szconfig,"a+")) == NULL ) {
			printf("Modify configration file failed\n");
			return;
		}
		
		memset(buffer,0,sizeof(buffer));
		sprintf(buffer,"%s = %s",attribute,value);
		fgets(buffer,sizeof(buffer),fp);

		fclose(fp);
	}
}

int MyWriteFile(char *filename,PHGlobal *pglobal,PH_parameter *parameter)
{
	FILE *fp = 0;
	char buffer[100];
	
	if( (fp = fopen(filename,"w")) == NULL ) {
		printf("write configration file failed\n");
		return -1;
	}

	memset(buffer,0,sizeof(buffer));
	strcpy(buffer,"[settings] \n");
	fputs(buffer,fp);

	memset(buffer,0,sizeof(buffer));
	sprintf(buffer,"szHost = %s \n",pglobal->szHost);
	fputs(buffer,fp);

	memset(buffer,0,sizeof(buffer));
	sprintf(buffer,"szUserID = %s \n",pglobal->szUserID);
	fputs(buffer,fp);

	memset(buffer,0,sizeof(buffer));
	sprintf(buffer,"szUserPWD = %s \n",pglobal->szUserPWD);
	fputs(buffer,fp);

	memset(buffer,0,sizeof(buffer));
	sprintf(buffer,"nicName = %s \n",parameter->nicName);
	fputs(buffer,fp);

	memset(buffer,0,sizeof(buffer));
	sprintf(buffer,"szLog = %s \n",parameter->logfile);
	fputs(buffer,fp);

	fclose(fp);

	return 0;
}

int MyReadFile(char* filename,PHGlobal *pglobal,PH_parameter *parameter)
{
	FILE *fp;
	char buffer[100];
	char* pointer;
	char attribute[100];
	char tvalue[100];
	int circle = 0;

	if( (fp = fopen(filename,"r")) == NULL ) {
		printf("read configration file failed\n");
		return -1;
	}

	memset(buffer,0,sizeof(buffer));
	fgets(buffer,sizeof(buffer),fp);

	while( memset(buffer,0,sizeof(buffer)) && fgets(buffer,sizeof(buffer),fp) && circle < 5 ) {
		pointer = strtok(buffer,"=");
		memset(attribute,0,sizeof(attribute));
		strcpy(attribute,trim(pointer));
		if( strstr(attribute,"settings") != NULL )
			continue;
		
		pointer = strtok( NULL,"=");
		memset(tvalue,0,sizeof(tvalue));
		strcpy(tvalue,trim(pointer));

		if( !strcmp(attribute,"szHost") ){
			memset(pglobal->szHost,0,sizeof(pglobal->szHost));
			strcpy(pglobal->szHost,tvalue);
		}
		else if( !strcmp(attribute,"szUserID") ){
			memset(pglobal->szUserID,0,sizeof(pglobal->szUserID));
			strcpy(pglobal->szUserID,tvalue);
		}
		else if( !strcmp(attribute,"szUserPWD") ){
			memset(pglobal->szUserPWD,0,sizeof(pglobal->szUserPWD));
			strcpy(pglobal->szUserPWD,tvalue);
		}
		else if( !strcmp(attribute,"nicName") ){
			memset(parameter->nicName,0,sizeof(parameter->nicName));
			strcpy(parameter->nicName,tvalue);
		}
		else if( !strcmp(attribute,"szLog") ){
			memset(parameter->logfile,0,sizeof(parameter->logfile));
			strcpy(parameter->logfile,tvalue);
		}
		circle ++;
	}

	fclose(fp);

	return 0;

}

char *trim(char *str) 
{
	char *ibuf = str, *obuf = str;
      int i = 0, cnt = 0;

      /*
      **  Trap NULL
      */

      if (str)
      {
            /*
            **  Remove leading spaces (from RMLEAD.C)
            */

            for (ibuf = str; *ibuf && ISSPACE(*ibuf); ++ibuf)
                  ;
            if (str != ibuf)
                  memmove(str, ibuf, ibuf - str);

            /*
            **  Collapse embedded spaces (from LV1WS.C)
            */

			while (*ibuf)
			{
				if (ISSPACE(*ibuf) && cnt)
					ibuf++;
				else
				{
					if (!ISSPACE(*ibuf))
						cnt = 0;
					else
					{
						*ibuf = ' ';
						cnt = 1;
					}
					obuf[i++] = *ibuf++;
				}
			}
			obuf[i] = 0;

            /*
            **  Remove trailing spaces (from RMTRAIL.C)
            */
            while (--i >= 0)
            {
                  if (!ISSPACE(obuf[i]))
                        break;
            }
            obuf[++i] = 0;
      }
      return str;
}

int GetValue(char* root,char* attribute,char* value,PHGlobal *pglobal,PH_parameter *parameter)
{
	FILE *fp;
	char buffer[100];
	char* pointer;
	char attribute_copy[100];
	char tvalue[100];

	if(access(parameter->szconfig,0) < 0 ) {
			printf("Read configration file failed,file does not exist\n");
			return -1;
	}
	else {
		if( (fp = fopen(parameter->szconfig,"r")) == NULL ) {
			printf("Read configration file failed\n");
			return -1;
		}

		while( memset(buffer,0,sizeof(buffer)) && fgets(buffer,sizeof(buffer),fp) ) {
			if( strstr(buffer,attribute) != NULL ) {
				pointer = strtok(buffer,"=");
				memset(attribute_copy,0,sizeof(attribute_copy));
				strcpy(attribute_copy,trim(pointer));
				if( strcmp(attribute_copy,attribute) != 0 )
					continue;

				pointer = strtok( NULL,"=");
				memset(tvalue,0,sizeof(tvalue));
				strcpy(tvalue,trim(pointer));


				break;
			}
			else
				continue;
		}

		fclose(fp);
	}

	return 0;
}

int init_parameter(PH_parameter *parameter)
{
	parameter->bNewIni=FALSE;
	parameter->bDaemon=FALSE;
	parameter->bAppointIni=FALSE;
	parameter->bUser=FALSE;
	parameter->bFirstRun=FALSE;
	parameter->nicNumber=0;

	return 0;
}

#ifndef WIN32
int getch_c (void){
	int ch;
	struct termios oldt, newt;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ECHO|ICANON);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	ch = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return ch;
}
#endif
