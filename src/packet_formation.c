#include "defs.h"
#include "packet_formation.h"



uint8_t dns_lookup(
    const char *addr_host,
    struct sockaddr_in *addr_con,
    char *target_ip_addr
) {
    struct hostent *host_entity;
    if (!(host_entity = gethostbyname(addr_host)))
        return 0;

    addr_con->sin_family = host_entity->h_addrtype;
    addr_con->sin_port = htons(PORT_NUM);
    addr_con->sin_addr.s_addr = *(long *)host_entity->h_addr_list[0];

    strcpy(target_ip_addr, inet_ntoa(*(struct in_addr *)host_entity->h_addr_list[0]));

    return 1;
}


uint8_t reverse_dns_lookup(
    const char *target_ip_addr,
    char *dest
) {
    struct sockaddr_in temp_addr;
    socklen_t len;
    char buff[MY_NI_MAXHOST];
    
    temp_addr.sin_family = AF_INET;
    temp_addr.sin_addr.s_addr = inet_addr(target_ip_addr);
    
    if (getnameinfo((struct sockaddr *)&temp_addr, sizeof(struct sockaddr_in), buff, sizeof(buff), NULL, 0, MY_NI_NAMEREQD)) {
        strcpy(dest, "-\t");
        return 0;
    }
    strncpy(dest, buff, MY_NI_MAXHOST);
    return 1;
}


uint16_t checksum(
    const struct packet_t *tx_pkt,
    int len
) {
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


void prepare_tx_pkt(
    struct packet_t *tx_pkt,
    int msg_sent
) {
    bzero(tx_pkt, sizeof(*tx_pkt));
    tx_pkt->icmp_hdr.type = ICMP_ECHO;
    tx_pkt->icmp_hdr.un.echo.id = getpid();
    for (int i = 0; i < sizeof(tx_pkt->msg)-1; ++i)
        tx_pkt->msg[i] = i + '0';
    tx_pkt->msg[sizeof(tx_pkt->msg)-1] = 0;
    tx_pkt->icmp_hdr.un.echo.sequence += msg_sent;
    tx_pkt->icmp_hdr.checksum = checksum(tx_pkt, sizeof(*tx_pkt));
}