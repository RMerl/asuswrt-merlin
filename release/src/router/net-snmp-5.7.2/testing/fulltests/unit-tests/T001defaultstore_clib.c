/* HEADER Testing the default_store API */
int i, j;
char buf[1024];

/* first we load everything up */

for(i = 0; i < NETSNMP_DS_MAX_IDS; i++) {
    /* booleans */
    for(j = 0; j < NETSNMP_DS_MAX_SUBIDS; j++) {
        OKF(SNMPERR_SUCCESS == netsnmp_ds_set_boolean(i, j, (i*j)%2),
            ("default store boolean: setting %d/%d returned failure", i, j));
        OKF(SNMPERR_SUCCESS == netsnmp_ds_set_int(i, j, i*j),
            ("default store int: setting %d/%d returned failure", i, j));
        sprintf(buf,"%d/%d", i, j);
        OKF(SNMPERR_SUCCESS == netsnmp_ds_set_string(i, j, buf),
            ("default store string: setting %d/%d returned failure", i, j));
    }
}
    
/* then we check all the values */

for(i = 0; i < NETSNMP_DS_MAX_IDS; i++) {
    /* booleans */
    for(j = 0; j < NETSNMP_DS_MAX_SUBIDS; j++) {
        OKF(netsnmp_ds_get_boolean(i, j) == (i*j)%2,
            ("default store boolean %d/%d was the expected value", i, j));
        OKF(netsnmp_ds_get_int(i, j) == (i*j),
            ("default store int %d/%d was the expected value", i, j));
        sprintf(buf,"%d/%d", i, j);
        OKF(strcmp(netsnmp_ds_get_string(i, j), buf) == 0,
            ("default store string %d/%d was the expected value", i, j));
    }
}

netsnmp_ds_shutdown();
