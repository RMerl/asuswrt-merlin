	// WIN32 notice
// DO NOT BIND OTHER PROTOCOLS
// Some protocols did not write well and no pass response to ADSLPROT.SYS.
// For best test result, please unbind all other protocols.

#ifndef WIN32
//#include <nvram/typedefs.h>
#include <bcmnvram.h>
//#include <nvparse.h>
#endif
#include <stdio.h>
#include "fw_conf.h"
#include <memory.h>
#include <string.h>

#include "../dsl_define.h"


extern void myprintf(const char *fmt, ...);
extern int AddPvc(int idx, int vlan_id, int vpi, int vci, int encap, int mode);
extern int SetQosToPvc(int idx, int SvcCat, int Pcr, int Scr, int Mbs);
extern int SetAdslMode(int EnumAdslModeValue, int FromAteCmd);
extern int SetAdslType(int EnumAdslTypeValue, int FromAteCmd);
extern int SetSNRMOffset(int EnumSNRMOffsetValue, int FromAteCmd); /* Paul add 2012/9/24 */
extern int SetSRA(int EnumSRAValue, int FromAteCmd); /* Paul add 2012/10/15 */

extern char* strcpymax(char *to, const char*from, int max_to_len);
//
//int DelPvc(int vpi, int vci)

typedef struct {
	int enabled;
    int vpi;
    int vci;
    int encap;
    int mode; 
    char proto[16];
    int svc_cat;
    int pcr;
    int scr;
    int mbs;    
} PVC_PARAM;    

static PVC_PARAM m_pvc[MAX_PVC];

void nvram_load_one_pvc(PVC_PARAM* pPvcSetting, int idx)
{
#ifndef WIN32
    char buf[32];
    char* pValue;

    sprintf(buf,"dsl%d_enable",idx);
    pValue = nvram_safe_get(buf);
    if (*pValue == 0) pPvcSetting->enabled = 0;
    else pPvcSetting->enabled = atoi(pValue);

    if (pPvcSetting->enabled == 0) return;

    sprintf(buf,"dsl%d_vpi",idx);
    pValue = nvram_safe_get(buf);
    if (*pValue == 0) pPvcSetting->vpi = 0;
    else pPvcSetting->vpi = atoi(pValue);
    
    sprintf(buf,"dsl%d_vci",idx);
    pValue = nvram_safe_get(buf);
    if (*pValue == 0) pPvcSetting->vci = 0;
    else pPvcSetting->vci = atoi(pValue);    

    sprintf(buf,"dsl%d_encap",idx);
    pValue = nvram_safe_get(buf);
    if (*pValue == 0) pPvcSetting->encap = 0;
    else pPvcSetting->encap = atoi(pValue);    
    
    sprintf(buf,"dsl%d_proto",idx);
    pValue = nvram_safe_get(buf);
    if (*pValue == 0) pPvcSetting->proto[0] = 0;
    else strcpymax(pPvcSetting->proto,pValue,sizeof(pPvcSetting->proto));        

    sprintf(buf,"dsl%d_svc_cat",idx);
    pValue = nvram_safe_get(buf);
    if (*pValue == 0) pPvcSetting->svc_cat = 0;
    else pPvcSetting->svc_cat = atoi(pValue);

    sprintf(buf,"dsl%d_pcr",idx);
    pValue = nvram_safe_get(buf);
    if (*pValue == 0) pPvcSetting->pcr = 0;
    else pPvcSetting->pcr = atoi(pValue);

    sprintf(buf,"dsl%d_scr",idx);
    pValue = nvram_safe_get(buf);
    if (*pValue == 0) pPvcSetting->scr = 0;
    else pPvcSetting->scr = atoi(pValue);

    sprintf(buf,"dsl%d_mbs",idx);
    pValue = nvram_safe_get(buf);
    if (*pValue == 0) pPvcSetting->mbs = 0;
    else pPvcSetting->mbs = atoi(pValue);


	// convert proto to modem firmware mode value
	if (!strcmp(pPvcSetting->proto,"pppoa"))
	{
		pPvcSetting->mode = 1;
	}
	else if (!strcmp(pPvcSetting->proto,"ipoa"))
	{
		pPvcSetting->mode = 2;	
	}	

    
#else
    pPvcSetting->vpi = 0;
    pPvcSetting->vci = 33;
    pPvcSetting->encap = 0;
    strcpymax(pPvcSetting->proto,"pppoe",sizeof(pPvcSetting->proto));       
    pPvcSetting->svc_cat = 1;
    pPvcSetting->pcr = 1000;
    pPvcSetting->scr = 0;
    pPvcSetting->mbs = 0;    
#endif  
}


int nvram_load_adsl_type()
{
#ifndef WIN32
    char* pValue;
    pValue = nvram_safe_get("dslx_annex");
    if (*pValue == 0) return EnumAdslTypeA;
    else return atoi(pValue);  
#else    
    return EnumAdslTypeA;
#endif        
}

int nvram_load_adsl_mode()
{
#ifndef WIN32
    char* pValue;
    pValue = nvram_safe_get("dslx_modulation");
    if (*pValue == 0) return EnumAdslModeMultimode;
    else return atoi(pValue);        
#else    
    return EnumAdslModeMultimode;
#endif        
}

/* Paul add start 2012/9/24, for SNR Margin tweaking. */
int nvram_load_SNRM_offset() 
{
#ifndef WIN32
    char* pValue;
    pValue = nvram_safe_get("dslx_snrm_offset");
    if (*pValue == 0) return EnumSNRMOffset;
    else return atoi(pValue);  
#else    
    return EnumSNRMOffset;
#endif        
}

/* Paul add 2012/10/15, for setting SRA. */
int nvram_load_SRA() 
{
#ifndef WIN32
    char* pValue;
    pValue = nvram_safe_get("dslx_sra");
    if (*pValue == 0) return EnumSRA;
    else return atoi(pValue);  
#else    
    return EnumSRA;
#endif        
}

int nvram_load_config_num(int* iptv)
{
#ifndef WIN32
    char* pValue;
    /* Paul modify 2012/10/16, when there is no STB port configured, PVC for IPTV should still be created for UDP Proxy */
    int ret;
    //pValue = nvram_safe_get("switch_stb_x");
    //if (*pValue == 0) *iptv = 1;
    //else *iptv = atoi(pValue);
    pValue = nvram_safe_get("dslx_config_num");
    //if (*pValue == 0) return 0;
    //else return atoi(pValue);
    ret = *pValue ? atoi(pValue) : 0;
    *iptv = (ret > 1);
    return ret;
#else    
    return 1;
#endif        
}

void nvram_load_pvcs(int config_num, int iptv_port, int internet_pvc)
{
    int i;
    memset(&m_pvc,0,sizeof(m_pvc));
    if (iptv_port == 0)
    {
        // user choose port mapping = none
        // only active internet PVC
        for (i=0; i<MAX_PVC; i++)
        {
            if (i==internet_pvc)
            {
                nvram_load_one_pvc(&m_pvc[i],i);
            }
        }        
    }
    else
    {
        for (i=0; i<MAX_PVC; i++)
        {
            nvram_load_one_pvc(&m_pvc[i],i);
        }    
    }
}

int write_ipvc_mode(int config_num, int iptv_port)
{
    int i;    
	int internet_pvc = 0;
	int all_bridge_pvc = 1;	    
    char buf[128];
	FILE* fp;
	
    memset(&m_pvc,0,sizeof(m_pvc));
    for (i=0; i<MAX_PVC; i++)
    {
        nvram_load_one_pvc(&m_pvc[i],i);
    }    
	
    fp = fopen("/tmp/adsl/ipvc_mode.txt","wb");
    if (fp == NULL) return 0;

    for (i=0; i<MAX_PVC; i++)
    {
        if (m_pvc[i].enabled==0)
        {
            // this is a empty pvc , we do not add it
        }
        else
        {
            sprintf(buf,"%d:vpi=%d,vci=%d,encap=%d,mode=%d,proto=%s\n",i,m_pvc[i].vpi,m_pvc[i].vci,
                m_pvc[i].encap,m_pvc[i].mode,m_pvc[i].proto);
            fputs(buf,fp);
            sprintf(buf,"%d:svc_cat=%d,pcr=%d,scr=%d,mbs=%d\n",i,m_pvc[i].svc_cat,m_pvc[i].pcr,
                m_pvc[i].scr,m_pvc[i].mbs);            
            fputs(buf,fp);
            if (strcmp(m_pvc[i].proto,"bridge") != 0)            
            {
			    all_bridge_pvc = 0;
			    internet_pvc = i;
            }            
        }
    }

	if (all_bridge_pvc == 1)
	{
		internet_pvc = 0;
	}

    sprintf(buf,"internet PVC=%d, iptv port=%d\n",internet_pvc, iptv_port);
    fputs(buf,fp);
    
    fclose(fp);
    
    return internet_pvc;
}

int nvram_set_adsl_fw_setting(int config_num, int iptv_port, int internet_pvc, int chg_mode)
{
	int i;    
	int EnumAdslType;
	int EnumAdslMode;
	int EnumSNRM_Offset;
	int EnumSRA_;
	int all_bridge_pvc = 1;	    
	int pvc_idx;
	FILE* fp;
	if (chg_mode)
	{
	    // load setting from nvram
#if DSL_N55U_ANNEX_B == 1
		EnumAdslType=EnumAdslTypeB;
#else
	    EnumAdslType=nvram_load_adsl_type();
#endif	
	    EnumAdslMode=nvram_load_adsl_mode();
	    EnumSNRM_Offset=nvram_load_SNRM_offset(); /* Paul add 2012/9/24 */
	    EnumSRA_=nvram_load_SRA(); /* Paul add 2012/10/15 */

	    // set the setting to adsl fw    
	    SetAdslType(EnumAdslType,0);
	    SetAdslMode(EnumAdslMode,0);
	    SetSNRMOffset(EnumSNRM_Offset,0); /* Paul add 2012/9/24 */
	    SetSRA(EnumSRA_,0); /* Paul add 2012/10/15 */
	    myprintf("ADSL MODE : %d\n", EnumAdslType);                
	    myprintf("ADSL TYPE : %d\n", EnumAdslMode); 
			myprintf("SNRM Offset: %d\n", EnumSNRM_Offset); /* Paul add 2012/9/24 */
			myprintf("SRA: %d\n", EnumSRA_); /* Paul add 2012/10/15 */
    }
    
    // add internet pvc to first vlan
    for (i=0; i<MAX_PVC; i++)
    {
        if (m_pvc[i].enabled == 0)
        {
            // this is a empty pvc , we do not add it
        }
        else
        {
            if (internet_pvc == i)
            {
                int ret;        
                myprintf("PVC%d : %d %d %d %d\n", i, m_pvc[i].vpi, m_pvc[i].vci, m_pvc[i].encap, m_pvc[i].mode);
                ret = AddPvc(0, 1, m_pvc[i].vpi, m_pvc[i].vci, m_pvc[i].encap, m_pvc[i].mode);
                myprintf("%d\n", ret );            
                ret = SetQosToPvc(0,m_pvc[i].svc_cat,m_pvc[i].pcr,m_pvc[i].scr,m_pvc[i].mbs);
                myprintf("%d\n", ret );
                break;
            }
        }
    }
        
    if (iptv_port != 0)
    {
        // add other pvcs
        pvc_idx = 1;
        for (i=0; i<MAX_PVC; i++)
        {
            if (m_pvc[i].enabled == 0)
            {
                // this is a empty pvc , we do not add it
            }
            else
            {
                if (internet_pvc != i)
                {
                    int ret;        
                    myprintf("PVC%d : %d %d %d %d\n", i, m_pvc[i].vpi, m_pvc[i].vci, m_pvc[i].encap, m_pvc[i].mode);
                    ret = AddPvc(pvc_idx, pvc_idx+1, m_pvc[i].vpi, m_pvc[i].vci, m_pvc[i].encap, m_pvc[i].mode);
                    myprintf("%d\n", ret );            
                    ret = SetQosToPvc(pvc_idx,m_pvc[i].svc_cat,m_pvc[i].pcr,m_pvc[i].scr,m_pvc[i].mbs);
                    myprintf("%d\n", ret );
					pvc_idx++;
                }
            }
        }        
    }        

    return 0;
}

void reload_pvc(void)
{
	int config_num; 
	int iptv_port;
	int internet_pvc;

	// delete timer
	disable_polling();
	// wait timer function exit
	wait_polling_stop();			
	
	config_num = nvram_load_config_num(&iptv_port);
	internet_pvc = write_ipvc_mode(config_num, iptv_port);		  
	nvram_load_pvcs(config_num, iptv_port, internet_pvc);
	nvram_set_adsl_fw_setting(config_num, iptv_port, internet_pvc, 0);

	enable_polling();	
}


static int m_always_report_link_up = 0;

void nvram_adslsts(char* sts)
{
	if(m_always_report_link_up)
	{
		nvram_set("dsltmp_adslsyncsts", "up");
	}
	else
	{
		nvram_set("dsltmp_adslsyncsts", sts);
	}
}

void nvram_adslsts_detail(char* sts)
{
	if(m_always_report_link_up)
	{
		nvram_set("dsltmp_adslsyncsts_detail", "up");
	}
	else
	{
		nvram_set("dsltmp_adslsyncsts_detail", sts);
	}
}

int nvram_get_dbg_flag()
{
	char* tmp;
	if(nvram_match("dslx_debug", "1"))
	{
		m_always_report_link_up = 1;
	}
	tmp=nvram_safe_get("tp_init_debug");
	if (!strcmp(tmp,""))
	{
		return 0;
	}
	else
	{
		return atoi(tmp);
	}
}


