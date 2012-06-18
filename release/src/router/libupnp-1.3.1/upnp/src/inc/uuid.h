  /*
  ** Copyright (c) 1990- 1993, 1996 Open Software Foundation, Inc.
  ** Copyright (c) 1989 by Hewlett-Packard Company, Palo Alto, Ca. &
  ** Digital Equipment Corporation, Maynard, Mass.
  ** Copyright (c) 1998 Microsoft.
  ** To anyone who acknowledges that this file is provided "AS IS"
  ** without any express or implied warranty: permission to use, copy,
  ** modify, and distribute this file for any purpose is hereby
  ** granted without fee, provided that the above copyright notices and
  ** this notice appears in all source code copies, and that none of
  ** the names of Open Software Foundation, Inc., Hewlett-Packard
  ** Company, or Digital Equipment Corporation be used in advertising
  ** or publicity pertaining to distribution of the software without
  ** specific, written prior permission.  Neither Open Software
  ** Foundation, Inc., Hewlett-Packard Company, Microsoft, nor Digital Equipment
  ** Corporation makes any representations about the suitability of
  ** this software for any purpose.
  */

  typedef struct _uuid_upnp {
      unsigned32          time_low;
      unsigned16          time_mid;
      unsigned16          time_hi_and_version;
      unsigned8           clock_seq_hi_and_reserved;
      unsigned8           clock_seq_low;
      byte                node[6];
  } uuid_upnp;

  /* uuid_create -- generate a UUID */
  int uuid_create(uuid_upnp * id);
  void uuid_unpack(uuid_upnp *u, char *out);	// out will be xxxx-xx-xx-xx-xxxxxx format

  /* uuid_create_from_name -- create a UUID using a "name"
     from a "name space" */
  void uuid_create_from_name(
    uuid_upnp * uid,        /* resulting UUID */
    uuid_upnp nsid,          /* UUID to serve as context, so identical
                             names from different name spaces generate
                             different UUIDs */
    void * name,          /* the name from which to generate a UUID */
    int namelen           /* the length of the name */
  );

  /* uuid_compare --  Compare two UUID's "lexically" and return
          -1   u1 is lexically before u2
           0   u1 is equal to u2
           1   u1 is lexically after u2
     Note:   lexical ordering is not temporal ordering!
  */
  int uuid_compare(uuid_upnp *u1, uuid_upnp *u2);
