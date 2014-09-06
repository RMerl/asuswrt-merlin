/*
 * snmpusm.h
 *
 * Header file for USM support.
 */

#ifndef SNMPUSM_H
#define SNMPUSM_H

#include <net-snmp/library/callback.h>

#ifdef __cplusplus
extern          "C" {
#endif

#define WILDCARDSTRING "*"

    /*
     * General.
     */
#define USM_MAX_ID_LENGTH		1024    /* In bytes. */
#define USM_MAX_SALT_LENGTH		128     /* In BITS. */
#define USM_DES_SALT_LENGTH		64      /* In BITS. */
#define USM_AES_SALT_LENGTH		128     /* In BITS. */
#define USM_MAX_KEYEDHASH_LENGTH	128     /* In BITS. */

#define USM_TIME_WINDOW			150
#define USM_MD5_AND_SHA_AUTH_LEN        12      /* bytes */
#define USM_MAX_AUTHSIZE                USM_MD5_AND_SHA_AUTH_LEN

#define USM_SEC_MODEL_NUMBER            SNMP_SEC_MODEL_USM

    /*
     * Structures.
     */
    struct usmStateReference {
        char           *usr_name;
        size_t          usr_name_length;
        u_char         *usr_engine_id;
        size_t          usr_engine_id_length;
        oid            *usr_auth_protocol;
        size_t          usr_auth_protocol_length;
        u_char         *usr_auth_key;
        size_t          usr_auth_key_length;
        oid            *usr_priv_protocol;
        size_t          usr_priv_protocol_length;
        u_char         *usr_priv_key;
        size_t          usr_priv_key_length;
        u_int           usr_sec_level;
    };


    /*
     * struct usmUser: a structure to represent a given user in a list 
     */
    /*
     * Note: Any changes made to this structure need to be reflected in
     * the following functions: 
     */

    struct usmUser;
    struct usmUser {
        u_char         *engineID;
        size_t          engineIDLen;
        char           *name;
        char           *secName;
        oid            *cloneFrom;
        size_t          cloneFromLen;
        oid            *authProtocol;
        size_t          authProtocolLen;
        u_char         *authKey;
        size_t          authKeyLen;
        oid            *privProtocol;
        size_t          privProtocolLen;
        u_char         *privKey;
        size_t          privKeyLen;
        u_char         *userPublicString;
        size_t          userPublicStringLen;
        int             userStatus;
        int             userStorageType;
       /* these are actually DH * pointers but only if openssl is avail. */
        void           *usmDHUserAuthKeyChange;
        void           *usmDHUserPrivKeyChange;
        struct usmUser *next;
        struct usmUser *prev;
    };



    /*
     * Prototypes.
     */
    struct usmStateReference *usm_malloc_usmStateReference(void);

    void            usm_free_usmStateReference(void *old);

    int             usm_set_usmStateReference_name(struct usmStateReference
                                                   *ref, char *name,
                                                   size_t name_len);

    int             usm_set_usmStateReference_engine_id(struct
                                                        usmStateReference
                                                        *ref,
                                                        u_char * engine_id,
                                                        size_t
                                                        engine_id_len);

    int             usm_set_usmStateReference_auth_protocol(struct
                                                            usmStateReference
                                                            *ref,
                                                            oid *
                                                            auth_protocol,
                                                            size_t
                                                            auth_protocol_len);

    int             usm_set_usmStateReference_auth_key(struct
                                                       usmStateReference
                                                       *ref,
                                                       u_char * auth_key,
                                                       size_t
                                                       auth_key_len);

    int             usm_set_usmStateReference_priv_protocol(struct
                                                            usmStateReference
                                                            *ref,
                                                            oid *
                                                            priv_protocol,
                                                            size_t
                                                            priv_protocol_len);

    int             usm_set_usmStateReference_priv_key(struct
                                                       usmStateReference
                                                       *ref,
                                                       u_char * priv_key,
                                                       size_t
                                                       priv_key_len);

    int             usm_set_usmStateReference_sec_level(struct
                                                        usmStateReference
                                                        *ref,
                                                        int sec_level);
    int             usm_clone_usmStateReference(struct usmStateReference *from,
                                                    struct usmStateReference **to);


#ifdef NETSNMP_ENABLE_TESTING_CODE
    void            emergency_print(u_char * field, u_int length);
#endif

    int             asn_predict_int_length(int type, long number,
                                           size_t len);

    int             asn_predict_length(int type, u_char * ptr,
                                       size_t u_char_len);

    int             usm_set_salt(u_char * iv,
                                 size_t * iv_length,
                                 u_char * priv_salt,
                                 size_t priv_salt_length,
                                 u_char * msgSalt);

    int             usm_parse_security_parameters(u_char * secParams,
                                                  size_t remaining,
                                                  u_char * secEngineID,
                                                  size_t * secEngineIDLen,
                                                  u_int * boots_uint,
                                                  u_int * time_uint,
                                                  char *secName,
                                                  size_t * secNameLen,
                                                  u_char * signature,
                                                  size_t *
                                                  signature_length,
                                                  u_char * salt,
                                                  size_t * salt_length,
                                                  u_char ** data_ptr);

    int             usm_check_and_update_timeliness(u_char * secEngineID,
                                                    size_t secEngineIDLen,
                                                    u_int boots_uint,
                                                    u_int time_uint,
                                                    int *error);

    SecmodSessionCallback usm_open_session;
    SecmodOutMsg    usm_secmod_generate_out_msg;
    SecmodOutMsg    usm_secmod_generate_out_msg;
    SecmodInMsg     usm_secmod_process_in_msg;
    int             usm_generate_out_msg(int, u_char *, size_t, int, int,
                                         u_char *, size_t, char *, size_t,
                                         int, u_char *, size_t, void *,
                                         u_char *, size_t *, u_char **,
                                         size_t *);
    int             usm_rgenerate_out_msg(int, u_char *, size_t, int, int,
                                          u_char *, size_t, char *, size_t,
                                          int, u_char *, size_t, void *,
                                          u_char **, size_t *, size_t *);

    int             usm_process_in_msg(int, size_t, u_char *, int, int,
                                       u_char *, size_t, u_char *,
                                       size_t *, char *, size_t *,
                                       u_char **, size_t *, size_t *,
                                       void **, netsnmp_session *, u_char);

    int             usm_check_secLevel(int level, struct usmUser *user);
    NETSNMP_IMPORT
    struct usmUser *usm_get_userList(void);
    NETSNMP_IMPORT
    struct usmUser *usm_get_user(u_char * engineID, size_t engineIDLen,
                                 char *name);
    struct usmUser *usm_get_user_from_list(u_char * engineID,
                                           size_t engineIDLen, char *name,
                                           struct usmUser *userList,
                                           int use_default);
    NETSNMP_IMPORT
    struct usmUser *usm_add_user(struct usmUser *user);
    struct usmUser *usm_add_user_to_list(struct usmUser *user,
                                         struct usmUser *userList);
    NETSNMP_IMPORT
    struct usmUser *usm_free_user(struct usmUser *user);
    NETSNMP_IMPORT
    struct usmUser *usm_create_user(void);
    NETSNMP_IMPORT
    struct usmUser *usm_create_initial_user(const char *name,
                                            const oid * authProtocol,
                                            size_t authProtocolLen,
                                            const oid * privProtocol,
                                            size_t privProtocolLen);
    NETSNMP_IMPORT
    struct usmUser *usm_cloneFrom_user(struct usmUser *from,
                                       struct usmUser *to);
    NETSNMP_IMPORT
    struct usmUser *usm_remove_user(struct usmUser *user);
    struct usmUser *usm_remove_user_from_list(struct usmUser *user,
                                              struct usmUser **userList);
    char           *get_objid(char *line, oid ** optr, size_t * len);
    NETSNMP_IMPORT
    void            usm_save_users(const char *token, const char *type);
    void            usm_save_users_from_list(struct usmUser *user,
                                             const char *token,
                                             const char *type);
    void            usm_save_user(struct usmUser *user, const char *token,
                                  const char *type);
    NETSNMP_IMPORT
    SNMPCallback    usm_store_users;
    struct usmUser *usm_read_user(const char *line);
    NETSNMP_IMPORT
    void            usm_parse_config_usmUser(const char *token,
                                             char *line);

    void            usm_set_password(const char *token, char *line);
    NETSNMP_IMPORT
    void            usm_set_user_password(struct usmUser *user,
                                          const char *token, char *line);
    void            init_usm(void);
    NETSNMP_IMPORT
    void            init_usm_conf(const char *app);
    int             init_usm_post_config(int majorid, int minorid,
                                         void *serverarg, void *clientarg);
    int             deinit_usm_post_config(int majorid, int minorid, void *serverarg,
					   void *clientarg);
    NETSNMP_IMPORT
    void            clear_user_list(void);
    NETSNMP_IMPORT
    void            shutdown_usm(void);

    NETSNMP_IMPORT
    int             usm_create_user_from_session(netsnmp_session * session);
    SecmodPostDiscovery usm_create_user_from_session_hook;
    NETSNMP_IMPORT
    void            usm_parse_create_usmUser(const char *token,
                                             char *line);
    NETSNMP_IMPORT
    const oid      *get_default_authtype(size_t *);
    NETSNMP_IMPORT
    const oid      *get_default_privtype(size_t *);
    void            snmpv3_authtype_conf(const char *word, char *cptr);
    void            snmpv3_privtype_conf(const char *word, char *cptr);

#ifdef __cplusplus
}
#endif
#endif                          /* SNMPUSM_H */
