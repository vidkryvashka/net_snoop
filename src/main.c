#include "defs.h"


#include <stdio.h>
#include <unistd.h>



int main(int argc, char** argv) {

    config_t conf;

    if (!choose_options(argc, argv, &conf))
        organize((const config_t *)&conf);

    // printf("end program\n");
    return 0;
}
