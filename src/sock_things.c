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
#include "packet_formation.h"

#define RECV_TIMEOUT    1      // timeout for receiving packets (in seconds)
#define MAX_ATTEMPTS 3


static void print_node(
    const int msg_sent,
    const struct iphdr *rx_ip_hdr,
    const struct icmphdr *rx_icmp_hdr,
    const float time_taken_ms,
    const uint8_t attempt
) {
    char ip_addr[16];
    unsigned char *ip = (unsigned char *)&rx_ip_hdr->saddr;
    snprintf(ip_addr, 16, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    char reverse_hostname[MY_NI_MAXHOST+1];
    reverse_dns_lookup(ip_addr, reverse_hostname);
    
    if (attempt == 0)
        printf("%d", msg_sent);
    printf("\t%s\t%s\t%.3f ms",
        ip_addr, reverse_hostname, time_taken_ms
    );
    if (rx_icmp_hdr->type != 0) {
        printf("\n");
    } else
        printf("\treached\n");
}


static int process_resp(
    const char *rx_buff,
    const int msg_sent,
    const float time_taken_ms,
    const uint8_t attempt
) {
    struct iphdr *rx_ip_hdr = (struct iphdr *)rx_buff;
    struct icmphdr *rx_icmp_hdr = (struct icmphdr *)(rx_buff + rx_ip_hdr->ihl * 4);
    
    switch (rx_icmp_hdr->type) {
        case 0:
        case 11:
            print_node(msg_sent, rx_ip_hdr, rx_icmp_hdr, time_taken_ms, attempt);
            return rx_icmp_hdr->type;
        case 3:
            if (attempt == 0)
                printf("%d", msg_sent);
            printf(" *p ");
            return 3;
	    case 8:
            printf("responce icmp type 8 loopback\n");
            return -1;
        default:
            printf("Error: responce icmp type %d\n", rx_icmp_hdr->type);
            return -1;
    }
    return 1;   // good
}


static int send_receive(
    const int sockfd,
    const struct sockaddr_in *target_addr,
    const int msg_sent,
    const config_t *conf,
    const uint8_t attempt
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
        if (attempt == 0)
            printf("%d\t", msg_sent);
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            printf("* timeout ");
        else
            printf("* recvfrom failed: %s ", strerror(errno));
        fflush(stdout);
        return 3;   // not an icmp resp, just try send again
    } else
        return process_resp(rx_buff, msg_sent, time_taken_ms, attempt);
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
    uint8_t msg_sent = 0, ttl_binded = 0;
    
move_deeper:
    if (++ msg_sent > conf->max_hops) {
        printf("Out of hops\n");
        return;
    }
    uint8_t attempt = 0;
send_again:
    ttl_binded = msg_sent;
    if (setsockopt(sockfd, SOL_IP, IP_TTL, &ttl_binded, sizeof(ttl_binded))) {
        printf("Error: setting socket options to TTL failed\n");
        return;
    }
    switch (send_receive(sockfd, target_addr, msg_sent, conf, attempt)) {
        case 0:
        case 1:
            break;
        case 3:     // return signal to attempt again
            if (++attempt < MAX_ATTEMPTS)
                goto send_again;
            attempt = 0;
            printf("\n");
        case 11:
            goto move_deeper;
        case -1:
            perror("error(s) occured");
            break;
        default:
            printf("fist_network idk what happen, u called send_receive, returned default\n");
            break;
    }
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
