
#include <zebra.h>
#include <math.h>

#include "log.h"
#include "memory.h"
#include "prefix.h"
#include "table.h"
#include "command.h"
#include "vty.h"
#include "thread.h"
#include "linklist.h"
#include "if.h"

#include "ospf6_proto.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"
#include "ospf6_route.h"
#include "ospf6_spf.h"

#include "ospf6_top.h"
#include "ospf6_area.h"
#include "ospf6_interface.h"
#include "ospf6_intra.h"

extern char *malloc_options;

struct thread_master *master;

#define NUMBER_OF_TRIALS 1000000
int N = 4;

#define ROW(n) ((n) % N)
#define COL(n) ((n) / N)

#define IS_TOPOF(n) (COL(n) > 0)
#define IS_BOTTOMOF(n) (COL(n) < N - 1)
#define IS_LEFTOF(n) (ROW(n) > 0)
#define IS_RIGHTOF(n) (ROW(n) < N - 1)
#define TOPOF(n) (n - N)
#define BOTTOMOF(n) (n + N)
#define LEFTOF(n) (n - 1)
#define RIGHTOF(n) (n + 1)

#define IFINDEX_TOP    0
#define IFINDEX_BOTTOM 1
#define IFINDEX_LEFT   2
#define IFINDEX_RIGHT  3

struct in6_addr topaddr =
{{{ 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, IFINDEX_TOP }}};
struct in6_addr leftaddr =
{{{ 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, IFINDEX_LEFT }}};
struct in6_addr rightaddr =
{{{ 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, IFINDEX_RIGHT }}};
struct in6_addr bottomaddr =
{{{ 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, IFINDEX_BOTTOM }}};

void
install_router_lsa (u_int32_t router_id, struct ospf6_lsdb *lsdb)
{
  char buffer[OSPF6_MAX_LSASIZE];
  struct ospf6_lsa_header *lsa_header;
  struct ospf6_lsa *lsa;
  struct ospf6_router_lsa *router_lsa;
  struct ospf6_router_lsdesc *lsdesc;

  //printf ("install %d\n", router_id);

  lsa_header = (struct ospf6_lsa_header *) buffer;
  router_lsa = (struct ospf6_router_lsa *)
    ((caddr_t) lsa_header + sizeof (struct ospf6_lsa_header));
  lsdesc = (struct ospf6_router_lsdesc *)
    ((caddr_t) router_lsa + sizeof (struct ospf6_router_lsa));

  if (IS_TOPOF (router_id))
    {
      lsdesc->type = OSPF6_ROUTER_LSDESC_POINTTOPOINT;
      lsdesc->metric = htons (1);
      lsdesc->interface_id = htonl (IFINDEX_TOP);
      lsdesc->neighbor_interface_id = htonl (IFINDEX_BOTTOM);
      lsdesc->neighbor_router_id = htonl (TOPOF (router_id));
      lsdesc++;
    }
  if (IS_LEFTOF (router_id))
    {
      lsdesc->type = OSPF6_ROUTER_LSDESC_POINTTOPOINT;
      lsdesc->metric = htons (1);
      lsdesc->interface_id = htonl (IFINDEX_LEFT);
      lsdesc->neighbor_interface_id = htonl (IFINDEX_RIGHT);
      lsdesc->neighbor_router_id = htonl (LEFTOF (router_id));
      lsdesc++;
    }
  if (IS_RIGHTOF (router_id))
    {
      lsdesc->type = OSPF6_ROUTER_LSDESC_POINTTOPOINT;
      lsdesc->metric = htons (1);
      lsdesc->interface_id = htonl (IFINDEX_RIGHT);
      lsdesc->neighbor_interface_id = htonl (IFINDEX_LEFT);
      lsdesc->neighbor_router_id = htonl (RIGHTOF (router_id));
      lsdesc++;
    }
  if (IS_BOTTOMOF (router_id))
    {
      lsdesc->type = OSPF6_ROUTER_LSDESC_POINTTOPOINT;
      lsdesc->metric = htons (1);
      lsdesc->interface_id = htonl (IFINDEX_BOTTOM);
      lsdesc->neighbor_interface_id = htonl (IFINDEX_TOP);
      lsdesc->neighbor_router_id = htonl (BOTTOMOF (router_id));
      lsdesc++;
    }

  OSPF6_OPT_SET (router_lsa->options, OSPF6_OPT_V6);
  OSPF6_OPT_SET (router_lsa->options, OSPF6_OPT_E);
  OSPF6_OPT_CLEAR (router_lsa->options, OSPF6_OPT_MC);
  OSPF6_OPT_CLEAR (router_lsa->options, OSPF6_OPT_N);
  OSPF6_OPT_SET (router_lsa->options, OSPF6_OPT_R);
  OSPF6_OPT_CLEAR (router_lsa->options, OSPF6_OPT_DC);
  UNSET_FLAG (router_lsa->bits, OSPF6_ROUTER_BIT_B);
  UNSET_FLAG (router_lsa->bits, OSPF6_ROUTER_BIT_E);
  UNSET_FLAG (router_lsa->bits, OSPF6_ROUTER_BIT_V);
  UNSET_FLAG (router_lsa->bits, OSPF6_ROUTER_BIT_W);

  lsa_header->age = 0;
  lsa_header->type = htons (OSPF6_LSTYPE_ROUTER);
  lsa_header->id = htonl (0);
  lsa_header->adv_router = htonl (router_id);
  lsa_header->seqnum = htonl (INITIAL_SEQUENCE_NUMBER);
  lsa_header->length = htons ((caddr_t) lsdesc - (caddr_t) buffer);

  lsa = ospf6_lsa_create (lsa_header);
  ospf6_lsdb_add (lsa, lsdb);
}

static double
diffsec (struct timeval *end, struct timeval *start)
{
  double time;
  time = (end->tv_sec - start->tv_sec) * 1.0 +
         (end->tv_usec - start->tv_usec) / 1000000.0;
  if (time < 0.0)
    fprintf (stderr, "time: %f end{%010lu,%010lu} start{%010lu,%010lu}\n",
             time, end->tv_sec, end->tv_usec, start->tv_sec, start->tv_usec);
  return time;
}

void
lsdb_lookup_try (int ntry, struct ospf6_lsdb *lsdb)
{
  struct ospf6_lsa *lsa;
  struct timeval start, now;
  double time, timesum, timesumsq;
  double max, min, avg, variance, std;
  int rets, retn;
  int i;
  u_int32_t router_id;

  max = 0.0;
  min = 999999999.0;
  timesum = 0.0;
  timesumsq = 0.0;
  for (i = 0; i < ntry; i++)
    {
      router_id = random () % (N * N);

      memset (&start, 0, sizeof (&start));
      memset (&now, 0, sizeof (&now));

      lsa = NULL;
      rets = gettimeofday (&start, NULL);
      lsa = ospf6_lsdb_lookup (htons (OSPF6_LSTYPE_ROUTER), htonl (0),
                               htonl (router_id), lsdb);
      retn = gettimeofday (&now, NULL);
      assert (lsa);

      if (rets != 0 || retn != 0)
        {
          fprintf (stderr, "gettimeofday failed: %s\n", strerror (errno));
          i--;
          continue;
        }

      time = diffsec (&now, &start);
      timesum += time;
      timesumsq += time * time;
      max = (time > max ? time : max);
      min = (time < min ? time : min);
    }

  avg = timesum / ntry;
  variance = (timesumsq - ntry * avg * avg) / ntry;
  std = sqrt (variance);

  printf ("test lsdb: #LSAs: %d #lookup: %d min: %.3f avg: %.3f max: %.3f stddev: %.3f (ms)\n",
          N * N, ntry, min * 1000.0, avg * 1000.0, max * 1000.0, std * 1000.0);
}

void
install_link_lsdb (u_int32_t router_id, int ifindex)
{
  struct interface *ifp;
  struct ospf6_interface *oi;
  char *ifname = NULL;
  u_int32_t peer = 0;
  int peer_ifindex = 0;
  char buffer[OSPF6_MAX_LSASIZE];
  struct ospf6_lsa_header *lsa_header;
  struct ospf6_link_lsa *link_lsa;
  struct ospf6_lsa *lsa;
  char *p;

  if (ifindex == IFINDEX_TOP && IS_TOPOF (router_id))
    {
      ifname = "top";
      peer = TOPOF (router_id);
      peer_ifindex = IFINDEX_BOTTOM;
    }
  else if (ifindex == IFINDEX_LEFT && IS_LEFTOF (router_id))
    {
      ifname = "left";
      peer = LEFTOF (router_id);
      peer_ifindex = IFINDEX_RIGHT;
    }
  else if (ifindex == IFINDEX_RIGHT && IS_RIGHTOF (router_id))
    {
      ifname = "right";
      peer = RIGHTOF (router_id);
      peer_ifindex = IFINDEX_LEFT;
    }
  else if (ifindex == IFINDEX_BOTTOM && IS_BOTTOMOF (router_id))
    {
      ifname = "bottom";
      peer = BOTTOMOF (router_id);
      peer_ifindex = IFINDEX_TOP;
    }
  else
    assert (0);

  ifp = if_get_by_name (ifname);
  ifp->ifindex = ifindex;

  oi = malloc (sizeof (struct ospf6_interface));
  oi->interface = ifp;
  ifp->info = oi;

  oi->lsdb = ospf6_lsdb_create (oi);

  memset (buffer, 0, sizeof (buffer));
  lsa_header = (struct ospf6_lsa_header *) buffer;
  link_lsa = (struct ospf6_link_lsa *)
    ((caddr_t) lsa_header + sizeof (struct ospf6_lsa_header));
  p = ((caddr_t) link_lsa + sizeof (struct ospf6_link_lsa));

  link_lsa->priority = 1;
  link_lsa->options[0] = 0;
  link_lsa->options[1] = 0;
  link_lsa->options[2] = 0;
  if (ifindex == IFINDEX_TOP)
    link_lsa->linklocal_addr = topaddr;
  else if (ifindex == IFINDEX_LEFT)
    link_lsa->linklocal_addr = leftaddr;
  else if (ifindex == IFINDEX_RIGHT)
    link_lsa->linklocal_addr = rightaddr;
  else
    link_lsa->linklocal_addr = bottomaddr;
  link_lsa->prefix_num = htonl (0);

  lsa_header->age = 0;
  lsa_header->type = htons (OSPF6_LSTYPE_LINK);
  lsa_header->id = htonl (ifindex);
  lsa_header->adv_router = htonl (router_id);
  lsa_header->seqnum = htonl (INITIAL_SEQUENCE_NUMBER);
  lsa_header->length = htons ((caddr_t) p - (caddr_t) buffer);

  lsa = ospf6_lsa_create (lsa_header);
  ospf6_lsdb_add (lsa, oi->lsdb);

  /* peer's */
  memset (buffer, 0, sizeof (buffer));
  lsa_header = (struct ospf6_lsa_header *) buffer;
  link_lsa = (struct ospf6_link_lsa *)
    ((caddr_t) lsa_header + sizeof (struct ospf6_lsa_header));
  p = ((caddr_t) link_lsa + sizeof (struct ospf6_link_lsa));

  link_lsa->priority = 1;
  link_lsa->options[0] = 0;
  link_lsa->options[1] = 0;
  link_lsa->options[2] = 0;
  if (peer_ifindex == IFINDEX_TOP)
    link_lsa->linklocal_addr = topaddr;
  else if (peer_ifindex == IFINDEX_LEFT)
    link_lsa->linklocal_addr = leftaddr;
  else if (peer_ifindex == IFINDEX_RIGHT)
    link_lsa->linklocal_addr = rightaddr;
  else
    link_lsa->linklocal_addr = bottomaddr;
  link_lsa->prefix_num = htonl (0);

  lsa_header->age = 0;
  lsa_header->type = htons (OSPF6_LSTYPE_LINK);
  lsa_header->id = htonl (peer_ifindex);
  lsa_header->adv_router = htonl (peer);
  lsa_header->seqnum = htonl (INITIAL_SEQUENCE_NUMBER);
  lsa_header->length = htons ((caddr_t) p - (caddr_t) buffer);

  lsa = ospf6_lsa_create (lsa_header);
  ospf6_lsdb_add (lsa, oi->lsdb);

  return;
}

void
spf_try (u_int32_t router_id, struct ospf6_lsdb *lsdb)
{
  struct ospf6_area *oa;
  struct ospf6_interface *oi;
  struct timeval start, end;
  double time;
  int rets, rete;
  listnode node;

  oa = malloc (sizeof (struct ospf6_area));
  snprintf (oa->name, sizeof (oa->name), "spf_test");
  oa->lsdb = lsdb;
  oa->spf_table = ospf6_route_table_create (); 

  oa->if_list = list_new ();
  if (IS_TOPOF (router_id))
    {
      install_link_lsdb (router_id, IFINDEX_TOP);
      oi = ospf6_interface_lookup_by_ifindex (IFINDEX_TOP);
      listnode_add (oa->if_list, oi);
    }
  if (IS_LEFTOF (router_id))
    {
      install_link_lsdb (router_id, IFINDEX_LEFT);
      oi = ospf6_interface_lookup_by_ifindex (IFINDEX_LEFT);
      listnode_add (oa->if_list, oi);
    }
  if (IS_RIGHTOF (router_id))
    {
      install_link_lsdb (router_id, IFINDEX_RIGHT);
      oi = ospf6_interface_lookup_by_ifindex (IFINDEX_RIGHT);
      listnode_add (oa->if_list, oi);
    }
  if (IS_BOTTOMOF (router_id))
    {
      install_link_lsdb (router_id, IFINDEX_BOTTOM);
      oi = ospf6_interface_lookup_by_ifindex (IFINDEX_BOTTOM);
      listnode_add (oa->if_list, oi);
    }

  rets = gettimeofday (&start, (struct timezone *) NULL);
  ospf6_spf_calculation (htonl (router_id), oa->spf_table, oa);
  rete = gettimeofday (&end, (struct timezone *) NULL);

  if (rets != 0 || rete != 0)
    fprintf (stderr, "gettimeofday failed: %s\n", strerror (errno));
  time = diffsec (&end, &start);

  printf ("test spf: root: %lu #LSAs: %d #routes: %d time: %.3f (ms)\n",
          (unsigned long) router_id, N * N, oa->spf_table->count,
          time * 1000.0);

  for (node = listhead (oa->if_list); node; nextnode (node))
    {
      oi = (struct ospf6_interface *) getdata (node);
      ospf6_lsdb_delete (oi->lsdb);
      oi->interface->info = NULL;
      free (oi);
    }
  list_delete (oa->if_list);

  ospf6_route_table_delete (oa->spf_table);
  free (oa);
}

int
main (int argc, char **argv)
{
  struct ospf6_lsdb *lsdb;
  u_int32_t router_id;
  struct timeval start, now;
  double timeadd;

#if 0
  malloc_options = "X";
#endif

  if (argc > 1)
    N = atoi (argv[1]);

  lsdb = ospf6_lsdb_create (NULL);

  fprintf (stderr, "Installing %d virtual Router-LSA ... ", N * N);
  fflush (stdout);
  gettimeofday (&start, NULL);
  for (router_id = 0; router_id < N * N; router_id++)
    install_router_lsa (router_id, lsdb);
  gettimeofday (&now, NULL);
  timeadd = diffsec (&now, &start);
  fprintf (stderr, "done (%.3f ms) \n", timeadd * 1000.0);

  gettimeofday (&now, NULL);
  srandom (now.tv_sec);

  //lsdb_lookup_try (NUMBER_OF_TRIALS, lsdb);

  cmd_init (0);
  vty_init ();
  if_init ();
  zlog_default = openzlog ("ospf6_test", ZLOG_STDERR, ZLOG_OSPF6,
                           LOG_CONS|LOG_NDELAY|LOG_PERROR|LOG_PID,
                           LOG_DAEMON);

  OSPF6_DEBUG_SPF_OFF (OSPF6_DEBUG_SPF_PROCESS);
  spf_try (0, lsdb);
  spf_try ((N / 2) * (N + 1), lsdb);

  return 0;
}


