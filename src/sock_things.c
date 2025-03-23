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

#include "defs.h"

#define PORT_NUM 0
#define TX_PACKET_SIZE 64
#define RECV_TIMEOUT 3      // timeout for receiving packets (in seconds)



struct packet_t {
    struct icmphdr icmp_hdr;
    char msg[TX_PACKET_SIZE - sizeof(struct icmphdr)];
};


static int dns_lookup(const char *addr_host, struct sockaddr_in *addr_con, char *target_ip_addr) {
    struct hostent *host_entity;
    if (!(host_entity = gethostbyname(addr_host)))
        return 0;
    
    addr_con->sin_family = host_entity->h_addrtype;
    addr_con->sin_port = htons(PORT_NUM);
    addr_con->sin_addr.s_addr = *(long *)host_entity->h_addr_list[0];
    
    strcpy(target_ip_addr, inet_ntoa(*(struct in_addr *)host_entity->h_addr_list[0]));

    return 1;
}


static int reverse_dns_lookup(const char *target_ip_addr, char *dest) {
    struct sockaddr_in temp_addr;
    socklen_t len;
    char buff[MY_NI_MAXHOST];

    temp_addr.sin_family = AF_INET;
    temp_addr.sin_addr.s_addr = inet_addr(target_ip_addr);

    if (getnameinfo((struct sockaddr *)&temp_addr, sizeof(struct sockaddr_in), buff, sizeof(buff), NULL, 0, NI_NAMEREQD)) {
        strcpy(dest, "-\t");
        return 0;
    }
    strncpy(dest, buff, MY_NI_MAXHOST);
    return 1;
}


static uint16_t checksum(const struct packet_t *tx_pkt, int len) {
    uint16_t *buff = (uint16_t *)tx_pkt;
    uint32_t sum = 0;
    u_int16_t res;
    for (sum = 0; len > 1; len -= 2)
        sum += *buff++;
    if (len == 1)
        sum += *(unsigned char *)buff;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    res = ~sum;
    return res;
}


static void prepare_tx_pkt(struct packet_t *tx_pkt, int msg_sent) {
    bzero(tx_pkt, sizeof(*tx_pkt));
    tx_pkt->icmp_hdr.type = ICMP_ECHO;
    tx_pkt->icmp_hdr.un.echo.id = getpid();
    for (int i = 0; i < sizeof(tx_pkt->msg)-1; ++i)
        tx_pkt->msg[i] = i + '0';
    tx_pkt->msg[sizeof(tx_pkt->msg)-1] = 0;
    tx_pkt->icmp_hdr.un.echo.sequence += msg_sent;
    tx_pkt->icmp_hdr.checksum = checksum(tx_pkt, sizeof(*tx_pkt));
}


static int print_node(
        int msg_sent,
        char *reverse_hostname,
        struct iphdr *rx_ip_hdr,
        struct icmphdr *rx_icmp_hdr,
        float time_taken_ms
    ) {
    
    char ip_addr[16];
    unsigned char *ip = (unsigned char *)&rx_ip_hdr->saddr;
    snprintf(ip_addr, 16, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    reverse_dns_lookup(ip_addr, reverse_hostname);

    printf("%d\t%s\t%s\t%.3f ms%s",
        msg_sent, ip_addr, reverse_hostname, time_taken_ms,
        (rx_icmp_hdr->type == 3) ? "\ticmp resp 3: port unreachable, recvfrom з глузду з\'їхав" : ""
    );
    if (rx_icmp_hdr->type == 11 || rx_icmp_hdr->type == 3) {
        printf("\n");
        return 11;
    } else
        printf("\treached\n");
    return 1;
}


static int process_resp(char *rx_buff, int msg_sent, float time_taken_ms) {

    struct iphdr *rx_ip_hdr = (struct iphdr *)rx_buff;
    struct icmphdr *rx_icmp_hdr = (struct icmphdr *)(rx_buff + rx_ip_hdr->ihl * 4);
    char reverse_hostname[MY_NI_MAXHOST+1];
    
    
    switch (rx_icmp_hdr->type) {
        case 0:
        case 3:
        case 11:
            if (print_node(msg_sent, reverse_hostname, rx_ip_hdr, rx_icmp_hdr, time_taken_ms) == 11)
                return 11;
            break;
	    case 8:
            printf("responce icmp type 8 loopback\n");
            break;
        default:
            printf("Error: responce icmp type %d\n", rx_icmp_hdr->type);
            return 0;
    }
    return 1;   // good
}


static int send_receive(
        const int sockfd,
        const struct sockaddr_in *target_addr,
        int msg_sent,
        const config_t *conf
    ) {

    struct packet_t tx_pkt;
    prepare_tx_pkt(&tx_pkt, msg_sent);
    clock_t t = clock();
    ssize_t err_sendto = sendto(
        sockfd,
        &tx_pkt,
        sizeof(tx_pkt),
        0,
        (struct sockaddr *)target_addr,
        sizeof(*target_addr)
    );
    if (err_sendto <= 0) {
        printf("Error: packet sending failed, sendto error %ld\n", err_sendto);
        return -1;
    }

    struct sockaddr_in rx_addr;
    socklen_t rx_addr_len = sizeof(rx_addr);
    bzero(&rx_addr, rx_addr_len);
    char rx_buff[256];
    ssize_t err_recvfrom = recvfrom(
        sockfd,
        rx_buff,
        sizeof(rx_buff),
        0,
        (struct sockaddr *)&rx_addr,
        &rx_addr_len
    );
    t = clock() - t;
    float time_taken_ms = (float)t*1000 / (float)CLOCKS_PER_SEC;
    if (err_recvfrom <= 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            printf("%d\t* * *\trecvfrom timeout\n", msg_sent);
        else
            printf("%d\t* * *\trecvfrom failed: %s\n", msg_sent, strerror(errno));
        return 11;
    } else if (process_resp(rx_buff, msg_sent, time_taken_ms) == 11)
        return 11;
    return 1;   // good
}


static void fist_network(
        const int sockfd,
        const struct sockaddr_in *target_addr,
        const char *rev_host,
        const char *target_ip,
        const config_t *conf
    ) {

    if (conf->interface[0])
        setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, conf->interface, INTERFACE_LENGTH);

    struct timeval tv_out = {
        .tv_sec = RECV_TIMEOUT,
        .tv_usec = 0
    };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv_out, sizeof(tv_out));
    int msg_sent = 0, ttl_binded = 0;
    
move_deeper:
    if (++ msg_sent > conf->max_hops) {
        printf("Out of hops\n");
        return;
    }
    ttl_binded = msg_sent;
    if (setsockopt(sockfd, SOL_IP, IP_TTL, &ttl_binded, sizeof(ttl_binded))) {
        printf("Error: setting socket options to TTL failed\n");
        return;
    }
    if (send_receive(sockfd, target_addr, msg_sent, conf) == 11)
        goto move_deeper;

}



int organize(const config_t *conf) {

    int sockfd;
    char target_ip_addr[MY_NI_MAXHOST];
    char reverse_hostname[MY_NI_MAXHOST+1];
    struct sockaddr_in addr_con;
    int addr_len = sizeof(addr_con);

    
    if (!dns_lookup(conf->target_fqdn, &addr_con, target_ip_addr)) {
        printf("Error: dns_lookup failed, couldn't resolve %s address\n", conf->target_fqdn);
        return 0;
    }

    reverse_dns_lookup(target_ip_addr, reverse_hostname);

    printf("Target hostname: %s, ip: %s, max-hops %d %s %s\n",
            conf->target_fqdn, target_ip_addr, conf->max_hops,
            (conf->interface[0]) ? " interface" : "",
            (conf->interface[0]) ? conf->interface : ""
        );


    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        printf("Error: socket file descriptor not received, maybe u r not root\n");
        return 0;
    }

    fist_network(sockfd, &addr_con, reverse_hostname, target_ip_addr, conf);

    close(sockfd);

    return 1;
}
