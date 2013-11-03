#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/param.h>

#include <atalk/fce_api.h>
#include <atalk/util.h>

#define MAXBUFLEN 1024

static char *fce_ev_names[] = {
    "",
    "FCE_FILE_MODIFY",
    "FCE_FILE_DELETE",
    "FCE_DIR_DELETE",
    "FCE_FILE_CREATE",
    "FCE_DIR_CREATE"
};

static int unpack_fce_packet(unsigned char *buf, struct fce_packet *packet)
{
    unsigned char *p = buf;

    memcpy(&packet->magic[0], p, sizeof(packet->magic));
    p += sizeof(packet->magic);

    packet->version = *p;
    p++;

    packet->mode = *p;
    p++;

    memcpy(&packet->event_id, p, sizeof(packet->event_id));
    p += sizeof(packet->event_id);
    packet->event_id = ntohl(packet->event_id);

    memcpy(&packet->datalen, p, sizeof(packet->datalen));
    p += sizeof(packet->datalen);
    packet->datalen = ntohs(packet->datalen);

    memcpy(&packet->data[0], p, packet->datalen);
    packet->data[packet->datalen] = 0; /* 0 terminate strings */
    p += packet->datalen;

    return 0;
}

int main(void)
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(NULL, FCE_DEFAULT_PORT_STRING, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);

    printf("listener: waiting to recvfrom...\n");

    addr_len = sizeof their_addr;

    struct fce_packet packet;
    while (1) {
        if ((numbytes = recvfrom(sockfd,
                                 buf,
                                 MAXBUFLEN - 1,
                                 0,
                                 (struct sockaddr *)&their_addr,
                                 &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }

        unpack_fce_packet((unsigned char *)buf, &packet);

        if (memcmp(packet.magic, FCE_PACKET_MAGIC, sizeof(packet.magic)) == 0) {

            switch (packet.mode) {
            case FCE_CONN_START:
                printf("FCE Start\n");
                break;

            case FCE_CONN_BROKEN:
                printf("Broken FCE connection\n");
                break;

            default:
                printf("ID: %" PRIu32 ", Event: %s, Path: %s\n",
                       packet.event_id, fce_ev_names[packet.mode], packet.data);
                break;
            }
        }
    }

    close(sockfd);
    return 0;
}
