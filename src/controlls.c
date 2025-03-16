#include "defs.h"

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h> // bzero
#include <stdlib.h>

static void print_help() {
    printf("\n\
    needs to be running with root\n\n\
    ./path-2-program <char *fqdn> \t\tsame as with -d --destination \tdefault %s\n\
    -d --destination <char *fqdn>\t\tspecify destination FQDN\n\
    -h --max-hops <uint8_t hops>\t\tspecify max hop (ttl) count, \tdefault %d\n\
    -i --interface <optional char *interface>\tallows to choose interface, not implemented\n\
    --help\n\n",
    DEFAULT_FQDN, DEFAULT_MAX_HOPS);
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
    char shortopts[] = ":d:hi";
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
                    printf("choosing interface feature not implemented, arg: %s\n", optarg);
                break;
            case 0:
                if (!strncmp(longopts[longindex].name, "max-hops", 8)) {
                    conf->max_hops = (uint8_t)strtoul(optarg, (char **)NULL, 10);
                    printf("max hops %s\n", optarg);
                }
                break;
            case ':':
                printf("option -%c needs a value\n", optopt);
                break;
            case '?':
                printf("unknown option: %c\n", optopt);
                break;
        }
    }

    // printf("unknown args diff: %d %d\n", optind, argc);
    if (argc - optind > 1) {
        printf("\nwhy so many unknown args\n");
        print_help();
        return -2;
    }

    if (!conf->target_fqdn[0])
        strcpy(conf->target_fqdn, DEFAULT_FQDN);

    return 0;
}