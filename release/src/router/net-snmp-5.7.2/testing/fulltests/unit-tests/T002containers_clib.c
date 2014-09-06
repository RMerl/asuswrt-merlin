/* HEADER Testing the container API */

netsnmp_container *container;
void *p;

init_snmp("container-test");
container = netsnmp_container_find("fifo");
container->compare = (netsnmp_container_compare*) strcmp;

CONTAINER_INSERT(container, "foo");
CONTAINER_INSERT(container, "bar");
CONTAINER_INSERT(container, "baz");

OK(CONTAINER_FIND(container, "bar") != NULL,
   "should be able to find the stored 'bar' string");

OK(CONTAINER_FIND(container, "foobar") == NULL,
   "shouldn't be able to find the (not) stored 'foobar' string");

OK(CONTAINER_SIZE(container) == 3,
   "container has the proper size for the elements we've added");

CONTAINER_REMOVE(container, "bar");

OK(CONTAINER_FIND(container, "bar") == NULL,
   "should no longer be able to find the (reoved) 'bar' string");

OK(CONTAINER_SIZE(container) == 2,
   "container has the proper size for the elements after a removal");

while ((p = CONTAINER_FIRST(container)))
  CONTAINER_REMOVE(container, p);
CONTAINER_FREE(container);

snmp_shutdown("container-test");
