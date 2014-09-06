/*
 * Wrapper for the full DisMan implementation
 */
config_require(disman/event-mib)
config_require(disman/expression-mib)
#ifndef NETSNMP_NO_WRITE_SUPPORT
/* the schedule mib is all about writing (SETs) */ 
config_require(disman/schedule)
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
/* config_require(disman/nslookup-mib)   */
/* config_require(disman/ping-mib)       */
/* config_require(disman/traceroute-mib) */
