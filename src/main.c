#include "defs.h"


#include <stdio.h>
#include <unistd.h>



int main(int argc, char** argv) {

    config_t conf;

    if (choose_options(argc, argv, &conf) > 0)
        organize((const config_t *)&conf);

    return 0;
}
