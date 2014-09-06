#ifdef __cplusplus
extern          "C" {
#endif

/*
 * struct for netlink extras
 */
struct address_flag_info {
    int bcastflg;
    int anycastflg;
    in_addr_t addr;
};
 
#define IS_APIPA(a)  (((in_addr_t)(a << 16)) == 0xFEA90000)

int
_netsnmp_ioctl_ipaddress_container_load_v4(netsnmp_container *container,
                                                  int idx_offset);
int
_netsnmp_ioctl_ipaddress_set_v4(netsnmp_ipaddress_entry * entry);
int
_netsnmp_ioctl_ipaddress_remove_v4(netsnmp_ipaddress_entry * entry);

int
_netsnmp_ioctl_ipaddress_set_v6(netsnmp_ipaddress_entry * entry);
int
_netsnmp_ioctl_ipaddress_remove_v6(netsnmp_ipaddress_entry * entry);

int
netsnmp_access_ipaddress_ioctl_get_interface_count(int sd, struct ifconf * ifc);

struct address_flag_info
netsnmp_access_other_info_get(int index, int family);

/*
 * struct ioctl for arch_data
 */
typedef struct _ioctl_extras {
   u_int            flags;
   u_char           name[IFNAMSIZ];
} _ioctl_extras;



_ioctl_extras *
netsnmp_ioctl_ipaddress_entry_init(netsnmp_ipaddress_entry *entry);
void
netsnmp_ioctl_ipaddress_entry_cleanup(netsnmp_ipaddress_entry *entry);
int
netsnmp_ioctl_ipaddress_entry_copy(netsnmp_ipaddress_entry *lhs,
                                   netsnmp_ipaddress_entry *rhs);

_ioctl_extras *
netsnmp_ioctl_ipaddress_extras_get(netsnmp_ipaddress_entry *entry);

int
_netsnmp_ioctl_ipaddress_delete_v4(netsnmp_ipaddress_entry * entry);
int
_netsnmp_ioctl_ipaddress_delete_v6(netsnmp_ipaddress_entry * entry);

#ifdef __cplusplus
}
#endif

