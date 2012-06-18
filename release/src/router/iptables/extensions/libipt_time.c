/* Shared library add-on to iptables to add TIME matching support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h> /* for 'offsetof' */
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_time.h>
#include <time.h>

static int globaldays;

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"TIME v%s options:\n"
" [ --timestart value ] [ --timestop value] [ --days listofdays ] [ --datestart value ] [ --datestop value ]\n"
"          timestart value : HH:MM (default 00:00)\n"
"          timestop  value : HH:MM (default 23:59)\n"
"                            Note: daylight savings time changes are not tracked\n"
"          listofdays value: a list of days to apply\n"
"                            from Mon,Tue,Wed,Thu,Fri,Sat,Sun\n"
"                            Coma speparated, no space, case sensitive.\n"
"                            Defaults to all days.\n"
"          datestart value : YYYY[:MM[:DD[:hh[:mm[:ss]]]]]\n"
"                            If any of month, day, hour, minute or second is\n"
"                            not specified, then defaults to their smallest\n"
"                            1900 <= YYYY < 2037\n"
"                               1 <= MM <= 12\n"
"                               1 <= DD <= 31\n"
"                               0 <= hh <= 23\n"
"                               0 <= mm <= 59\n"
"                               0 <= ss <= 59\n"
"          datestop  value : YYYY[:MM[:DD[:hh[:mm[:ss]]]]]\n"
"                            If the whole option is ommited, default to never stop\n"
"                            If any of month, day, hour, minute or second is\n"
"                            not specified, then default to their smallest\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "timestart", 1, 0, '1' },
	{ "timestop", 1, 0, '2' },
	{ "days", 1, 0, '3'},
	{ "datestart", 1, 0, '4' },
	{ "datestop", 1, 0, '5' },
	{0}
};

/* Initialize the match. */
static void
init(struct ipt_entry_match *m, unsigned int *nfcache)
{
	struct ipt_time_info *info = (struct ipt_time_info *)m->data;
	globaldays = 0;
        /* By default, we match on everyday */
	info->days_match = 127;
	/* By default, we match on every hour:min of the day */
	info->time_start = 0;
	info->time_stop  = 1439;  /* (23*60+59 = 1439 */
	/* By default, we don't have any date-begin or date-end boundaries */
	info->date_start = 0;
	info->date_stop  = LONG_MAX;
}

/**
 * param: part1, a pointer on a string 2 chars maximum long string, that will contain the hours.
 * param: part2, a pointer on a string 2 chars maximum long string, that will contain the minutes.
 * param: str_2_parse, the string to parse.
 * return: 1 if ok, 0 if error.
 */
static int
split_time(char **part1, char **part2, const char *str_2_parse)
{
	unsigned short int i,j=0;
	char *rpart1 = *part1;
	char *rpart2 = *part2;
	unsigned char found_column = 0;

	/* Check the length of the string */
	if (strlen(str_2_parse) > 5)
		return 0;
	/* parse the first part until the ':' */
	for (i=0; i<2; i++)
	{
		if (str_2_parse[i] == ':')
			found_column = 1;
		else
			rpart1[i] = str_2_parse[i];
	}
	if (!found_column)
		i++;
	j=i;
	/* parse the second part */
	for (; i<strlen(str_2_parse); i++)
	{
		rpart2[i-j] = str_2_parse[i];
	}
	/* if we are here, format should be ok. */
	return 1;
}

static int
parse_number(char *str, int num_min, int num_max, int *number)
{
	/* if the number starts with 0, replace it with a space else
	string_to_number() will interpret it as octal !! */
	if (strlen(str) == 0)
		return 0;

	if ((str[0] == '0') && (str[1] != '\0'))
		str[0] = ' ';

	return string_to_number(str, num_min, num_max, number);
}

static void
parse_time_string(int *hour, int *minute, const char *time)
{
	char *hours;
	char *minutes;
	hours = (char *)malloc(3);
	minutes = (char *)malloc(3);
	memset(hours, 0, 3);
	memset(minutes, 0, 3);

	if (split_time((char **)&hours, (char **)&minutes, time) == 1)
	{
		*hour = 0;
		*minute = 0;
		if ((parse_number((char *)hours, 0, 23, hour) != -1) &&
		    (parse_number((char *)minutes, 0, 59, minute) != -1))
		{
			free(hours);
			free(minutes);
			return;
		}
	}

	free(hours);
	free(minutes);

	/* If we are here, there was a problem ..*/
	exit_error(PARAMETER_PROBLEM,
		   "invalid time `%s' specified, should be HH:MM format", time);
}

/* return 1->ok, return 0->error */
static int
parse_day(int *days, int from, int to, const char *string)
{
	char *dayread;
	char *days_str[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	unsigned short int days_of_week[7] = {64, 32, 16, 8, 4, 2, 1};
	unsigned int i;

	dayread = (char *)malloc(4);
	bzero(dayread, 4);
	if ((to-from) != 3) {
		free(dayread);
		return 0;
	}
	for (i=from; i<to; i++)
		dayread[i-from] = string[i];
	for (i=0; i<7; i++)
		if (strcmp(dayread, days_str[i]) == 0)
		{
			*days |= days_of_week[i];
			free(dayread);
			return 1;
		}
	/* if we are here, we didn't read a valid day */
	free(dayread);
	return 0;
}

static void
parse_days_string(int *days, const char *daystring)
{
	int len;
	int i=0;
	char *err = "invalid days `%s' specified, should be Sun,Mon,Tue... format";

	len = strlen(daystring);
	if (len < 3)
		exit_error(PARAMETER_PROBLEM, err, daystring);	
	while(i<len)
	{
		if (parse_day(days, i, i+3, daystring) == 0)
			exit_error(PARAMETER_PROBLEM, err, daystring);
		i += 4;
	}
}

static int
parse_date_field(const char *str_to_parse, int str_to_parse_s, int start_pos,
                 char *dest, int *next_pos)
{
	unsigned char found_value = 0;
	unsigned char found_column = 0;
	int i;

	for (i=0; i<2; i++)
	{
		if ((i+start_pos) >= str_to_parse_s) /* don't exit boundaries of the string..  */
			break;
		if (str_to_parse[i+start_pos] == ':')
			found_column = 1;
		else
		{
			found_value = 1;
			dest[i] = str_to_parse[i+start_pos];
		}
	}
	if (found_value == 0)
		return 0;
	*next_pos = i + start_pos;
	if (found_column == 0)
		++(*next_pos);
	return 1;
}

static int
split_date(char *year, char *month,  char *day,
           char *hour, char *minute, char *second,
           const char *str_to_parse)
{
        int i;
        unsigned char found_column = 0;
	int str_to_parse_s = strlen(str_to_parse);

        /* Check the length of the string */
        if ((str_to_parse_s > 19) ||  /* YYYY:MM:DD:HH:MM:SS */
            (str_to_parse_s < 4))     /* YYYY*/
                return 0;

	/* Clear the buffers */
        memset(year, 0, 4);
	memset(month, 0, 2);
	memset(day, 0, 2);
	memset(hour, 0, 2);
	memset(minute, 0, 2);
	memset(second, 0, 2);

	/* parse the year YYYY */
	found_column = 0;
	for (i=0; i<5; i++)
	{
		if (i >= str_to_parse_s)
			break;
		if (str_to_parse[i] == ':')
		{
			found_column = 1;
			break;
		}
		else
			year[i] = str_to_parse[i];
	}
	if (found_column == 1)
		++i;

	/* parse the month if it exists */
	if (! parse_date_field(str_to_parse, str_to_parse_s, i, month, &i))
		return 1;

	if (! parse_date_field(str_to_parse, str_to_parse_s, i, day, &i))
		return 1;

	if (! parse_date_field(str_to_parse, str_to_parse_s, i, hour, &i))
		return 1;

	if (! parse_date_field(str_to_parse, str_to_parse_s, i, minute, &i))
		return 1;

	parse_date_field(str_to_parse, str_to_parse_s, i, second, &i);

        /* if we are here, format should be ok. */
        return 1;
}

static time_t
parse_date_string(const char *str_to_parse)
{
	char year[5];
	char month[3];
	char day[3];
	char hour[3];
	char minute[3];
	char second[3];
	struct tm t;
	time_t temp_time;

	memset(year, 0, 5);
	memset(month, 0, 3);
	memset(day, 0, 3);
	memset(hour, 0, 3);
	memset(minute, 0, 3);
	memset(second, 0, 3);

        if (split_date(year, month, day, hour, minute, second, str_to_parse) == 1)
        {
		memset((void *)&t, 0, sizeof(struct tm));
		t.tm_isdst = -1;
		t.tm_mday = 1;
		if (!((parse_number(year, 1900, 2037, &(t.tm_year)) == -1) ||
		      (parse_number(month, 1, 12, &(t.tm_mon)) == -1) ||
		      (parse_number(day, 1, 31, &(t.tm_mday)) == -1) ||
		      (parse_number(hour, 0, 9999, &(t.tm_hour)) == -1) ||
		      (parse_number(minute, 0, 59, &(t.tm_min)) == -1) ||
		      (parse_number(second, 0, 59, &(t.tm_sec)) == -1)))
		{
			t.tm_year -= 1900;
			--(t.tm_mon);
			temp_time = mktime(&t);
			if (temp_time != -1)
				return temp_time;
		}
	}
	exit_error(PARAMETER_PROBLEM,
		   "invalid date `%s' specified, should be YYYY[:MM[:DD[:hh[:mm[:ss]]]]] format", str_to_parse);
}

#define IPT_TIME_START 0x01
#define IPT_TIME_STOP  0x02
#define IPT_TIME_DAYS  0x04
#define IPT_DATE_START 0x08
#define IPT_DATE_STOP  0x10

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	struct ipt_time_info *timeinfo = (struct ipt_time_info *)(*match)->data;
	int hours, minutes;
	time_t temp_date;

	switch (c)
	{
		/* timestart */
	case '1':
		if (invert)
			exit_error(PARAMETER_PROBLEM,
                                   "unexpected '!' with --timestart");
		if (*flags & IPT_TIME_START)
                        exit_error(PARAMETER_PROBLEM,
                                   "Can't specify --timestart twice");
		parse_time_string(&hours, &minutes, optarg);
		timeinfo->time_start = (hours * 60) + minutes;
		*flags |= IPT_TIME_START;
		break;
		/* timestop */
	case '2':
		if (invert)
			exit_error(PARAMETER_PROBLEM,
                                   "unexpected '!' with --timestop");
		if (*flags & IPT_TIME_STOP)
                        exit_error(PARAMETER_PROBLEM,
                                   "Can't specify --timestop twice");
		parse_time_string(&hours, &minutes, optarg);
		timeinfo->time_stop = (hours * 60) + minutes;
		*flags |= IPT_TIME_STOP;
		break;

		/* days */
	case '3':
		if (invert)
			exit_error(PARAMETER_PROBLEM,
                                   "unexpected '!' with --days");
		if (*flags & IPT_TIME_DAYS)
                        exit_error(PARAMETER_PROBLEM,
                                   "Can't specify --days twice");
		parse_days_string(&globaldays, optarg);
		timeinfo->days_match = globaldays;
		*flags |= IPT_TIME_DAYS;
		break;

		/* datestart */
	case '4':
		if (invert)
			exit_error(PARAMETER_PROBLEM,
                                   "unexpected '!' with --datestart");
		if (*flags & IPT_DATE_START)
			exit_error(PARAMETER_PROBLEM,
                                   "Can't specify --datestart twice");
		temp_date = parse_date_string(optarg);
		timeinfo->date_start = temp_date;
		*flags |= IPT_DATE_START;
		break;

		/* datestop*/
	case '5':
		if (invert)
			exit_error(PARAMETER_PROBLEM,
                                   "unexpected '!' with --datestop");
		if (*flags & IPT_DATE_STOP)
			exit_error(PARAMETER_PROBLEM,
                                   "Can't specify --datestop twice");
		temp_date = parse_date_string(optarg);
		timeinfo->date_stop = temp_date;
		*flags |= IPT_DATE_STOP;
		break;
	default:
		return 0;
	}
	return 1;
}

/* Final check */
static void
final_check(unsigned int flags)
{
	/* Nothing to do */
}


static void
print_days(int daynum)
{
	char *days[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	unsigned short int days_of_week[7] = {64, 32, 16, 8, 4, 2, 1};
	unsigned short int i, nbdays=0;

	for (i=0; i<7; i++) {
		if ((days_of_week[i] & daynum) == days_of_week[i])
		{
			if (nbdays>0)
				printf(",%s", days[i]);
			else
				printf("%s", days[i]);
			++nbdays;
		}
	}
	printf(" ");
}

static void
divide_time(int fulltime, int *hours, int *minutes)
{
	*hours = fulltime / 60;
	*minutes = fulltime % 60;
}

static void
print_date(time_t date, char *command)
{
	struct tm *t;

	/* If it's default value, don't print..*/
	if (((date == 0) || (date == LONG_MAX)) && (command != NULL))
		return;
	t = localtime(&date);
	if (command != NULL)
		printf("%s %d:%d:%d:%d:%d:%d ", command, (t->tm_year + 1900), (t->tm_mon + 1),
			t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
        else
        	printf("%d-%d-%d %d:%d:%d ", (t->tm_year + 1900), (t->tm_mon + 1),
			t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
}

/* Prints out the matchinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	struct ipt_time_info *time = ((struct ipt_time_info *)match->data);
	int hour_start, hour_stop, minute_start, minute_stop;

	divide_time(time->time_start, &hour_start, &minute_start);
	divide_time(time->time_stop, &hour_stop, &minute_stop);
	printf("TIME ");
	if (time->time_start != 0)
		printf("from %d:%d ", hour_start, minute_start);
	if (time->time_stop != 1439) /* 23*60+59 = 1439 */
		printf("to %d:%d ", hour_stop, minute_stop);
	printf("on ");
	if (time->days_match == 127)
		printf("all days ");
	else
		print_days(time->days_match);
	if (time->date_start != 0)
	{
		printf("starting from ");
		print_date(time->date_start, NULL);
	}
	if (time->date_stop != LONG_MAX)
	{
		printf("until date ");
		print_date(time->date_stop, NULL);
	}
}

/* Saves the data in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	struct ipt_time_info *time = ((struct ipt_time_info *)match->data);
	int hour_start, hour_stop, minute_start, minute_stop;

	divide_time(time->time_start, &hour_start, &minute_start);
	divide_time(time->time_stop, &hour_stop, &minute_stop);
	if (time->time_start != 0)
		printf("--timestart %.2d:%.2d ",
		        hour_start, minute_start);
	
	if (time->time_stop != 1439) /* 23*60+59 = 1439 */
		printf("--timestop %.2d:%.2d ",
		        hour_stop, minute_stop);
	
	if (time->days_match != 127)
	{
		printf("--days ");
		print_days(time->days_match);
		printf(" ");
	}
	print_date(time->date_start, "--datestart");
	print_date(time->date_stop, "--datestop");
}

/* have to use offsetof() instead of IPT_ALIGN(), since kerneltime must not
 * be compared when user deletes rule with '-D' */
static
struct iptables_match timestruct = {
	.next		= NULL,
	.name		= "time",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_time_info)),
	.userspacesize	= offsetof(struct ipt_time_info, kerneltime),
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
	register_match(&timestruct);
}
