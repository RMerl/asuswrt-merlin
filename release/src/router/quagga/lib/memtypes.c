/*
 * Memory type definitions. This file is parsed by memtypes.awk to extract
 * MTYPE_ and memory_list_.. information in order to autogenerate 
 * memtypes.h.
 *
 * The script is sensitive to the format (though not whitespace), see
 * the top of memtypes.awk for more details.
 */

#include "zebra.h"
#include "memory.h"

struct memory_list memory_list_lib[] =
{
  { MTYPE_TMP,			"Temporary memory"		},
  { MTYPE_STRVEC,		"String vector"			},
  { MTYPE_VECTOR,		"Vector"			},
  { MTYPE_VECTOR_INDEX,		"Vector index"			},
  { MTYPE_LINK_LIST,		"Link List"			},
  { MTYPE_LINK_NODE,		"Link Node"			},
  { MTYPE_THREAD,		"Thread"			},
  { MTYPE_THREAD_MASTER,	"Thread master"			},
  { MTYPE_THREAD_STATS,		"Thread stats"			},
  { MTYPE_VTY,			"VTY"				},
  { MTYPE_VTY_OUT_BUF,		"VTY output buffer"		},
  { MTYPE_VTY_HIST,		"VTY history"			},
  { MTYPE_IF,			"Interface"			},
  { MTYPE_CONNECTED,		"Connected" 			},
  { MTYPE_CONNECTED_LABEL,	"Connected interface label"	},
  { MTYPE_BUFFER,		"Buffer"			},
  { MTYPE_BUFFER_DATA,		"Buffer data"			},
  { MTYPE_STREAM,		"Stream"			},
  { MTYPE_STREAM_DATA,		"Stream data"			},
  { MTYPE_STREAM_FIFO,		"Stream FIFO"			},
  { MTYPE_PREFIX,		"Prefix"			},
  { MTYPE_PREFIX_IPV4,		"Prefix IPv4"			},
  { MTYPE_PREFIX_IPV6,		"Prefix IPv6"			},
  { MTYPE_HASH,			"Hash"				},
  { MTYPE_HASH_BACKET,		"Hash Bucket"			},
  { MTYPE_HASH_INDEX,		"Hash Index"			},
  { MTYPE_ROUTE_TABLE,		"Route table"			},
  { MTYPE_ROUTE_NODE,		"Route node"			},
  { MTYPE_DISTRIBUTE,		"Distribute list"		},
  { MTYPE_DISTRIBUTE_IFNAME,	"Dist-list ifname"		},
  { MTYPE_ACCESS_LIST,		"Access List"			},
  { MTYPE_ACCESS_LIST_STR,	"Access List Str"		},
  { MTYPE_ACCESS_FILTER,	"Access Filter"			},
  { MTYPE_PREFIX_LIST,		"Prefix List"			},
  { MTYPE_PREFIX_LIST_ENTRY,	"Prefix List Entry"		},
  { MTYPE_PREFIX_LIST_STR,	"Prefix List Str"		},
  { MTYPE_ROUTE_MAP,		"Route map"			},
  { MTYPE_ROUTE_MAP_NAME,	"Route map name"		},
  { MTYPE_ROUTE_MAP_INDEX,	"Route map index"		},
  { MTYPE_ROUTE_MAP_RULE,	"Route map rule"		},
  { MTYPE_ROUTE_MAP_RULE_STR,	"Route map rule str"		},
  { MTYPE_ROUTE_MAP_COMPILED,	"Route map compiled"		},
  { MTYPE_CMD_TOKENS,		"Command desc"			},
  { MTYPE_KEY,			"Key"				},
  { MTYPE_KEYCHAIN,		"Key chain"			},
  { MTYPE_IF_RMAP,		"Interface route map"		},
  { MTYPE_IF_RMAP_NAME,		"I.f. route map name",		},
  { MTYPE_SOCKUNION,		"Socket union"			},
  { MTYPE_PRIVS,		"Privilege information"		},
  { MTYPE_ZLOG,			"Logging"			},
  { MTYPE_ZCLIENT,		"Zclient"			},
  { MTYPE_WORK_QUEUE,		"Work queue"			},
  { MTYPE_WORK_QUEUE_ITEM,	"Work queue item"		},
  { MTYPE_WORK_QUEUE_NAME,	"Work queue name string"	},
  { MTYPE_PQUEUE,		"Priority queue"		},
  { MTYPE_PQUEUE_DATA,		"Priority queue data"		},
  { MTYPE_HOST,			"Host config"			},
  { -1, NULL },
};

struct memory_list memory_list_zebra[] = 
{
  { MTYPE_RTADV_PREFIX,		"Router Advertisement Prefix"	},
  { MTYPE_VRF,			"VRF"				},
  { MTYPE_VRF_NAME,		"VRF name"			},
  { MTYPE_NEXTHOP,		"Nexthop"			},
  { MTYPE_RIB,			"RIB"				},
  { MTYPE_RIB_QUEUE,		"RIB process work queue"	},
  { MTYPE_STATIC_IPV4,		"Static IPv4 route"		},
  { MTYPE_STATIC_IPV6,		"Static IPv6 route"		},
  { MTYPE_RIB_DEST,		"RIB destination"		},
  { MTYPE_RIB_TABLE_INFO,	"RIB table info"		},
  { -1, NULL },
};

struct memory_list memory_list_bgp[] =
{
  { MTYPE_BGP,			"BGP instance"			},
  { MTYPE_BGP_LISTENER,		"BGP listen socket details"	},
  { MTYPE_BGP_PEER,		"BGP peer"			},
  { MTYPE_BGP_PEER_HOST,	"BGP peer hostname"		},
  { MTYPE_PEER_GROUP,		"Peer group"			},
  { MTYPE_PEER_DESC,		"Peer description"		},
  { MTYPE_PEER_PASSWORD,	"Peer password string"		},
  { MTYPE_ATTR,			"BGP attribute"			},
  { MTYPE_ATTR_EXTRA,		"BGP extra attributes"		},
  { MTYPE_AS_PATH,		"BGP aspath"			},
  { MTYPE_AS_SEG,		"BGP aspath seg"		},
  { MTYPE_AS_SEG_DATA,		"BGP aspath segment data"	},
  { MTYPE_AS_STR,		"BGP aspath str"		},
  { 0, NULL },
  { MTYPE_BGP_TABLE,		"BGP table"			},
  { MTYPE_BGP_NODE,		"BGP node"			},
  { MTYPE_BGP_ROUTE,		"BGP route"			},
  { MTYPE_BGP_ROUTE_EXTRA,	"BGP ancillary route info"	},
  { MTYPE_BGP_CONN,		"BGP connected"			},
  { MTYPE_BGP_STATIC,		"BGP static"			},
  { MTYPE_BGP_ADVERTISE_ATTR,	"BGP adv attr"			},
  { MTYPE_BGP_ADVERTISE,	"BGP adv"			},
  { MTYPE_BGP_SYNCHRONISE,	"BGP synchronise"		},
  { MTYPE_BGP_ADJ_IN,		"BGP adj in"			},
  { MTYPE_BGP_ADJ_OUT,		"BGP adj out"			},
  { MTYPE_BGP_MPATH_INFO,	"BGP multipath info"		},
  { 0, NULL },
  { MTYPE_AS_LIST,		"BGP AS list"			},
  { MTYPE_AS_FILTER,		"BGP AS filter"			},
  { MTYPE_AS_FILTER_STR,	"BGP AS filter str"		},
  { 0, NULL },
  { MTYPE_COMMUNITY,		"community"			},
  { MTYPE_COMMUNITY_VAL,	"community val"			},
  { MTYPE_COMMUNITY_STR,	"community str"			},
  { 0, NULL },
  { MTYPE_ECOMMUNITY,		"extcommunity"			},
  { MTYPE_ECOMMUNITY_VAL,	"extcommunity val"		},
  { MTYPE_ECOMMUNITY_STR,	"extcommunity str"		},
  { 0, NULL },
  { MTYPE_COMMUNITY_LIST,	"community-list"		},
  { MTYPE_COMMUNITY_LIST_NAME,	"community-list name"		},
  { MTYPE_COMMUNITY_LIST_ENTRY,	"community-list entry"		},
  { MTYPE_COMMUNITY_LIST_CONFIG,  "community-list config"	},
  { MTYPE_COMMUNITY_LIST_HANDLER, "community-list handler"	},
  { 0, NULL },
  { MTYPE_CLUSTER,		"Cluster list"			},
  { MTYPE_CLUSTER_VAL,		"Cluster list val"		},
  { 0, NULL },
  { MTYPE_BGP_PROCESS_QUEUE,	"BGP Process queue"		},
  { MTYPE_BGP_CLEAR_NODE_QUEUE, "BGP node clear queue"		},
  { 0, NULL },
  { MTYPE_TRANSIT,		"BGP transit attr"		},
  { MTYPE_TRANSIT_VAL,		"BGP transit val"		},
  { 0, NULL },
  { MTYPE_BGP_DISTANCE,		"BGP distance"			},
  { MTYPE_BGP_NEXTHOP_CACHE,	"BGP nexthop"			},
  { MTYPE_BGP_CONFED_LIST,	"BGP confed list"		},
  { MTYPE_PEER_UPDATE_SOURCE,	"BGP peer update interface"	},
  { MTYPE_BGP_DAMP_INFO,	"Dampening info"		},
  { MTYPE_BGP_DAMP_ARRAY,	"BGP Dampening array"		},
  { MTYPE_BGP_REGEXP,		"BGP regexp"			},
  { MTYPE_BGP_AGGREGATE,	"BGP aggregate"			},
  { MTYPE_BGP_ADDR,		"BGP own address"		},
  { -1, NULL }
};

struct memory_list memory_list_rip[] =
{
  { MTYPE_RIP,                "RIP structure"			},
  { MTYPE_RIP_INFO,           "RIP route info"			},
  { MTYPE_RIP_INTERFACE,      "RIP interface"			},
  { MTYPE_RIP_PEER,           "RIP peer"			},
  { MTYPE_RIP_OFFSET_LIST,    "RIP offset list"			},
  { MTYPE_RIP_DISTANCE,       "RIP distance"			},
  { -1, NULL }
};

struct memory_list memory_list_ripng[] =
{
  { MTYPE_RIPNG,              "RIPng structure"			},
  { MTYPE_RIPNG_ROUTE,        "RIPng route info"		},
  { MTYPE_RIPNG_AGGREGATE,    "RIPng aggregate"			},
  { MTYPE_RIPNG_PEER,         "RIPng peer"			},
  { MTYPE_RIPNG_OFFSET_LIST,  "RIPng offset lst"		},
  { MTYPE_RIPNG_RTE_DATA,     "RIPng rte data"			},
  { -1, NULL }
};

struct memory_list memory_list_babel[] =
{
  { MTYPE_BABEL,              "Babel structure"			},
  { MTYPE_BABEL_IF,           "Babel interface"			},
  { -1, NULL }
};

struct memory_list memory_list_ospf[] =
{
  { MTYPE_OSPF_TOP,           "OSPF top"			},
  { MTYPE_OSPF_AREA,          "OSPF area"			},
  { MTYPE_OSPF_AREA_RANGE,    "OSPF area range"			},
  { MTYPE_OSPF_NETWORK,       "OSPF network"			},
  { MTYPE_OSPF_NEIGHBOR_STATIC,"OSPF static nbr"		},
  { MTYPE_OSPF_IF,            "OSPF interface"			},
  { MTYPE_OSPF_NEIGHBOR,      "OSPF neighbor"			},
  { MTYPE_OSPF_ROUTE,         "OSPF route"			},
  { MTYPE_OSPF_TMP,           "OSPF tmp mem"			},
  { MTYPE_OSPF_LSA,           "OSPF LSA"			},
  { MTYPE_OSPF_LSA_DATA,      "OSPF LSA data"			},
  { MTYPE_OSPF_LSDB,          "OSPF LSDB"			},
  { MTYPE_OSPF_PACKET,        "OSPF packet"			},
  { MTYPE_OSPF_FIFO,          "OSPF FIFO queue"			},
  { MTYPE_OSPF_VERTEX,        "OSPF vertex"			},
  { MTYPE_OSPF_VERTEX_PARENT, "OSPF vertex parent",		},
  { MTYPE_OSPF_NEXTHOP,       "OSPF nexthop"			},
  { MTYPE_OSPF_PATH,	      "OSPF path"			},
  { MTYPE_OSPF_VL_DATA,       "OSPF VL data"			},
  { MTYPE_OSPF_CRYPT_KEY,     "OSPF crypt key"			},
  { MTYPE_OSPF_EXTERNAL_INFO, "OSPF ext. info"			},
  { MTYPE_OSPF_DISTANCE,      "OSPF distance"			},
  { MTYPE_OSPF_IF_INFO,       "OSPF if info"			},
  { MTYPE_OSPF_IF_PARAMS,     "OSPF if params"			},
  { MTYPE_OSPF_MESSAGE,		"OSPF message"			},
  { -1, NULL },
};

struct memory_list memory_list_ospf6[] =
{
  { MTYPE_OSPF6_TOP,          "OSPF6 top"			},
  { MTYPE_OSPF6_AREA,         "OSPF6 area"			},
  { MTYPE_OSPF6_IF,           "OSPF6 interface"			},
  { MTYPE_OSPF6_NEIGHBOR,     "OSPF6 neighbor"			},
  { MTYPE_OSPF6_ROUTE,        "OSPF6 route"			},
  { MTYPE_OSPF6_PREFIX,       "OSPF6 prefix"			},
  { MTYPE_OSPF6_MESSAGE,      "OSPF6 message"			},
  { MTYPE_OSPF6_LSA,          "OSPF6 LSA"			},
  { MTYPE_OSPF6_LSA_SUMMARY,  "OSPF6 LSA summary"		},
  { MTYPE_OSPF6_LSDB,         "OSPF6 LSA database"		},
  { MTYPE_OSPF6_VERTEX,       "OSPF6 vertex"			},
  { MTYPE_OSPF6_SPFTREE,      "OSPF6 SPF tree"			},
  { MTYPE_OSPF6_NEXTHOP,      "OSPF6 nexthop"			},
  { MTYPE_OSPF6_EXTERNAL_INFO,"OSPF6 ext. info"			},
  { MTYPE_OSPF6_OTHER,        "OSPF6 other"			},
  { -1, NULL },
};

struct memory_list memory_list_isis[] =
{
  { MTYPE_ISIS,               "ISIS"				},
  { MTYPE_ISIS_TMP,           "ISIS TMP"			},
  { MTYPE_ISIS_CIRCUIT,       "ISIS circuit"			},
  { MTYPE_ISIS_LSP,           "ISIS LSP"			},
  { MTYPE_ISIS_ADJACENCY,     "ISIS adjacency"			},
  { MTYPE_ISIS_AREA,          "ISIS area"			},
  { MTYPE_ISIS_AREA_ADDR,     "ISIS area address"		},
  { MTYPE_ISIS_TLV,           "ISIS TLV"			},
  { MTYPE_ISIS_DYNHN,         "ISIS dyn hostname"		},
  { MTYPE_ISIS_SPFTREE,       "ISIS SPFtree"			},
  { MTYPE_ISIS_VERTEX,        "ISIS vertex"			},
  { MTYPE_ISIS_ROUTE_INFO,    "ISIS route info"			},
  { MTYPE_ISIS_NEXTHOP,       "ISIS nexthop"			},
  { MTYPE_ISIS_NEXTHOP6,      "ISIS nexthop6"			},
  { MTYPE_ISIS_DICT,          "ISIS dictionary"			},
  { MTYPE_ISIS_DICT_NODE,     "ISIS dictionary node"		},
  { -1, NULL },
};

struct memory_list memory_list_pim[] =
{
  { MTYPE_PIM_CHANNEL_OIL,       "PIM SSM (S,G) channel OIL"      },
  { MTYPE_PIM_INTERFACE,         "PIM interface"	          },
  { MTYPE_PIM_IGMP_JOIN,         "PIM interface IGMP static join" },
  { MTYPE_PIM_IGMP_SOCKET,       "PIM interface IGMP socket"      },
  { MTYPE_PIM_IGMP_GROUP,        "PIM interface IGMP group"       },
  { MTYPE_PIM_IGMP_GROUP_SOURCE, "PIM interface IGMP source"      },
  { MTYPE_PIM_NEIGHBOR,          "PIM interface neighbor"         },
  { MTYPE_PIM_IFCHANNEL,         "PIM interface (S,G) state"      },
  { MTYPE_PIM_UPSTREAM,          "PIM upstream (S,G) state"       },
  { MTYPE_PIM_SSMPINGD,          "PIM sspimgd socket"             },
  { -1, NULL },
};

struct memory_list memory_list_vtysh[] =
{
  { MTYPE_VTYSH_CONFIG,		"Vtysh configuration",		},
  { MTYPE_VTYSH_CONFIG_LINE,	"Vtysh configuration line"	},
  { -1, NULL },
};

struct mlist mlists[] __attribute__ ((unused)) = {
  { memory_list_lib,	"LIB"	},
  { memory_list_zebra,	"ZEBRA"	},
  { memory_list_rip,	"RIP"	},
  { memory_list_ripng,	"RIPNG"	},
  { memory_list_ospf,	"OSPF"	},
  { memory_list_ospf6,	"OSPF6"	},
  { memory_list_isis,	"ISIS"	},
  { memory_list_bgp,	"BGP"	},
  { memory_list_pim,	"PIM"	},
  { NULL, NULL},
};
