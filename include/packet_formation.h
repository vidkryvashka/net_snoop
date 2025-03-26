#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ether.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <linux/if_packet.h>
#include <errno.h>


#define TX_PACKET_SIZE  32
#define PORT_NUM    0
#define MY_NI_NAMEREQD  8   // like NI_NAMEREQD in <netdb.h>

struct packet_t {
    struct icmphdr icmp_hdr;
    char msg[TX_PACKET_SIZE - sizeof(struct icmphdr)];
};


uint8_t dns_lookup(
    const char *addr_host,
    struct sockaddr_in *addr_con,
    char *target_ip_addr
);


uint8_t reverse_dns_lookup(
    const char *target_ip_addr,
    char *dest
);


uint16_t checksum(
    const struct packet_t *tx_pkt,
    int len
);


void prepare_tx_pkt(
    struct packet_t *tx_pkt,
    const int msg_sent
);