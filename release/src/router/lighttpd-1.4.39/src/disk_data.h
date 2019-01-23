
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef struct disk_info disk_info_t;
typedef struct partition_info partition_info_t;

#pragma pack(1) // let struct be neat by byte.
struct disk_info{
        char *tag;
        char *vendor;
        char *model;
        char *device;
        u32 major;
        u32 minor;
        char *port;
        u32 partition_number;
        u32 mounted_number;
        u64 size_in_kilobytes;
        partition_info_t *partitions;
        disk_info_t *next;
} ;

struct partition_info{
        char *device;
        char *label;
        u32 partition_order;
        char *mount_point;
        char *file_system;
        char *permission;
        u64 size_in_kilobytes;
        u64 used_kilobytes;
        disk_info_t *disk;
        partition_info_t *next;
} ;

extern disk_info_t *read_disk_data();
extern void free_disk_data(disk_info_t **disk_info_list);
