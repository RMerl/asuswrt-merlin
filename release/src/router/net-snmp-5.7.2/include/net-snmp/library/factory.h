#ifndef NETSNMP_FACTORY_H
#define NETSNMP_FACTORY_H


#ifdef __cplusplus
extern "C" {
#endif

    typedef void * (netsnmp_factory_produce_f)(void);
    typedef int (netsnmp_factory_produce_noalloc_f)(void *);

    typedef struct netsnmp_factory_s {
        /*
         * a string describing the product the factory creates
         */
        const char                           *product;

        /*
         * a function to create an object in newly allcoated memory
         */
        netsnmp_factory_produce_f            *produce;

        /*
         * a function to create an object in previously allcoated memory
         */
        netsnmp_factory_produce_noalloc_f    *produce_noalloc;

    } netsnmp_factory;

    /*
     * init factory registry
     */
    void netsnmp_factory_init(void);

    /*
     * register a factory type
     */
    int  netsnmp_factory_register(netsnmp_factory *f);

    /*
     * get a factory
     */
    netsnmp_factory* netsnmp_factory_get(const char* product);

    /*
     * ask a factory to produce an object
     */
    void * netsnmp_factory_produce(const char* product);

    /*
     * ask a factory to produce an object in the provided memory
     */
    int netsnmp_factory_produce_noalloc(const char *product, void *memory);

    /*
     * factory return codes
     */
    enum {
        FACTORY_NOERROR = 0,
        FACTORY_EXISTS,
        FACTORY_NOTFOUND,
        FACTORY_NOMEMORY,
        FACTORY_GENERR,
        FACTORY_MAXIMUM_ERROR
    };

#ifdef __cplusplus
}
#endif


#endif /* NETSNMP_FACTORY_H */
