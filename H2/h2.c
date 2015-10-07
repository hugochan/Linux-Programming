#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char* argv[])
{
    if (argc != 3) {
        printf("ERROR: Invalid arguments\nUSAGE: ./a.out <input-file> <chunk-size>\n");
        return EXIT_FAILURE;
    }

    int chunk_size = atoi(argv[2]);
    struct stat sb;
    if (lstat(argv[1], &sb) == -1) {
        perror("lstat() failed\n");
        exit(EXIT_FAILURE);
    }
    int child_proc_count = sb.st_size/chunk_size;
    if ( child_proc_count * atoi(argv[2]) < sb.st_size )
    {
        child_proc_count++;
    }
    printf("PARENT: File '%s' contains %lld bytes\n", argv[1], sb.st_size);
    printf("PARENT: ... and will be processed via %d child processes\n", child_proc_count);
    int fd = open( argv[1], O_RDONLY );
    if ( fd == -1 )
    {
        perror( "open() failed\n" );
        return EXIT_FAILURE;
    }

    int i;
    pid_t pid;
    for ( i = 0; i < child_proc_count; i++ )
    {
        pid = fork();  /* create a child process */
        if ( pid == -1 )
        {
            perror( "fork() failed\n" );
            return EXIT_FAILURE;
        }
        
        if ( pid == 0 )  /* child process fork() returns 0 */
        {
            char buffer[chunk_size + 1];
            int rc = read(fd, buffer, chunk_size);
            if ( rc == -1 )
            {
                perror("read() failed\n");
                return EXIT_FAILURE;
            }
            buffer[rc] = '\0';
            printf("CHILD %d CHUNK: %s\n", getpid(), buffer);
            /* sleep( 5 ); */
            return 0;
        }
        else /* pid > 0 */  /* parent process fork() returns the child's pid */
        {
            /* wait for the child process to terminate */
            int status;
            pid_t child_pid = wait(&status);
            /* wait() blocks indefinitely */
            
            if ( WIFSIGNALED(status) )   /* core dump or kill or kill -9 */
            {
                printf("PARENT: child %d terminated abnormally\n", (int)child_pid);
            }
            else if ( WIFEXITED(status) ) /* child called return or exit() */
            {
                int rc = WEXITSTATUS(status);
                if ( rc != 0 )
                {
                    printf("PARENT: child %d terminated with nonzero exit status %d\n", (int)child_pid, rc);
                }
            }
        }
    }
    return EXIT_SUCCESS;
}