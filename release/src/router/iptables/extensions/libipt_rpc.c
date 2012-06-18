/* RPC extension for IP connection matching, Version 2.2
 * (C) 2000 by Marcelo Barbosa Lima <marcelo.lima@dcc.unicamp.br>
 *	- original rpc tracking module
 *	- "recent" connection handling for kernel 2.3+ netfilter
 *
 * (C) 2001 by Rusty Russell <rusty@rustcorp.com.au>
 *	- upgraded conntrack modules to oldnat api - kernel 2.4.0+
 *
 * (C) 2002,2003 by Ian (Larry) Latter <Ian.Latter@mq.edu.au>
 *	- upgraded conntrack modules to newnat api - kernel 2.4.20+
 *	- extended matching to support filtering on procedures
 *
 * libipt_rpc.c,v 2.2 2003/01/12 18:30:00
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 **
 *	Userspace library syntax:
 *	--rpc [--rpcs procedure1,procedure2,...procedure128] [--static]
 *
 *	Procedures can be supplied in either numeric or named formats.
 *	Without --rpcs, this module will behave as the old record-rpc.
 **
 *	Note to all:
 *
 *	RPCs should not be exposed to the internet - ask the Pentagon;
 *
 *	  "The unidentified crackers pleaded guilty in July to charges
 *	   of juvenile delinquency stemming from a string of Pentagon
 *	   network intrusions in February.
 *
 *	   The youths, going by the names TooShort and Makaveli, used
 *	   a common server security hole to break in, according to
 *	   Dane Jasper, owner of the California Internet service
 *	   provider, Sonic. They used the hole, known as the 'statd'
 *	   exploit, to attempt more than 800 break-ins, Jasper said."
 *
 *	From: Wired News; "Pentagon Kids Kicked Off Grid" - Nov 6, 1998
 *	URL:  http://www.wired.com/news/politics/0,1283,16098,00.html
 **
 */

#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <rpc/rpc.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_rpc.h>
#include <time.h>


const int IPT_RPC_RPCS = 1;
const int IPT_RPC_STRC = 2;

const int IPT_RPC_INT_LBL = 1;
const int IPT_RPC_INT_NUM = 2;
const int IPT_RPC_INT_BTH = 3;

const int IPT_RPC_CHAR_LEN = 11;
const int IPT_RPC_MAX_ENTS = 128;

const char preerr[11] = "RPC match:";


static int k_itoa(char *string, int number)
{
	int maxoctet = IPT_RPC_CHAR_LEN - 1;
	int store[IPT_RPC_CHAR_LEN];
	int counter;


        for (counter=0 ; maxoctet != 0 && number != 0; counter++, maxoctet--) {
		store[counter] = number / 10;
		store[counter] = number - ( store[counter] * 10 );
		number = number / 10;
        }

        for ( ; counter != 0; counter--, string++)
		*string = store[counter - 1] + 48;

	*string = 0;

	return(0);
}


static int k_atoi(char *string)
{
	unsigned int result = 0;
	int maxoctet = IPT_RPC_CHAR_LEN;


        for ( ; *string != 0 && maxoctet != 0; maxoctet--, string++) {
                if (*string < 0)
                        return(0);
                if (*string == 0)
                        break;
                if (*string < 48 || *string > 57) {
                        return(0);
                }
                result = result * 10 + ( *string - 48 );
        }

	return(result);
}


static void print_rpcs(char *c_procs, int i_procs, int labels)
{
	int   proc_ctr;
	char *proc_ptr;
	unsigned int proc_num;
	struct rpcent *rpcent;


	for (proc_ctr=0; proc_ctr <= i_procs; proc_ctr++) {

		if ( proc_ctr != 0 )
			printf(",");

		proc_ptr = c_procs;
		proc_ptr += proc_ctr * IPT_RPC_CHAR_LEN;
		proc_num = k_atoi(proc_ptr);

		/* labels(1) == no labels, only numbers
		 * labels(2) == no numbers, only labels
		 * labels(3) == both labels and numbers
		 */

		if (labels == IPT_RPC_INT_LBL || labels == IPT_RPC_INT_BTH ) {
			if ( (rpcent = getrpcbynumber(proc_num)) == NULL )
				printf("unknown");
			else
				printf("%s", rpcent->r_name);
		}

		if (labels == IPT_RPC_INT_BTH )
			printf("(");

		if (labels == IPT_RPC_INT_NUM || labels == IPT_RPC_INT_BTH )
			printf("%i", proc_num);

		if (labels == IPT_RPC_INT_BTH )
			printf(")");

	}

}


static void help(void) 
{
	printf(
		"RPC v%s options:\n"
		"  --rpcs list,of,procedures"
		"\ta list of rpc program numbers to apply\n"
		"\t\t\t\tie. 100003,mountd,rquotad (numeric or\n"
		"\t\t\t\tname form; see /etc/rpc).\n"
		"  --strict"
		"\t\t\ta flag to force the drop of packets\n"
		"\t\t\t\tnot containing \"get\" portmapper requests.\n",
		IPTABLES_VERSION);
}


static struct option opts[] = {
	{ "rpcs", 1, 0, '1'},
	{ "strict", 0, 0, '2'},
	{0}
};


static void init(struct ipt_entry_match *match, unsigned int *nfcache)
{
	struct ipt_rpc_info *rpcinfo = ((struct ipt_rpc_info *)match->data);



	/* initialise those funky user vars */
	rpcinfo->i_procs = -1;
	rpcinfo->strict  =  0;
	memset((char *)rpcinfo->c_procs, 0, sizeof(rpcinfo->c_procs));
}


static void parse_rpcs_string(char *string, struct ipt_entry_match **match)
{
	char err1[64] = "%s invalid --rpcs option-set: `%s' (at character %i)";
	char err2[64] = "%s unable to resolve rpc name entry: `%s'";
	char err3[64] = "%s maximum number of --rpc options (%i) exceeded";
	char buf[256];
	char *dup = buf;
	int idup = 0;
	int term = 0;
	char *src, *dst;
	char *c_procs;
	struct rpcent *rpcent_ptr;
	struct ipt_rpc_info *rpcinfo = (struct ipt_rpc_info *)(*match)->data;


	memset(buf, 0, sizeof(buf));

	for (src=string, dst=buf; term != 1 ; src++, dst++) {

		if ( *src != ',' && *src != '\0' ) {
			if ( ( *src >= 65 && *src <= 90 ) || ( *src >= 97 && *src <= 122) ) {
				*dst = *src;
				idup = 1;

			} else if ( *src >= 48 && *src <= 57 ) {
				*dst = *src;

			} else {
				exit_error(PARAMETER_PROBLEM, err1, preerr,
					   string, src - string + 1);

			}

		} else {
			*dst = '\0';
			if ( idup == 1 ) {
				if ( (rpcent_ptr = getrpcbyname(dup)) == NULL )
					exit_error(PARAMETER_PROBLEM, err2,
						   preerr, dup);
				idup = rpcent_ptr->r_number;
			} else {
				idup = k_atoi(dup);
			}

			rpcinfo->i_procs++;
			if ( rpcinfo->i_procs > IPT_RPC_MAX_ENTS )
				exit_error(PARAMETER_PROBLEM, err3, preerr,
					   IPT_RPC_MAX_ENTS);
				
			c_procs  = (char *)rpcinfo->c_procs;
			c_procs += rpcinfo->i_procs * IPT_RPC_CHAR_LEN;
			
			memset(buf, 0, sizeof(buf));
			k_itoa((char *)dup, idup);

			strcpy(c_procs, dup);
	
			if ( *src == '\0')
				term = 1;

			idup = 0;
			memset(buf, 0, sizeof(buf));
			dst = (char *)buf - 1;
		}
	}

	return;
}


static int parse(int c, char **argv, int invert, unsigned int *flags,
		const struct ipt_entry *entry,
		unsigned int *nfcache,
		struct ipt_entry_match **match)
{
	struct ipt_rpc_info *rpcinfo = (struct ipt_rpc_info *)(*match)->data;


	switch (c)
	{
	case '1':
		if (invert)
			exit_error(PARAMETER_PROBLEM,
                                   "%s unexpected '!' with --rpcs\n", preerr);
		if (*flags & IPT_RPC_RPCS)
                        exit_error(PARAMETER_PROBLEM,
                                   "%s repeated use of --rpcs\n", preerr);
		parse_rpcs_string(optarg, match);

		*flags |= IPT_RPC_RPCS;
		break;

	case '2':
		if (invert)
			exit_error(PARAMETER_PROBLEM,
                                   "%s unexpected '!' with --strict\n", preerr);
		if (*flags & IPT_RPC_STRC)
                        exit_error(PARAMETER_PROBLEM,
                                   "%s repeated use of --strict\n", preerr);
		rpcinfo->strict = 1;
		*flags |= IPT_RPC_STRC;
		break;

	default:
		return 0;
	}

	return 1;

}


static void final_check(unsigned int flags)
{
	if (flags != (flags | IPT_RPC_RPCS)) {
		printf("%s option \"--rpcs\" was not used ... reverting ", preerr);
		printf("to old \"record-rpc\" functionality ..\n");
	}
}


static void print(const struct ipt_ip *ip,
		const struct ipt_entry_match *match,
		int numeric)
{
	struct ipt_rpc_info *rpcinfo = ((struct ipt_rpc_info *)match->data);


	printf("RPCs");
	if(rpcinfo->strict == 1)
		printf("[strict]");

	printf(": ");

	if(rpcinfo->i_procs == -1) {
		printf("any(*)");

	} else {
		print_rpcs((char *)&rpcinfo->c_procs, rpcinfo->i_procs, IPT_RPC_INT_BTH);
	}
	printf(" ");

}


static void save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	struct ipt_rpc_info *rpcinfo = ((struct ipt_rpc_info *)match->data);


	if(rpcinfo->i_procs > -1) {
		printf("--rpcs ");
		print_rpcs((char *)&rpcinfo->c_procs, rpcinfo->i_procs, IPT_RPC_INT_NUM);
		printf(" ");
	}

	if(rpcinfo->strict == 1)
		printf("--strict ");

}


static struct iptables_match rpcstruct = { 
	.next		= NULL,
	.name		= "rpc",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_rpc_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_rpc_info)),
	.help		= &help,
	.init		= &init,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};


void _init(void)
{
	register_match(&rpcstruct);
}

