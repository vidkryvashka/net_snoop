#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>    // idk why, without it struct timeval glows red (inclomplete type) but works
// #include <fcntl.h>
// #include <signal.h>

// #define CUSTOM_FQDN "google.com"
#define CUSTOM_FQDN "127.0.0.1"

#define MY_NI_MAXHOST   1025    // NI_MAXHOST from <netdb.h>, errorlesly glows
#define MY_NI_NAMEREQD  8       // NI_NAMEREQD from <netdb.h>, too
#define PORT_NUM 0
#define TX_PACKET_SIZE 64
#define RECV_TIMEOUT 1          // timeout for receiving packets (in seconds)
// #define SLEEP_RATE  1000000

struct packet_t {
    struct icmphdr hdr;
    char msg[TX_PACKET_SIZE - sizeof(struct icmphdr)];
};


static char *dns_lookup(char *addr_host, struct sockaddr_in *addr_con) {
    struct hostent *host_entity;
    char *ip = (char *)malloc(MY_NI_MAXHOST * sizeof(char));
    if (!(host_entity = gethostbyname(addr_host)))
        return NULL;
    
    strcpy(ip, inet_ntoa(*(struct in_addr *)host_entity->h_addr_list[0]));
    addr_con->sin_family = host_entity->h_addrtype;
    addr_con->sin_port = htons(PORT_NUM);
    addr_con->sin_addr.s_addr = *(long *)host_entity->h_addr_list[0];

    return ip;
}

static char *reverse_dns_lookup(char *target_ip_addr) {
    struct sockaddr_in temp_addr;
    socklen_t len;
    char buff[MY_NI_MAXHOST], *ret_buf;

    temp_addr.sin_family = AF_INET;
    temp_addr.sin_addr.s_addr = inet_addr(target_ip_addr);
    len = sizeof(struct sockaddr_in);

    if (getnameinfo((struct sockaddr *)&temp_addr, len, buff, sizeof(buff), NULL, 0, MY_NI_NAMEREQD)) {
        printf("Error: could not resolve reverse lookup of hostname\n");
        return NULL;
    }

    ret_buf = (char *)malloc((strlen(buff)+1) * sizeof(char));
    strcpy(ret_buf, buff);
    return ret_buf;
}

static uint16_t checksum(struct packet_t *pkt, int len) {
    uint16_t *buff = (uint16_t *)pkt;
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

static void send_packet(int sockfd, struct sockaddr_in *target_addr, char *target_fqdn, char *target_ip, char *rev_host) {
    int ttl_val = 64;
    char rx_buff[128];
    // struct packet_t ping_packet;
    struct sockaddr_in rx_addr;
    int rx_addr_len = sizeof(rx_addr);

    struct timeval tv_out = {
        .tv_sec = RECV_TIMEOUT,
        .tv_usec = 0
    };
    
    if (setsockopt(sockfd, SOL_IP, IP_TTL, &ttl_val, sizeof(ttl_val))) {
        printf("Error: setting socket options to TTL failed\n");
        return;
    } else {
        printf("Socket set to TTL ...\n");
    }

    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv_out, sizeof(tv_out));

    int is_pack_sent = 1, msg_count = 0;
    struct packet_t pkt;
    bzero(&pkt, sizeof(pkt));
    pkt.hdr.type = ICMP_ECHO;
    pkt.hdr.un.echo.id = getpid();

    for (int i = 0; i < sizeof(pkt.msg)-1; ++i)
        pkt.msg[i] = i + '0';
    pkt.msg[sizeof(pkt.msg)-1] = 0;
    pkt.hdr.un.echo.sequence = msg_count++;
    pkt.hdr.checksum = checksum(&pkt, sizeof(pkt));

    if (sendto(
        sockfd,
        &pkt,
        sizeof(pkt),
        0,
        (struct sockaddr *)target_addr,
        sizeof(*target_addr)
    ) <= 0) {
        printf("Error: packet sending failed\n");
        is_pack_sent = 0;
    };

    if (recvfrom(
        sockfd,
        rx_buff,
        sizeof(rx_buff),
        0,
        (struct sockaddr *)&rx_addr,
        (socklen_t *)&rx_addr_len
    ) <= 0 && msg_count > 1) {
        printf("Error: packet receive failed");
    } else {
        if (is_pack_sent) {
            struct icmphdr *recv_hdr = (struct icmphdr *)rx_buff;
            if (!(recv_hdr->type == 0 && recv_hdr->code == 0)) {
                printf("Error: packet received with ICMP type %d code %d\n",
                        recv_hdr->type, recv_hdr->code);
            } else
                printf("%d bytes from %s (h: %s) (ip: %s) msg_seq = %d ttl = %d\n",
                        TX_PACKET_SIZE, target_fqdn, rev_host, target_ip, msg_count, ttl_val);
        }
    }
}

int worker() {

    int sockfd;
    char *target_ip_addr, *reverse_hostname;
    struct sockaddr_in addr_con;
    int addr_len = sizeof(addr_con);
    // char netbuff[MY_NI_MAXHOST];

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

    send_packet(sockfd, &addr_con, reverse_hostname, target_ip_addr, CUSTOM_FQDN);

    printf("end program\n");

    return 1;
}