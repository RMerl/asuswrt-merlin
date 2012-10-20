#define APPS_LIST_ASUS "/opt/lib/ipkg/lists/optware.asus"
#define APPS_LIST_OLEG "/opt/lib/ipkg/lists/optware.oleg"
#define APPS_STATUS "/opt/lib/ipkg/status"
#define APPS_INFO "/opt/lib/ipkg/info"
#define APPS_FILE_END "\n\n"
#define FIELD_PACKAGE "Package: "
#define FIELD_VERSION "Version: "
#define FIELD_SOURCE "Source: "
#define FIELD_URL "URL: "
#define FIELD_DESCRIPTION "Description: "
#define FIELD_DEPENDS "Depends: "
#define FIELD_OPTIONALUTILITY "OptionalUtility: "
#define FIELD_ENABLED "Enabled: "
#define FIELD_HELPPATH "HelpPath: "
#define FIELD_FILENAME "Filename: "
#define FIELD_STATUS "Status: "

#define APP_OWNER_ALL "all"
#define APP_OWNER_ASUS "asus"
#define APP_OWNER_OLEG "oleg"
#define APP_OWNER_OTHERS "others"

#define FIELD_YES "yes"
#define FIELD_NO "no"

typedef struct apps_info apps_info_t;

#pragma pack(1) // let struct be neat by byte.
struct apps_info{
	char *name;
	char *version;
	char *new_version;
	char *installed;
	char *enabled;
	char *source;
	char *url;
	char *description;
	char *depends;
	char *optional_utility;
	char *new_optional_utility;
	char *help_path;
	char *new_file_name;
	char *from_owner;
	apps_info_t *next;
} ;
#pragma pack() // End.

#ifdef DEBUG_USB
#define apps_dbg(fmt, args...) do{ \
		FILE *fp = fopen("/tmp/apps.log", "a+"); \
		if(fp){ \
			fprintf(fp, "[apps_dbg: %s] ", __FUNCTION__); \
			fprintf(fp, fmt, ## args); \
			fclose(fp); \
		} \
	}while(0)
#else
#define apps_dbg printf
#endif

char *get_status_field(const char *target, const char *field);
apps_info_t *initial_apps_data();
void free_apps_list(apps_info_t **apps_info_list);
apps_info_t *get_apps_list(char *argv);
int printf_apps_info();
