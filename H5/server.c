/* server.c */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define BLOCK_SIZE 4096
#define DISK_PLACE_HOLDER '.'
#define ROOT "./.storage/"

/* global mutex variable */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
  int blocksize;
  int n_blocks;
  int n_free;
  char * buff;
} Disk;

typedef struct
{
  int sock;
  Disk * disk;
} ThreadArg;

int init_disk(Disk * disk, int blocksize, int n_blocks)
{
  disk->n_blocks = n_blocks;
  disk->blocksize = blocksize;
  disk->n_free = n_blocks;
  disk->buff = malloc(sizeof(char)*n_blocks);
  for (int i = 0; i < n_blocks; i++)
  {
    disk->buff[i] = DISK_PLACE_HOLDER;
  }
  return 0;
}

int disk_insert(Disk * disk, char fd, int need_blocks, int * n_clusters)
{
  if (need_blocks > disk->n_free) return -1;

  int i, count = need_blocks;
  int new_flg = 1;
  * n_clusters = 0;
  for (i = 0;i < disk->n_blocks; i++)
  {
    if (disk->buff[i] == DISK_PLACE_HOLDER)
    {
      disk->buff[i] = fd;
      disk->n_free--;
      count--;
      if (new_flg == 1) (*n_clusters)++;
      new_flg = 0;
    }
    else
    {
      new_flg = 1;
    }
    if (count == 0) break;
  }
  return 0;
}

int disk_delete(Disk * disk, char fd)
{
  int n_b = 0;
  for (int i = 0;i < disk->n_blocks; i++)
  {
    if (disk->buff[i] == fd)
    {
      disk->buff[i] = DISK_PLACE_HOLDER;
      disk->n_free++;
      n_b++;
    }
  }
  return n_b;
}

int disk_display(Disk * disk)
{
  int i, blocks_per_line = 32;
  for (i = 0; i < blocks_per_line; i++) printf("=");
  printf("\n");
  for (i = 0;i < disk->n_blocks; i++)
  {
    printf("%c", disk->buff[i]);
    if ((i + 1) % blocks_per_line == 0)
    {
      printf("\n");
    }
  }
  for (i = 0; i < blocks_per_line; i++) printf("=");
  printf("\n");
  return 0;
}

int parse_cmd(char * cmd, Disk * disk)
{
  // char * buff;
  char * p = NULL;
  char delim[] = " ";
  // char * root = NULL;
  char filename[100];
  FILE * fp;

  p = strtok(cmd, delim);
  int n_bytes, n_b, n_clusters;
  char file_content[1024];
  int bytes_offset, length;
  int ret = 0;
  if (strcmp(p, "ADD") == 0)
  {
    // ADD <filename> <bytes>\n<file-contents>
    p = strtok(NULL, delim);
    if (p == NULL)
    {
       ret = -1;
    }
    else
    {
      strcpy(filename, p);
      p = strtok(NULL, "\\n");
      if (p == NULL)
      {
         ret = -1;
      }
      else
      {
        n_bytes = atoi(p);

        if (n_bytes % BLOCK_SIZE == 0)
        {
          n_b = n_bytes / BLOCK_SIZE;
        }
        else
        {
          n_b = n_bytes / BLOCK_SIZE + 1;
        }
        p = strtok(NULL, "\r\n");
        if (p == NULL)
        {
          ret = -1;
        }
        else
        {
          strcpy(file_content, p + 1);

          // to be done ????
          pthread_mutex_lock( &mutex );
          ret = disk_insert(disk, filename[0], n_b, &n_clusters);
          pthread_mutex_unlock( &mutex );
          if (ret == -1)
          {
            printf( "[thread %d] Disk is full\n", (unsigned int)pthread_self() );
          }
          else
          {
            if (!access(ROOT, F_OK))
            {
              ret = mkdir(ROOT, S_IRWXU | S_IRWXG | S_IRWXO);
            }
            // root = strcat(root, filename);
            // printf("%s", root);
            fp = fopen(filename, "w+");
            if (fp == 0)
            {
              printf( "[thread %d] Could not open file\n", (unsigned int)pthread_self());
              return -1;
            }
            else
            {
              fprintf(fp, "%s", file_content);
              fclose(fp);
            }
            // ????
            printf("[thread %d] Stored file '%c' (%d bytes; %d blocks; ", (unsigned int)pthread_self(), filename[0], n_bytes, n_b);

            if (n_clusters == 1)
              printf("1 cluster)\n");
            else
              printf("%d clusters)\n", n_clusters);

          }
          printf("[thread %d] Simulated Clustered Disk Space Allocation:\n", (unsigned int)pthread_self());
          disk_display(disk);
        }
      }

    }
  }
  else if (strcmp(p, "READ") == 0)
  {
    // READ <filename> <byte-offset> <length>\n
    p = strtok(NULL, delim);
    if (p == NULL)
    {
       ret = -1;
    }
    else
    {
      strcpy(filename, p);
      printf("filename: %s", filename);
      p = strtok(NULL, delim);
      if (p == NULL)
      {
         ret = -1;
      }
      else
      {
        bytes_offset = atoi(p);
        printf("bytes_offset: %d\n", bytes_offset);
        p = strtok(NULL, delim);
        if (p == NULL)
        {
          ret = -1;
        }
        else
        {
          length = atoi(p);
          printf("length: %d\n", length);
        }
      }
    }

  }
  else if (strcmp(p, "DELETE") == 0)
  {
    // DELETE <filename>\n
    p = strtok(NULL, delim);
    if (p == NULL)
    {
       ret = -1;
    }
    else
    {
      strcpy(filename, p);
      printf("filename: %s", filename);
      // remove the file

      // to do ????
      pthread_mutex_lock( &mutex );
      n_b = disk_delete(disk, filename[0]);
      pthread_mutex_unlock( &mutex );

      printf("[thread %d] Deleted %s file '%c' (deallocated %d blocks)\n", (unsigned int)pthread_self(), filename, filename[0], n_b);
      printf("[thread %d] Simulated Clustered Disk Space Allocation:\n", (unsigned int)pthread_self());
      disk_display(disk);
    }
  }
  else if (strcmp(p, "DIR") == 0)
  {
    // DIR\n
  }
  else
  {
   ret = -1;
  }

  if (ret == -1)
  {
    printf( "[thread %d] Cannot parse the command\n",
                  (unsigned int)pthread_self() );
  }
  return ret;
}

void * thread_job( void * arg )
{
  int n;
  char buffer[ BUFFER_SIZE ];
  ThreadArg * thread_arg = (ThreadArg *) arg;
  do
  {
    /* can also use read() and write()..... */
    n = recv( thread_arg->sock, buffer, BUFFER_SIZE, 0 );

    if ( n < 0 )
    {
      perror( "recv() failed" );
      printf("error");
    }
    else if ( n == 0 )
    {
      printf( "[thread %d] Rcvd 0 from recv(); closing socket\n",
        (unsigned int)pthread_self() );
    }
    else
    {
      buffer[n] = '\0';  /* assuming text.... */
      printf( "[thread %d] Rcvd: %s\n",
              (unsigned int)pthread_self(),
              buffer );


      parse_cmd(buffer, thread_arg->disk);
      /* send ack message back to the client */
      n = send( thread_arg->sock, "ACK\n", 4, 0 );
      printf( "[thread %d] Sent: ACK\n",
              (unsigned int)pthread_self() );

      fflush( NULL );
      if ( n != 4 )
      {
        printf( "[thread %d] send() failed", (unsigned int)pthread_self());
      }
    }
  }
  while ( n > 0 );
  /* this do..while loop exits when the recv() call
     returns 0, indicating the remote/client side has
     closed its socket */

  printf( "[thread %d] Client closed its socket....terminating\n", (unsigned int)pthread_self() );
  // close( thread_arg->sock );
  exit( EXIT_SUCCESS );  /* child terminates here! */
}

int main()
{
  int n_blocks = 128;

  /* Create the listener socket as TCP socket */
  int sock = socket( PF_INET, SOCK_STREAM, 0 );
  /* sock is the socket descriptor */
  /*  and SOCK_STREAM == TCP       */

  if ( sock < 0 )
  {
    perror( "socket() failed" );
    exit( EXIT_FAILURE );
  }

  /* socket structures */
  struct sockaddr_in server;

  server.sin_family = PF_INET;
  server.sin_addr.s_addr = INADDR_ANY;

  unsigned short port = 8000;

  /* htons() is host-to-network-short for marshalling */
  /* Internet is "big endian"; Intel is "little endian" */
  server.sin_port = htons( port );
  int len = sizeof( server );

  if ( bind( sock, (struct sockaddr *)&server, len ) < 0 )
  {
    perror( "bind() failed" );
    exit( EXIT_FAILURE );
  }
  printf("Block size is %d\nNumber of blocks is %d\n", BLOCK_SIZE, n_blocks);
  printf( "Listening on port %d\n", port );
  listen( sock, 5 );   /* 5 is the max number of waiting clients */

  struct sockaddr_in client;
  int fromlen = sizeof( client );


  // init simulated disk
  Disk disk;
  init_disk(&disk, BLOCK_SIZE, n_blocks);

  int rc;

  while ( 1 )
  {
    // printf( "PARENT: Blocked on accept()\n" );
    int newsock = accept( sock, (struct sockaddr *)&client,
                          (socklen_t*)&fromlen );

    printf( "Received incoming connection from %s\n", inet_ntoa( (struct in_addr)client.sin_addr));
    ThreadArg thread_arg = {.sock = newsock, .disk = &disk};
    pthread_t tid;
    rc = pthread_create( &tid, NULL, thread_job , &thread_arg );
    if ( rc != 0 )
    {
      perror( "Could not create the thread" );
    }

  }

  close( sock );
  free(disk.buff);
  return EXIT_SUCCESS;
}
