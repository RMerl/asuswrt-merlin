/* HEADER Testing snmp_enum */

#define CONFIG_TYPE "snmp-enum-unit-test"
#define STRING1 "life, and everything"
#define STRING2 "restaurant at the end of the universe"
#define STRING3 "label3"
#define LONG_STRING "a-string-of-255-characters-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------"

#define STORE_AND_COMPARE(maj, min, s)                                  \
    {                                                                   \
        FILE *fp;                                                       \
        int read = 0;                                                   \
        char *p, contents[4096];                                        \
                                                                        \
        se_store_list(maj, min, CONFIG_TYPE);                           \
        fp = fopen(tmp_persist_file, "r");                              \
        if (fp) {                                                       \
            read = fread(contents, 1, sizeof(contents) - 1, fp);        \
            fclose(fp);                                                 \
        }                                                               \
        contents[read > 0 ? read : 0] = '\0';                           \
        for (p = contents; *p; ++p)                                     \
            if (*p == '\n')                                             \
                *p = '|';                                               \
        OKF(strcmp(contents, (s)) == 0,                                 \
            ("stored list %s <> %s", (s), contents));                   \
        remove(tmp_persist_file);                                       \
    }

char tmp_persist_file[256];
char *se_find_result;

sprintf(tmp_persist_file, "/tmp/snmp-enum-unit-test-%d", getpid());
netsnmp_setenv("SNMP_PERSISTENT_FILE", tmp_persist_file, 1);

init_snmp_enum("snmp");

STORE_AND_COMPARE(1, 1, "enum 1:1|");

se_add_pair(1, 1, strdup("hi"), 1);

STORE_AND_COMPARE(1, 1, "enum 1:1 1:hi|");

se_add_pair(1, 1, strdup("there"), 2);

STORE_AND_COMPARE(1, 1, "enum 1:1 1:hi 2:there|");

se_add_pair(1, 1, strdup(LONG_STRING), 3);
se_add_pair(1, 1, strdup(LONG_STRING), 4);
se_add_pair(1, 1, strdup(LONG_STRING), 5);
se_add_pair(1, 1, strdup(LONG_STRING), 6);
se_add_pair(1, 1, strdup(LONG_STRING), 7);
se_add_pair(1, 1, strdup(LONG_STRING), 8);
se_add_pair(1, 1, strdup(LONG_STRING), 9);

STORE_AND_COMPARE(1, 1, "enum 1:1 1:hi 2:there 3:" LONG_STRING " 4:" LONG_STRING
                 " 5:" LONG_STRING " 6:" LONG_STRING " 7:" LONG_STRING
                 " 8:" LONG_STRING " 9:" LONG_STRING "|");

se_add_pair(1, 1, strdup(LONG_STRING), 10);

STORE_AND_COMPARE(1, 1, "enum 1:1 1:hi 2:there 3:" LONG_STRING " 4:" LONG_STRING
                 " 5:" LONG_STRING " 6:" LONG_STRING " 7:" LONG_STRING
                 " 8:" LONG_STRING " 9:" LONG_STRING "|"
                 "enum 1:1 10:" LONG_STRING "|");

OK(se_find_value(1, 1, "hi") == 1,
   "lookup by number #1 should be the proper string");
OK(strcmp(se_find_label(1, 1, 2), "there") == 0,
   "lookup by string #1 should be the proper number");


se_add_pair_to_slist("testing", strdup(STRING1), 42);
se_add_pair_to_slist("testing", strdup(STRING2), 2);
se_add_pair_to_slist("testing", strdup(STRING3), 2);
    
OK(se_find_value_in_slist("testing", STRING1) == 42,
   "lookup by number should be the proper string");
OK(strcmp(se_find_label_in_slist("testing", 2), STRING2) == 0,
   "lookup by string should be the proper number");

se_clear_slist("testing");


se_read_conf("enum",
             NETSNMP_REMOVE_CONST(char *, "2:3 1:apple 2:pear 3:kiwifruit"));
OK(se_find_list(2, 3), "list (2, 3) should be present");
if (se_find_list(2, 3)) {
  OK(se_find_value(2, 3, "kiwifruit") == 3,
     "lookup by string should return the proper value");
  se_find_result = se_find_label(2, 3, 2);
  OK(se_find_result && strcmp(se_find_result, "pear") == 0,
     "lookup by label should return the proper string");
}

se_read_conf("enum",
             NETSNMP_REMOVE_CONST(char *, "fruit 1:apple 2:pear 3:kiwifruit"));
OK(se_find_value_in_slist("fruit", "kiwifruit") == 3,
   "lookup by string should return the proper value");
se_find_result = se_find_label_in_slist("fruit", 2);
OK(se_find_result && strcmp(se_find_result, "pear") == 0,
   "lookup by value should return the proper string");

clear_snmp_enum();
unregister_all_config_handlers();
