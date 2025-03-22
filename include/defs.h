#include <stdint.h>

#define MY_NI_MAXHOST   1025    // NI_MAXHOST from <netdb.h>, errorlesly glows in vs code
#define MY_NI_NAMEREQD  8       // NI_NAMEREQD from <netdb.h>, too


#define DEFAULT_FQDN        "rhodesia.me.uk"
#define DEFAULT_MAX_HOPS    32
#define INTERFACE_LENGTH    16
// #define DEFAULT_INTERFACE   "enp0s1"

typedef struct {
    char target_fqdn[MY_NI_MAXHOST];
    uint8_t max_hops;
    char interface[INTERFACE_LENGTH];
} config_t;


int choose_options(int argc, char** argv, config_t *conf);

int organize(const config_t *conf);
