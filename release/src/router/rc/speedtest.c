/*
	speedtest.c for simple traceroute to get upload bandwidth
*/

#include "rc.h"
#define DETECT_FILE "/tmp/speedtest"

int speedtest_main(int argc, char **argv)
{
	return speedtest();
}

int speedtest()
{
	FILE *fp = NULL;
	char cmd[80];
	char detect_ip[20] = "8.8.8.8";
	char get_ip[6][30];
	char gateway_ip[6][30], get_sp[6][10];
	char line[254];
	char *no_host = "!H", *no_net = "!N", *tr_str = "traceroute to";
	int i, j;
	char *p;
	char *ms_p, *line_p, *tmp_p, tmp_buf[10]; 
	double ms_num, ms_n, ms_avg, sp, base_ms = 0;

	j = 0;
	while(j < 6){
		memset(&get_ip[j], 0, sizeof(get_ip[j]));
		memset(&gateway_ip[j], 0, sizeof(gateway_ip[j]));
		memset(&get_sp[j], 0, sizeof(get_sp[j]));
		++j;
	}

	snprintf(cmd, sizeof(cmd), "traceroute -m 6 %s >%s", detect_ip, DETECT_FILE);
	remove(DETECT_FILE);
	
	system(cmd);

	i = 0;
	if ((fp = fopen(DETECT_FILE, "r")) != NULL) 
	{
		while(i < 6)
		{
			/* get ip */
			if( fgets(line, sizeof(line), fp) != NULL ) 
			{
                                if(strstr(line, no_host) || strstr(line, no_net))       // add
                                {
                                        printf("detect: no host or no network\n");      // tmp test
					fclose(fp);
                                        return -1;
                                }

				if(strstr(line, tr_str))
					continue;

				p=index(line,'(');
				if(!p)
					continue;

				j=1;
				while((*p)!=')')
				{
					++p;
					get_ip[i][j-1]=*p;
					++j;
				}
				get_ip[i][j-2]='\0';
				
			}

			/* get speed */
                        ms_num = 0.0;
                        ms_n = 0;
                        line_p = line;
                        while((ms_p = strstr(line_p, "ms")) != NULL)
                        {
                                ++ms_n;
                                tmp_p = ms_p-2;
                                line_p = ms_p + 1;
                                j = 0;
                                memset(tmp_buf, 0, sizeof(tmp_buf));

                                for(; *tmp_p!= ' '; --tmp_p, ++j)
                                {
                                        tmp_buf[9-j] = *tmp_p;
                                }
                                j=0;
                                while((tmp_buf[j]=='\0') && (j < 10))
                                {
                                        tmp_buf[j] = '0';
                                        ++j;
                                }

                                ms_num += atof(tmp_buf);
                                //dbg("get ms from [%s]: %f\n", get_ip[i], ms_num);     // tmp test
                        }
                        if(ms_num > 0.0)
                        {
                                memset(tmp_buf, 0, sizeof(tmp_buf));
                                ms_avg = ms_num / ms_n;

				/* judge codes */
                                if(i==1)
                                {
                                        base_ms = ms_avg;
                                        //dbg("base ms is %f\n", base_ms);   // tmp test
                                }
                                else if(i > 1)
                                {
                                        if(ms_avg > (3*base_ms))
                                        {
                                                //dbg("!(over 3*base) %f\n", ms_avg); // tmp test
                                                ms_avg = base_ms*(1.30);
                                                //dbg("judge that as %f\n", ms_avg);       // tmp test
                                        }
                                        else if(ms_avg > (2*base_ms))
                                        {
                                                //dbg("!(over the 2*base) %f\n", ms_avg); // tmp test
                                                ms_avg = base_ms*(1.20);
                                                //dbg("judge that as %f\n", ms_avg);       // tmp test
                                        }
                                }

                                sp = (((38.00/1024.00)/(ms_avg/1000000)))/2;
                                sprintf(get_sp[i], "%.2f", sp);
                                //dbg("get ms avg : %f(%f), sp=%s\n", ms_avg, ms_num, get_sp[i]);      // tmp test

				// save speed result into nvram
				nvram_set("qos_obw_detect", get_sp[i]);
                        }
                        ++i;
		}

		i=0;
		while(i<6)
		{
			sprintf(gateway_ip[i],"%s",get_ip[i]);
			++i;
		}	
	}
	else
		return 0;

	fclose(fp);


	return 1;
}
