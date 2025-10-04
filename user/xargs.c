#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

#define stdin 0
#define stdout 1
#define stderr 2
#define arg_len 2048

int 
main(int argc, char *argv[])
{
    if(argc < 2)
    {
        fprintf(stdout,"usage: xargs <command> ...");
        exit(0);
    }
    
    int pid, buf_index = 0, n;
    char arg[arg_len], *args[MAXARG], buf;
    for(int i = 1; i < argc; i++)
    {
        args[i-1] = argv[i];
    }
    
    while((n = read(stdin, &buf, 1)) > 0)
    {
        if(buf == '\n')
        {
            
            arg[buf_index] = 0;

            pid = fork();
            if(pid < 0)
            {
                fprintf(stderr,"xargs error");
                exit(0);
            }
            else if(pid == 0)
            {
                args[argc - 1] = arg;
                args[argc] = 0;
                exec(args[0], args);
            }
            else
            {
                wait((int *)0);
                buf_index = 0;
            }
            
        }
        else arg[buf_index++] = buf;
    }
    exit(0);
}