#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
// #include <netinet/ip_icmp.h>
// #include <time.h>
// #include <fcntl.h>
// #include <signal.h>

#define custom_FQDN "google.com"

#define MY_NI_MAXHOST   1025
#define MY_NI_NAMEREQD  8
#define PORT_NO 0

char *dns_lookup(char *addr_host, struct sockaddr_in *addr_con) {
    struct hostent *host_entity;
    char *ip = (char *)malloc(MY_NI_MAXHOST * sizeof(char));
    if (!(host_entity = gethostbyname(addr_host)))
        return NULL;
    
    strcpy(ip, inet_ntoa(*(struct in_addr *)host_entity->h_addr_list[0]));
    addr_con->sin_family = host_entity->h_addrtype;
    addr_con->sin_port = htons(PORT_NO);
    addr_con->sin_addr.s_addr = *(long *)host_entity->h_addr_list[0];

    return ip;
}

char *reverse_dns_lookup(char *target_ip_addr) {
    struct sockaddr_in temp_addr;
    socklen_t len;
    char buf[MY_NI_MAXHOST], *ret_buf;

    temp_addr.sin_family = AF_INET;
    temp_addr.sin_addr.s_addr = inet_addr(target_ip_addr);
    len = sizeof(struct sockaddr_in);

    if (getnameinfo((struct sockaddr *)&temp_addr, len, buf, sizeof(buf), NULL, 0, MY_NI_NAMEREQD)) {
        printf("Could not resolve reverse lookup of hostname\n");
        return NULL;
    }

    ret_buf = (char *)malloc((strlen(buf)+1) * sizeof(char));
    strcpy(ret_buf, buf);
    return ret_buf;
}

int worker() {

    int sockfd;
    char *target_ip_addr, *reverse_hostname;
    struct sockaddr_in addr_con;
    int addrlen = sizeof(addr_con);
    // char netbuff[MY_NI_MAXHOST];

    target_ip_addr = dns_lookup(custom_FQDN, &addr_con);
    if (target_ip_addr == NULL) {
        printf("dns_lookup failed, couldn't resolve %s address\n", custom_FQDN);
        return 0;
    }

    reverse_hostname = reverse_dns_lookup(target_ip_addr);
    printf("Target ip: %s, reverse lookup domain: %s\n", target_ip_addr, reverse_hostname);

    printf("gg\n");

    return 1;
}