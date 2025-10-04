#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int n = atoi(argv[1]);
    if(n==0){
        printf("Please input sleep time!");
        return 0;
    }
    
    sleep(n);
    return 0;
}