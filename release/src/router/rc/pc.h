#define MIN_DAY 1
#define MAX_DAY 7

#define MIN_HOUR 0
#define MAX_HOUR 23

typedef struct pc_event pc_event_s;
struct pc_event{
	char e_name[32];
	int start_day;
	int end_day;
	int start_hour;
	int end_hour;
	pc_event_s *next;
};

typedef struct pc pc_s;
struct pc{
	int enabled;
	char device[32];
	char mac[18];
	pc_event_s *events;
	pc_s *next;
};

