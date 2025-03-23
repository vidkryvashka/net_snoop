#include "defs.h"

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h> // bzero
#include <stdlib.h>
#include <ifaddrs.h>

static void print_help() {
    printf("\n\
    needs to be running with root\n\n\
    ./path2program <char *fqdn> \t\tsame as with -d --destination \tdefault %s\n\
    -d --destination <char *fqdn>\t\tspecify destination FQDN\n\
    -h --max-hops <uint8_t hops>\t\tspecify max hop (ttl) count, \tdefault %d\n\
    -i --interface <optional char *interface>\tallows to choose interface, not implemented\n\
    --help\n\n",
    DEFAULT_FQDN, DEFAULT_MAX_HOPS);
}


static int choose_interface(char *iface_name_arg, char *iface2write) {
    struct ifaddrs *addrs, *tmp;

    getifaddrs(&addrs);
    tmp = addrs;

    if (iface_name_arg) {
        int is_iface_available = 0;
        while (tmp) {
            if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET && !strcmp(iface_name_arg, tmp->ifa_name)) {
                printf("interface %s available\n", tmp->ifa_name);
                is_iface_available = 1;
                strcpy(iface2write, iface_name_arg);
                freeifaddrs(addrs);
                return 1;
            }
            tmp = tmp->ifa_next;
        }
        if (!is_iface_available) {
            printf("\tlooks like you specified wrong interface %s\n", iface_name_arg);
            tmp = addrs;
        }
    }

    uint8_t index2choose, count_ifaces = 0;
    printf("available interfaces: \n");
    while (tmp) {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET) {
            printf("\t%d) %s\n", count_ifaces, tmp->ifa_name);
            ++ count_ifaces;
        }
        tmp = tmp->ifa_next;
    }
choose_again:
    printf("type index of interface: ");
    scanf("%hhd", &index2choose);

    if (index2choose == 0 || (index2choose && index2choose > 0 && index2choose < count_ifaces)) {
        tmp = addrs;
        for (uint8_t i; tmp && i < index2choose;) {
            if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET)
                ++i;
            tmp = tmp->ifa_next;
        }
        if (strlen(tmp->ifa_name) < INTERFACE_LENGTH)
            strcpy(iface2write, tmp->ifa_name);
        else
            printf("couldn't write interface name");
    } else {
        printf("\twrong choise\n");
        goto choose_again;
    }

    freeifaddrs(addrs);
    return 1;   // good
}


int choose_options(int argc, char **argv, config_t *conf) {
    if (argc == 1) {
        print_help();
        printf("default config: fqdn \"%s\", max hops %d, default interface\n\n", DEFAULT_FQDN, DEFAULT_MAX_HOPS);
    }
    
    bzero(conf, sizeof(*conf));
    conf->max_hops = DEFAULT_MAX_HOPS;
    
    if (argc > 1 && argv[1][0] != '-')
    strcpy(conf->target_fqdn, argv[1]);

    struct option longopts[] = {
        {"destination", required_argument, NULL, 'd'},
        {"max-hops", required_argument, NULL, 0},
        {"interface", optional_argument, NULL, 'i'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    int opt, longindex;
    char shortopts[] = ":d:hi::";
    while ((opt = getopt_long(
        argc,
        argv,
        shortopts,
        longopts,
        &longindex)) != -1
    ) {
        // printf("getopt_long returned: '%c' (%d)\n", opt, opt);
        switch (opt) {
            case 'd':
                strcpy(conf->target_fqdn, optarg);
                break;
            case 'h':
                print_help();
                return -1;
            case 'i':
                if (optarg == NULL && optind < argc && argv[optind][0] != '-')
                    optarg = argv[optind++];
                choose_interface(optarg, conf->interface);
                // printf("result iface %s\n", conf->interface);
                break;
            case 0:
                if (!strncmp(longopts[longindex].name, "max-hops", 8))
                    conf->max_hops = (uint8_t)strtoul(optarg, (char **)NULL, 10);
                break;
            case ':':
                printf("option -%c needs a value\n", optopt);
                break;
            case '?':
                printf("unknown option: %c\n", optopt);
                break;
            default:
                printf("choose_options case default\n");
        }
    }

    if (argc - optind > 1) {
        printf("\nwhy so many unknown args\n");
        print_help();
        return -2;
    }

    if (!conf->target_fqdn[0])
        strcpy(conf->target_fqdn, DEFAULT_FQDN);

    return 1;   // good
}