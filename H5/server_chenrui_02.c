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
#include <fcntl.h>
#include <sys/types.h>

#define BUFFER_SIZE 1024
#define BLOCK_SIZE 4096
#define DISK_PLACE_HOLDER '.'

/* global mutex variable */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
  int blocksize;
  int n_blocks;
  int n_free;
  char * buff;
  char** fname_to_label;
  int file_num;
} Disk;

typedef struct
{
  int sock;
  Disk * disk;
} ThreadArg;

void initialize_str_arr(char** hash_arr) {
  int i = 0;
  for (;  i< 26; i++) {
    hash_arr[i] = NULL;
  }
  return;
}

//insert the file name to the hash style array that holds the names of all the files
//return the index of the slot that the file being inserted
//and this index can be used to get the file label in disk's buffer
int insert_to_str_arr(char** hash_arr, char* f_name) {
  int i = 0;
  for (; i < 26; i++) {
    if (hash_arr[i] == NULL) {
      hash_arr[i] = malloc((strlen(f_name) + 1)*sizeof(char));
      strcpy(hash_arr[i], f_name);
      return i;
    }
  }
  return -1;
}

//find the file in the hash style array, if exists, delete it and return i
//else return -1
int delete_file(char** hash_arr, char* f_name) {
  int i = 0;
  for (; i < 26; i++) {
    if (hash_arr[i] != NULL) {
      // printf("the file name is %s\n", hash_arr[i]);
      if (strcmp(hash_arr[i], f_name) == 0) {
        free(hash_arr[i]);
        hash_arr[i] = NULL;
        return i;
      }
    }
  }
  return -1;
}

//if file exists in the hash style array, return the index of that filename
// else return -1
int check_file_exists(char** hash_arr, char* f_name) {
  int i = 0;
  for(; i < 26; i++) {
    if (hash_arr[i] != NULL) {
      if (strcmp(hash_arr[i], f_name) == 0) return i;
    }
  }
  return -1;
}

//get all the files names from hash style array to array of strings
void get_files(char** dest, char** resource, int size) {
  int i = 0;
  int j = 0;
  for (; j < 26; j++) {
    if (resource[j] != NULL) {
      dest[i] = malloc((strlen(resource[j])+1) * sizeof(char));
      strcpy(dest[i], resource[j]);
      i++;
    }
  }
}

//heper function used by quick sort
void swap_str_ptrs(char** arg1, char** arg2)
{
    char* tmp = *arg1;
    *arg1 = *arg2;
    *arg2 = tmp;
}

//helper function to sort the array of the names of files
void quicksort_strs(char** args, unsigned int len)
{
    unsigned int i, pvt=0;
    if (len <= 1) return;
    // swap a randomly selected value to the last node
    swap_str_ptrs(args+((unsigned int)rand() % len), args+len-1);
    // reset the pivot index to zero, then scan
    for (i=0;i<len-1;++i) {
        if (strcmp(args[i], args[len-1]) < 0) swap_str_ptrs(args+i, args+pvt++);
    }

    // move the pivot value into its place
    swap_str_ptrs(args+pvt, args+len-1);
    // and invoke on the subsequences. does NOT include the pivot-slot
    quicksort_strs(args, pvt++);
    quicksort_strs(args+pvt, len - pvt);
}

int init_disk(Disk * disk, int blocksize, int n_blocks)
{
  int i;
  disk->n_blocks = n_blocks;
  disk->blocksize = blocksize;
  disk->n_free = n_blocks;
  disk->buff = malloc(sizeof(char)*n_blocks);
  for (i = 0; i < n_blocks; i++)
  {
    disk->buff[i] = DISK_PLACE_HOLDER;
  }
  disk->fname_to_label = malloc(26 * sizeof(char*));
  initialize_str_arr(disk->fname_to_label);
  disk->file_num = 0;
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
    else new_flg = 1;
    if (count == 0) break;
  }
  disk->file_num++;
  return 0;
}

int disk_delete(Disk * disk, char fd)
{
  int i, n_b = 0;
  for (i = 0;i < disk->n_blocks; i++)
  {
    if (disk->buff[i] == fd)
    {
      disk->buff[i] = DISK_PLACE_HOLDER;
      disk->n_free++;
      n_b++;
    }
  }
  disk->file_num--;
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

int parse_cmd(char * cmd, Disk * disk, int sock)
{
  // printf("the command is %s\n", cmd);
  char * p = NULL;
  char delim[] = " ";
  char filename[100];
  char * saveprt = NULL;
  FILE * fp;
  int ret = 0;
  int n, i = 0;
  if (strcmp(cmd, "DIR\n") == 0)
  {
    // DIR\n
    printf("[thread %d] There are currently %d files on the disk\n", (unsigned int)pthread_self(), disk->file_num);
    char** f_array = malloc(disk->file_num * sizeof(char*));
    get_files(f_array, disk->fname_to_label, disk->file_num);
    quicksort_strs(f_array, disk->file_num);
    for(; i < disk->file_num; i++) {
      // send to client
      sprintf(filename, "%s\n", f_array[i]);
      n = send( sock, filename, strlen(filename), 0 );
      if ( n != strlen(filename) ) printf( "[thread %d] send() failed", (unsigned int)pthread_self());

      printf("%s\n", f_array[i]);
      free(f_array[i]);
      memset(filename, 0, strlen(filename));
    }
    free(f_array);
  }
  else
  {
    p = strtok_r(cmd, delim, &saveprt);
    int n_bytes, n_b, n_w, n_clusters;
    // char file_content[1024];
    int bytes_offset, length;
    if (strcmp(p, "STORE") == 0)
    {
      // ADD <filename> <bytes>\n<file-contents>
      p = strtok_r(NULL, delim, &saveprt);
      if (p == NULL) ret = -1;
      else
      {
        strcpy(filename, p);

        if (check_file_exists(disk->fname_to_label, filename) != -1) {
          perror("ERROR: FILE EXISTS\n");
          return -1;
        }

        p = strtok_r(NULL, "\n", &saveprt);
        if (p == NULL) ret = -1;
        else
        {
          n_bytes = atoi(p);

          if (n_bytes % BLOCK_SIZE == 0) n_b = n_bytes / BLOCK_SIZE;
          else n_b = n_bytes / BLOCK_SIZE + 1;


          // recv file content
          char buffer[BUFFER_SIZE];
          /* can also use read() and write()..... */
          if (n_bytes % BUFFER_SIZE == 0) n_w = n_bytes / BUFFER_SIZE;
          else n_w = n_bytes / BUFFER_SIZE + 1;

          //FILE* f = fopen(serv_name, "w");
          char serv_name[1024];
          snprintf(serv_name, sizeof(serv_name), ".storage/%s", filename);
          fp = fopen(serv_name, "w+");
          if (fp == 0)
          {
            printf( "[thread %d] Could not open file\n", (unsigned int)pthread_self());
            return -1;
          }

          for (i = 0; i < n_w; i++)
          {
            int n = recv( sock, buffer, BUFFER_SIZE, 0 );
            if ( n < 0 )
            {
              perror( "\nrecv() failed" );
              return -2;
            }
            else if ( n == 0 )
            {
              printf( "\n[thread %d] Rcvd 0 from recv(); closing socket\n",
                (unsigned int)pthread_self() );
              return -2;
            }
            else
            {
              buffer[n] = '\0';  /* assuming text.... */
              if (i == 0) printf( "[thread %d] Rcvd: ", (unsigned int)pthread_self());
              printf( "%s", buffer );

              fprintf(fp, "%s", buffer);
            }
          }
          fclose(fp);
          fflush( NULL );

          pthread_mutex_lock( &mutex );
          int ascii_val = insert_to_str_arr(disk->fname_to_label, filename);
          pthread_mutex_unlock( &mutex );

          if (ascii_val == -1) {
            printf("[thread %d] Disk already has enough files\n", (unsigned int)pthread_self());
            //pthread_mutex_unlock( &mutex );
            return -1;
          }

          char label = (char) 65+ascii_val;
          //ret = disk_insert(disk, filename[0], n_b, &n_clusters);
          pthread_mutex_lock( &mutex );
          ret = disk_insert(disk, label, n_b, &n_clusters);
          pthread_mutex_unlock( &mutex );
          if (ret == -1) {
            printf( "[thread %d] Disk is full\n", (unsigned int)pthread_self() );
            return -1;
          }


          printf("[thread %d] Stored file '%c' (%d bytes; %d blocks; ", (unsigned int)pthread_self(), label, n_bytes, n_b);
          if (n_clusters == 1) printf("1 cluster)\n");
          else printf("%d clusters)\n", n_clusters);

          printf("[thread %d] Simulated Clustered Disk Space Allocation:\n", (unsigned int)pthread_self());
          disk_display(disk);
          // }
        }
      }
    }
    else if (strcmp(p, "READ") == 0)
    {
      // READ <filename> <byte-offset> <length>\n
      p = strtok_r(NULL, delim, &saveprt);
      if (p == NULL)
      {
         ret = -1;
      }
      else
      {
        strcpy(filename, p);
        // printf("filename: |%s| ", filename);
        p = strtok_r(NULL, delim, &saveprt);
        if (p == NULL) ret = -1;
        else
        {
          bytes_offset = atoi(p);
          // printf("bytes_offset: %d ", bytes_offset);
          p = strtok_r(NULL, delim, &saveprt);
          if (p == NULL) ret = -1;
          else
          {
            ret = check_file_exists(disk->fname_to_label, filename);
            if ( ret == -1) {
              printf("ERROR: NO SUCH FILE\n");
              return -1;
            }
            char flabel = ret + 65;
            char read_f_name[80];
            strcat(read_f_name, ".storage/");
            strcat(read_f_name, filename);
            length = atoi(p);
            // printf("length: %d\n", length);

            //debug
            // printf("the provided file name for open is |%s|\n", read_f_name);
            //check if the file exits and open the file
            int file=0;
            if((file=open(read_f_name,O_RDONLY)) < -1) {
              perror("OPEN FAILS\n");
              return -1;
            }
            // printf("the file is %d\n", file);
            //get the total size of the file
            struct stat fileStat;
            if(stat(read_f_name,&fileStat) < 0)  {
              //clear filename char array before return for future use
              memset(read_f_name, 0, 80);
              return -1;
            }

            memset(read_f_name, 0, 80);
            unsigned long long file_len = fileStat.st_size;
            // printf("the file length is %llu\n", file_len);

            //check if the range is valid
            if ((bytes_offset < 0) || (length < 0) || (bytes_offset+length > file_len)) {
              printf("ERROR: INVALID BYTE RANGE\n");
              return -1;
            }

            //set start of file to be read
            if(lseek(file, bytes_offset, SEEK_SET) < 0) {
              printf("ERROR: LSEEK FAILS\n");
              return 1;
            }
            //if the length to be read is smaller than the buffer size
            //simply read the content
            char* total_content = malloc((length+2) * sizeof(char));
            if (length < BUFFER_SIZE) {
              read(file,total_content,length);
              total_content[length] = '\n';
              total_content[length+1] = '\0';
              // printf("after reading, total content is %s", total_content);
            } else {
              //if the length is greater than the buffer size
              //read the full content by while loop
              char buffer[BUFFER_SIZE];
              int numloop = length/BUFFER_SIZE;
              if (length % BUFFER_SIZE != 0) numloop += 1;

              int i = 0;
              int remains = length;
              int read_bytes = BUFFER_SIZE;
              for (; i < numloop; i++) {
                if (BUFFER_SIZE > remains) read_bytes = remains;
                if(read(file,buffer,read_bytes) != read_bytes) {
                  printf("error occurs at %dth iteration\n", i);
                  return 1;
                }
                buffer[read_bytes] = '\0';
                //printf("%dth iteration: %s\n", i, buffer);
                strcat(total_content, buffer);
                remains -= read_bytes;
              }

              total_content[length] = '\n';
              total_content[length+1] = '\0';
            }
            // printf("total content is %s", total_content);

            // send the file content
            n = send( sock, total_content, length + 1, 0 );
            if ( n != length + 1 ) printf( "[thread %d] send() failed", (unsigned int)pthread_self());
            else
            {
              printf( "[thread %d] Sent: ACK %d\n",
                      (unsigned int)pthread_self(), length );
              if (length % BUFFER_SIZE == 0) n_b = length / BUFFER_SIZE;
              else n_b = length / BUFFER_SIZE + 1;
              if (n_b == 1)
                printf( "[thread %d] Sent %d bytes (from %d '%c' block) from offset %d\n",
                        (unsigned int)pthread_self(), length,  n_b, flabel, bytes_offset);
              else
                printf( "[thread %d] Sent %d bytes (from %d '%c' blocks) from offset %d\n",
                        (unsigned int)pthread_self(), length,  n_b, flabel, bytes_offset);
            }

            fflush( NULL );

            free(total_content);
          }
        }
      }
    }
    else if (strcmp(p, "DELETE") == 0)
    {
      p = strtok_r(NULL, delim, &saveprt);
      if (p == NULL) ret = -1;
      else {
        strcpy(filename, p);
        // printf("filename: |%s| test\n", filename);
        size_t fn_len = strlen(filename);
        if (filename[fn_len - 1] == '\n') filename[fn_len - 1] = '\0';
        // printf("filename: %s test\n", filename);
        // remove the file

        char del_label;
        //int del_ascii;
        pthread_mutex_lock( &mutex );
        int del_ascii = delete_file(disk->fname_to_label, filename);
        // printf("del_ascii is %d\n", del_ascii);
        if (del_ascii != -1) {
          del_label = (char) 65+del_ascii;
          n_b = disk_delete(disk, del_label);
          char fnamebuffer[80];
          strcat(fnamebuffer, ".storage/");
          strcat(fnamebuffer, filename);
          ret = remove(fnamebuffer);
          memset(fnamebuffer, 0, 80);
        }
        pthread_mutex_unlock( &mutex );
        if (del_ascii == -1) {
          printf("ERROR: NO SUCH FILE\n");
          return -1;
        }
        printf("[thread %d] Deleted %s file '%c' (deallocated %d blocks)\n", (unsigned int)pthread_self(), filename, del_label, n_b);
        printf("[thread %d] Simulated Clustered Disk Space Allocation:\n", (unsigned int)pthread_self());
        disk_display(disk);
      }
    }
    else ret = -1;
  }

  if (ret == -1) printf( "[thread %d] Cannot parse the command\n", (unsigned int)pthread_self());
  return ret;
}

void * thread_job( void * arg )
{
  int n, ret;
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


      ret = parse_cmd(buffer, thread_arg->disk, thread_arg->sock);
      if (ret == -2) break; // remote client side has closed its socket

      /* send ack message back to the client */
      n = send( thread_arg->sock, "ACK\n", 4, 0 );
      printf( "[thread %d] Sent: ACK\n",
              (unsigned int)pthread_self() );

      fflush( NULL );
      if ( n != 4 ) printf( "[thread %d] send() failed", (unsigned int)pthread_self());
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

  //create a directory called storage if it does not exist
  struct stat st = {0};

  if (stat(".storage", &st) == -1) {
    printf("storage directory does not exist, make directory\n");
    if (mkdir(".storage", 0777) < 0) printf("Error: %d\n", mkdir(".storage/directory", 0777));
    else printf("create successfully\n");
  } else {
    system("rm -r .storage/*");
    printf("the storage directory already exists\n");
  }

  unsigned short port = 8765;

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
