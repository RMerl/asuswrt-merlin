struct cyclades_monitor {
        unsigned long           int_count;
        unsigned long           char_count;
        unsigned long           char_max;
        unsigned long           char_last;
};

#define CYGETMON                0x435901
#define CYGETTHRESH             0x435902
#define CYSETTHRESH             0x435903
#define CYGETDEFTHRESH          0x435904
#define CYSETDEFTHRESH          0x435905
#define CYGETTIMEOUT            0x435906
#define CYSETTIMEOUT            0x435907
#define CYGETDEFTIMEOUT         0x435908
#define CYSETDEFTIMEOUT         0x435909
