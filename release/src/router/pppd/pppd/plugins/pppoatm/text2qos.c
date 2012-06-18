/* text2qos.c - Converts textual representation of QOS parameters to binary
		encoding */

/* Written 1996-2000 by Werner Almesberger, EPFL-LRC/ICA */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "atm.h"


#define fetch __atmlib_fetch


#define RATE_ERROR -2


int __t2q_get_rate(const char **text,int up)
{
    const char mult[] = "kKmMgGg";
    const char *multiplier;
    char *end;
    unsigned int rate,fract;
    int power;

    if (!strncmp(*text,"max",3)) {
	*text += 3;
	return ATM_MAX_PCR;
    }
    rate = strtoul(*text,&end,10);
    power = fract = 0;
    if (*end == '.')
	for (end++; *end && isdigit(*end); end++) {
	    fract = fract*10+*end-48;
	    if (--power == -9) break;
	}
    multiplier = NULL;
    if (*end && (multiplier = strchr(mult,*end))) {
	while (multiplier >= mult) {
	    if (rate > UINT_MAX/1000) return RATE_ERROR;
	    rate *= 1000;
	    power += 3;
	    multiplier -= 2;
	}
	end++;
    }
    while (power && fract)
	if (power < 0) {
	    fract /= 10;
	    power++;
	}
	else {
	    fract *= 10;
	    power--;
	}
    rate += fract;
    if (strlen(end) < 3) {
	if (multiplier) return RATE_ERROR;
    }
    else if (!strncmp(end,"cps",3)) end += 3;
	else if (!strncmp(end,"bps",3)) {
		rate = (rate+(up ? 8*ATM_CELL_PAYLOAD-1 : 0))/8/
		  ATM_CELL_PAYLOAD;
		end += 3;
	    }
	    else if (multiplier) return RATE_ERROR;
    if (rate > INT_MAX) return RATE_ERROR;
    *text = end;
    return rate;
}


static int params(const char **text,struct atm_trafprm *a,
  struct atm_trafprm *b)
{
    int value;
    char *end;

    if (*(*text)++ != ':') return -1;
    while (1) {
	if (!**text) return -1;
	switch (fetch(text,"max_pcr=","pcr=","min_pcr=","max_sdu=","sdu=",
	  NULL)) {
	    case 0:
		if ((value = __t2q_get_rate(text,0)) == RATE_ERROR) return -1;
		if (a) a->max_pcr = value;
		if (b) b->max_pcr = value;
		break;
	    case 1:
		if ((value = __t2q_get_rate(text,0)) == RATE_ERROR) return -1;
		if (a) a->pcr = value;
		if (b) b->pcr = value;
		break;
	    case 2:
		if ((value = __t2q_get_rate(text,1)) == RATE_ERROR) return -1;
		if (value == ATM_MAX_PCR) return -1;
		if (a) a->min_pcr = value;
		if (b) b->min_pcr = value;
		break;
	    case 3:
	    case 4:
		value = strtol(*text,&end,10);
		if (value < 0) return -1;
		*text = end;
		if (a) a->max_sdu = value;
		if (b) b->max_sdu = value;
		break;
	    default:
		return 0;
	}
	if (!**text) break;
	if (*(*text)++ != ',') return -1;
    }
    return 0;
}


int text2qos(const char *text,struct atm_qos *qos,int flags)
{
    int traffic_class,aal;

    traffic_class = ATM_NONE;
    aal = ATM_NO_AAL;
    do {
	static const unsigned char aal_number[] = { ATM_AAL0, ATM_AAL5 };
	int item;

	item = fetch(&text,"!none","ubr","cbr","vbr","abr","aal0","aal5",NULL);
	switch (item) {
	    case 1:
	    case 2:
		/* we don't support VBR yet */
	    case 4:
		traffic_class = item;
		break;
	    case 5:
	    case 6:
		aal = aal_number[item-5];
		break;
	    default:
		return -1;
	}
    }
    while (*text == ',' ? text++ : 0);
    if (!traffic_class) return -1;
    if (qos && !(flags & T2Q_DEFAULTS)) memset(qos,0,sizeof(*qos));
    if (qos) qos->txtp.traffic_class = qos->rxtp.traffic_class = traffic_class;
    if (qos && aal) qos->aal = aal;
    if (!*text) return 0;
    if (params(&text,qos ? &qos->txtp : NULL,qos ? &qos->rxtp : NULL))
	return -1;
    if (!*text) return 0;
    switch (fetch(&text,"tx","rx",NULL)) {
	case 0:
	    if (!fetch(&text,":none",NULL)) {
		if (qos) qos->txtp.traffic_class = ATM_NONE;
		if (*text == ',') text++;
		break;
	    }
	    if (params(&text,qos ? &qos->txtp : NULL,NULL)) return -1;
	    break;
	case 1:
	    text -= 2;
	    break;
	default:
	    return -1;
    }
    if (!*text) return 0;
    if (fetch(&text,"rx",NULL)) return -1;
    if (!fetch(&text,":none",NULL) && qos) qos->rxtp.traffic_class = ATM_NONE;
    else if (params(&text,qos ? &qos->rxtp : NULL,NULL)) return -1;
    return *text ? -1 : 0;
}
