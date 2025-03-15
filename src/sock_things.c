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
// #include <signal.h>

// #define CUSTOM_FQDN "google.com"
#define CUSTOM_FQDN "google.com"
#define CUSTOM_MAX_HOP 32
// #define CUSTOM_FQDN "localhost"

#define MY_NI_MAXHOST   1025    // NI_MAXHOST from <netdb.h>, errorlesly glows in vs code
#define MY_NI_NAMEREQD  8       // NI_NAMEREQD from <netdb.h>, too
#define PORT_NUM 0
#define TX_PACKET_SIZE 64
#define RECV_TIMEOUT 1          // timeout for receiving packets (in seconds)

struct packet_t {
    struct icmphdr icmp_hdr;
    char msg[TX_PACKET_SIZE - sizeof(struct icmphdr)];
};


static char *dns_lookup(const char *addr_host, struct sockaddr_in *addr_con) {
    struct hostent *host_entity;
    char *ip = (char *)malloc(MY_NI_MAXHOST * sizeof(char));
    if (!(host_entity = gethostbyname(addr_host)))
        return NULL;
    
    addr_con->sin_family = host_entity->h_addrtype;
    addr_con->sin_port = htons(PORT_NUM);
    addr_con->sin_addr.s_addr = *(long *)host_entity->h_addr_list[0];
    
    strcpy(ip, inet_ntoa(*(struct in_addr *)host_entity->h_addr_list[0]));

    return ip;
}

static char *reverse_dns_lookup(const char *target_ip_addr) {
    struct sockaddr_in temp_addr;
    socklen_t len;
    char buff[MY_NI_MAXHOST], *ret_buf;

    temp_addr.sin_family = AF_INET;
    temp_addr.sin_addr.s_addr = inet_addr(target_ip_addr);

    if (getnameinfo((struct sockaddr *)&temp_addr, sizeof(struct sockaddr_in), buff, sizeof(buff), NULL, 0, MY_NI_NAMEREQD)) {
        printf("Error: could not resolve reverse lookup of hostname\n");
        return NULL;
    }

    ret_buf = (char *)malloc((strlen(buff)+1) * sizeof(char));
    strcpy(ret_buf, buff);
    return ret_buf;
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


static void fist_network(
        const int sockfd,
        const struct sockaddr_in *target_addr,
        const char *rev_host,
        const char *target_ip,
        const char *target_fqdn
    ) {

    struct timeval tv_out = {
        .tv_sec = RECV_TIMEOUT,
        .tv_usec = 0
    };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv_out, sizeof(tv_out));
    
    int is_pack_sent = 1, msg_sent = 0;
    struct packet_t tx_pkt;
    
    
move_deeper:
    int ttl_binded = 1;
    ttl_binded += msg_sent;
    if (setsockopt(sockfd, SOL_IP, IP_TTL, &ttl_binded, sizeof(ttl_binded))) {
        printf("Error: setting socket options to TTL failed\n");
        return;
    } else {
        // printf("Socket set to TTL %d ...\n", ttl_binded);
    }

    prepare_tx_pkt(&tx_pkt, msg_sent);
    
    if (sendto(
        sockfd,
        &tx_pkt,
        sizeof(tx_pkt),
        0,
        (struct sockaddr *)target_addr,
        sizeof(*target_addr)
    ) <= 0) {
        printf("Error: packet sending failed\n");
        is_pack_sent = 0;
    };
    ++ msg_sent;

    struct sockaddr_in rx_addr;
    uint8_t rx_addr_len = sizeof(rx_addr);
    char rx_buff[128];  // big ugly fat fuck
    if (recvfrom(
        sockfd,
        rx_buff,
        sizeof(rx_buff),
        0,
        (struct sockaddr *)&rx_addr,
        (socklen_t *)&rx_addr_len
    ) <= 0) {
        printf("Error: packet receive failed\n");
    } else {
        if (is_pack_sent) {
            struct iphdr *ip_hdr = (struct iphdr *)rx_buff;
            struct icmphdr *rx_icmp_hdr = (struct icmphdr *)(rx_buff + ip_hdr->ihl * 4);
            printf("%d\t", msg_sent);
            char source[16];
            unsigned char *ip = (unsigned char *)&ip_hdr->saddr;
            switch (rx_icmp_hdr->type) {
                case 0:         // echo reply
                    snprintf(source, 16, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
                    printf("%s\treach\n",
                            source);
                    // printf("%d bytes from %s (h: %s) (ip: %s) msg_seq = %d ttl = %d\n",
                    //         TX_PACKET_SIZE, target_fqdn, rev_host, target_ip, msg_sent, ttl_binded);
                    break;
                case 11:
                    snprintf(source, 16, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
                    printf("%s\n",
                            source);
                    // printf("time exceeded %d bytes from %s (h: %s) (ip: %s) msg_seq = %d ttl = %d\n",
                    //         TX_PACKET_SIZE, target_fqdn, rev_host, target_ip, msg_sent, ttl_binded);
                    if (ttl_binded <= CUSTOM_MAX_HOP) {
                        goto move_deeper;
                    }
                    break;
                default:
                    printf("Error: packet received with ICMP type %d code %d\n",
                            rx_icmp_hdr->type, rx_icmp_hdr->code);
                    break;
            }
        }
    }
}   // static void fist_network


int organize() {

    int sockfd;
    char *target_ip_addr, *reverse_hostname;
    struct sockaddr_in addr_con;
    int addr_len = sizeof(addr_con);

    target_ip_addr = dns_lookup(CUSTOM_FQDN, &addr_con);
    if (target_ip_addr == NULL) {
        printf("Error: dns_lookup failed, couldn't resolve %s address\n", CUSTOM_FQDN);
        return 0;
    }

    reverse_hostname = reverse_dns_lookup(target_ip_addr);
    printf("Target ip: %s, reverse lookup domain: %s\n", target_ip_addr, reverse_hostname);

    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        printf("Error: socket file descriptor not received, maybe u r not root\n");
        return 0;
    }

    fist_network(sockfd, &addr_con, reverse_hostname, target_ip_addr, CUSTOM_FQDN);


    free(target_ip_addr);
    free(reverse_hostname);

    printf("end program\n");

    return 1;
}