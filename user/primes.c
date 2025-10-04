#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void primes(int read_fd){
    int prime;
    int num;
    if(read(read_fd,&prime,sizeof(prime)) == 0) {
        return;
    }
    if(prime) printf("prime %d\n",prime);

    int p[2];
    pipe(p);

    int pid = fork();

    if(pid == 0){
        close(p[1]);
        close(read_fd);
        primes(p[0]);
        close(p[0]);
        exit(0);
    }
    else{
        close(p[0]);
        while(read(read_fd,&num,sizeof(num)) && num!=0){
            if(num % prime == 0) continue;
            write(p[1],&num,sizeof(num));
        }
        close(read_fd);
        close(p[1]);
        wait((int *)0);
        exit(0);
    }
}

int 
main(int argc, char *argv[])
{
    int p[2];
    pipe(p);

    if (fork() == 0) {
        close(p[1]); 
        primes(p[0]);
        close(p[0]);
        exit(0);
    } else {
        close(p[0]); 
        for (int i = 2; i <= 34; i++) {
            write(p[1], &i, sizeof(i));
        }
        close(p[1]);

        wait((int *)0);
        exit(0);
    }
    

    
}