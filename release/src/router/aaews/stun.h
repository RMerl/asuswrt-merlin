#define STUN_MESSAGETYPE_SIZE 2
#define STUN_MESSAGELEN_SIZE  2
#define STUN_MAGICCOOKIE_SIZE 4
#define STUN_TID_SIZE		  12

#define STUN_ATTR_SOFTWARE 	 0x8022
#define STUN_PORT 			 3478
#define STUN_HEADER_SIZE 	 20
#define STUN_MAGIC_COOKIE 	 0x2112a442
#define STUN_METHOD_BINDING  0x001
#define STUN_CLASS_REQUEST   0x0
#define BINDING_REQUEST_SIZE 20

#define STUN_ATTRTYPE_SIZE  2
#define STUN_ATTRLEN_SIZE   2
#define MAPPEDADDRESS_SIZE	8

#define STUN_MAPPEDADDRESS      0x0001
#define STUN_RESPONSEADDRESS    0x0002
#define STUN_CHANGEREQUEST      0x0003
#define STUN_SOURCEADDRESS      0x0004
#define STUN_CHANGEADDRESS      0x0005
#define STUN_USERNAME           0x0006
#define STUN_PASSWORD           0x0007
#define STUN_MESSAGEINTEGRITY   0x0008
#define STUN_ERRORCODE          0x0009
#define STUN_UNKNOWNATTRIBUTE   0x000A
#define STUN_REFLECTEDFROM      0x000B
#define STUN_XORONLY            0x0021
#define STUN_XORMAPPEDADDRESS   0x0020

#define MAX_STUN_RETRY 3

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

struct stun_hdr {
        uint16_t type;               /**< Message type   */
        uint16_t len;                /**< Payload length */
        uint32_t cookie;             /**< Magic cookie   */
        uint8_t tid[STUN_TID_SIZE];  /**< Transaction ID */
};


struct mbuf {
        uint8_t *buf;   /**< Buffer memory      */
        int size;    /**< Size of buffer     */
        int pos;     /**< Position in buffer */
        int end;     /**< End of buffer      */
};

int send_binding_request(unsigned int stun_ip,unsigned short stun_port,unsigned int *dieve_public_ip);