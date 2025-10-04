#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int 
main(int argc, char *argv[])
{
    int p1[2];
    int p2[2];
    pipe(p1);
    pipe(p2);

    char buffer[100];

    int pid = fork();
    if(pid<0)
    {
        return 0;
    }

    if(pid == 0)//children
    {
        close(p1[1]);
        close(p2[0]);

        write(p2[1],"ping\0",5);
        read(p1[0],buffer,sizeof(buffer)-1);
        
        int pid = getpid();
        close(p1[0]);
        close(p2[1]);
        
        printf("%d: received %s\n",pid,buffer);
        exit(0);
    }
    else
    {
        close(p1[0]);
        close(p2[1]);
        write(p1[1],"pong\0",5);
        read(p2[0],buffer,sizeof(buffer)-1);
        int pid = getpid();
        close(p1[1]);
        close(p2[0]);
        wait((int *)0);
        printf("%d: received %s\n",pid,buffer);
        exit(0);
    }
    return 0;
}