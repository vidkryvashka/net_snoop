#define VERSION "0.0.2"

#include <stdint.h>


#define DEFAULT_FQDN        "rhodesia.me.uk"
#define DEFAULT_MAX_HOPS    32
#define INTERFACE_LENGTH    16

#define MY_NI_MAXHOST 1025  // like NI_MAXHOST in <netdb.h>

typedef struct {
    char target_fqdn[MY_NI_MAXHOST];
    uint8_t max_hops;
    char interface[INTERFACE_LENGTH];
} config_t;


/**
 * @brief parse arguments, assembly of conf
 * 
 * @param conf to fill
 * @return int 
 */
int choose_options(int argc, char* argv[], config_t *conf);

/**
 * @brief organizes sock knocking
 * 
 * @param conf config structure, not necessary, has default values
 * @return int error not implemented
 */
int organize(const config_t *conf);
