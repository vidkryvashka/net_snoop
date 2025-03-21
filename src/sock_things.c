#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>    // idk why, without it struct timeval glows red in vs code (inclomplete type) but works
// #include <fcntl.h>
// #include <signal.h>  // for capturing ^C

#include "defs.h"

#define CUSTOM_MAX_HOP 32

// #define MY_NI_MAXHOST   1025    // NI_MAXHOST from <netdb.h>, errorlesly glows in vs code
// #define MY_NI_NAMEREQD  8       // NI_NAMEREQD from <netdb.h>, too
#define PORT_NUM 0
#define TX_PACKET_SIZE 64
#define RECV_TIMEOUT 1          // timeout for receiving packets (in seconds)

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

    if (getnameinfo((struct sockaddr *)&temp_addr, sizeof(struct sockaddr_in), buff, sizeof(buff), NULL, 0, MY_NI_NAMEREQD)) {
        strcpy(dest, "-");
        return 0;
    }
    strcpy(dest, buff);
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


static int process_resp(char *rx_buff, int msg_sent, int ttl_binded) {

    struct iphdr *ip_hdr = (struct iphdr *)rx_buff;
    struct icmphdr *rx_icmp_hdr = (struct icmphdr *)(rx_buff + ip_hdr->ihl * 4);
    char ip_addr[16];
    char reverse_hostname[MY_NI_MAXHOST+1];
    unsigned char *ip = (unsigned char *)&ip_hdr->saddr;
    
    if ( !(rx_icmp_hdr->type % 11)) {
        printf("%d\t", msg_sent);
        snprintf(ip_addr, 16, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        reverse_dns_lookup(ip_addr, reverse_hostname);
        printf("%s\t%s\n",
            ip_addr, reverse_hostname);
            
        if (rx_icmp_hdr->type == 11)
            return 11;
        else
            printf("reached\n");
    } else {
        printf("Error: responce icmp type %d\n", rx_icmp_hdr->type);
        return 0;
    }
    return 1;
}


static void fist_network(
        const int sockfd,
        const struct sockaddr_in *target_addr,
        const char *rev_host,
        const char *target_ip,
        const char *target_fqdn,
        const uint8_t max_hops
    ) {

    struct timeval tv_out = {
        .tv_sec = RECV_TIMEOUT,
        .tv_usec = 0
    };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv_out, sizeof(tv_out));
    
    int is_pack_sent = 1, msg_sent = 0, ttl_binded = 0;
    struct packet_t tx_pkt;
    
move_deeper:
    ++ msg_sent;
    ttl_binded = msg_sent;
    // ttl_binded += msg_sent + 1;
    if (setsockopt(sockfd, SOL_IP, IP_TTL, &ttl_binded, sizeof(ttl_binded))) {
        printf("Error: setting socket options to TTL failed\n");
        return;
    }

    prepare_tx_pkt(&tx_pkt, msg_sent);
    
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
        is_pack_sent = 0;
    } //else
      //  ++ msg_sent;

    struct sockaddr_in rx_addr;
    uint8_t rx_addr_len = sizeof(rx_addr);
    char rx_buff[128];
    ssize_t err_recvfrom = recvfrom(
        sockfd,
        rx_buff,
        sizeof(rx_buff),
        0,
        (struct sockaddr *)&rx_addr,
        (socklen_t *)&rx_addr_len
    );
    if (err_recvfrom <= 0) {
        printf("%d\t* * *\trecvfrom = %ld\tnode refuses to echo\n", msg_sent, err_recvfrom);
        goto move_deeper;
    } else {
        if (is_pack_sent) {
            if (process_resp(rx_buff, msg_sent, ttl_binded) == 11 && msg_sent < max_hops)
                goto move_deeper;
        }
    }
}   // static void fist_network


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

    printf("Target hostname: %s, ip: %s, max-hops %d\n", conf->target_fqdn, target_ip_addr, conf->max_hops);


    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        printf("Error: socket file descriptor not received, maybe u r not root\n");
        return 0;
    }

    fist_network(sockfd, &addr_con, reverse_hostname, target_ip_addr, conf->target_fqdn, conf->max_hops);

    return 1;
}