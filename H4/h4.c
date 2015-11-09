#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>

#define MAX 100

typedef struct{
  short isfst_oprd; // if it is first oprd
  short * isfst_done; // if the first oprd done
  char oprt;
  char * expr;
  int * rst;
}expr_data;

/* global mutex variable */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int parse_expr( char expr[] );
void critical_section( expr_data * arg,  int rst);
void * thread_calc( void * arg );

void * thread_calc( void * arg )
{
  int rst = parse_expr( ((expr_data *)arg)->expr );
  pthread_mutex_lock( &mutex );
  critical_section( (expr_data *)arg, rst );
  pthread_mutex_unlock( &mutex );
  pthread_exit( NULL );
  return NULL;
}

void critical_section( expr_data * arg,  int rst)
{

  int tmp_rst = *(arg->rst);
  if (arg->oprt == '+')
  {
    *(arg->rst) = tmp_rst + rst;
    printf( "THREAD %u: Adding '%d'\n", (unsigned int)pthread_self(), rst );
  }
  else if (arg->oprt == '-')
  {
    if (arg->isfst_oprd == 1)
    {
      *(arg->rst) = tmp_rst + rst;
      printf( "THREAD %u: '%d' is subtracted \n", (unsigned int)pthread_self(), rst );
    }
    else
    {
      *(arg->rst) = tmp_rst - rst;
      printf( "THREAD %u: Subtracting '%d'\n", (unsigned int)pthread_self(), rst );
    }
  }
  else if (arg->oprt == '*')
  {
    *(arg->rst) = tmp_rst * rst;
    printf( "THREAD %u: Multiplying by '%d'\n", (unsigned int)pthread_self(), rst );
  }
  else
  {
    if (arg->isfst_oprd == 1)
    {
      *(arg->rst) = rst;
      *(arg->isfst_done) = 1;
      printf( "THREAD %u: '%d' is divided\n", (unsigned int)pthread_self(), rst );
    }
    else
    {
      if (*(arg->isfst_done) == 0)
      {
        pthread_mutex_unlock( &mutex );
        while (*(arg->isfst_done) == 0);
        pthread_mutex_lock( &mutex );
      }
      *(arg->rst) = *(arg->rst)/rst;
      printf( "THREAD %u: Dividing by '%d'\n", (unsigned int)pthread_self(), rst );
    }
  }
}

// recursive function
int parse_expr( char expr[] )
{
  if ( expr[0] != '(')
  {
    int rst = atoi(expr);
    return rst;
  }
  else
  {
    char oprt; // operator
    int oprd_count = 0;
    int i = 1, rc, j;
    pthread_t tid[ MAX ];   // keep track of the thread IDs
    char * tmp_expr[ MAX ]; //
    expr_data edata[ MAX ];
    int tcount = 0; // count child threads
    while ( expr[i] == ' ' ) i++;
    oprt = expr[i];
    i++;

    if (oprt != '+' && oprt != '-' && oprt != '*' && oprt != '/')
    {
      printf("THREAD %u: ERROR: unknown '%c' operator\n", (unsigned int)pthread_self(), oprt);
      exit(1);
    }
    printf("THREAD %u: Starting '%c' operation\n", (unsigned int)pthread_self(), oprt);

    int * rst = (int *)malloc( sizeof( int ) );
    short * isfst_done = (short *)malloc( sizeof( short ) );
    *isfst_done = 0;

    if (oprt == '+' || oprt == '-')
    {
      *rst = 0;
    }
    else
    {
      *rst = 1;
    }

    int start_index, end_index;
    expr[strlen(expr)-1] = '\0'; // set the final ')' as '\0'
    while ( expr[i] != '\0')
    {
      // match a operand every loop
      while ( expr[i] == ' ' ) i++;
      start_index = i;
      if ( expr[i] != '(' )
      {
        while ( expr[i] != ' ')
        {
          if ( expr[i] == '\0') break;
          i++;
        }
      }
      else
      {
        int parentheses_count = 1;
        while ( parentheses_count != 0 )
        {
          i++;
          if ( expr[i] == '(' ) parentheses_count += 1;
          if ( expr[i] == ')' ) parentheses_count -= 1;
          if ( expr[i] == '\0')
          {
            printf("ERROR: Invalid expression\n");
            return EXIT_FAILURE;
          }
        }
        i++;
      }
      end_index = i;
      // find an operand
      // creat a thread, return the result
      tmp_expr[tcount] = (char *)malloc( sizeof( char )*(end_index - start_index + 1) );
      memcpy( tmp_expr[tcount], expr + start_index, end_index - start_index);
      tmp_expr[tcount][end_index - start_index] = '\0';

      if ( tcount == 0)
      {
        edata[tcount].isfst_oprd = 1;
      }
      else
      {
        edata[tcount].isfst_oprd = 0;
      }
      edata[tcount].isfst_done = isfst_done;
      edata[tcount].oprt = oprt;
      edata[tcount].expr = tmp_expr[tcount];
      edata[tcount].rst = rst;

      rc = pthread_create( &tid[tcount], NULL, thread_calc , &edata[tcount] );
      tcount++;
      oprd_count++;

      if ( rc != 0 )
      {
        perror( "Could not create the thread" );
      }
    }

    for (j=0; j<tcount; j++)
    {
      rc = pthread_join( tid[j], NULL );  /* BLOCK */
      if (rc != 0)
      {
        printf("THREAD %u: child %u terminated with nonzero exit status %d", (unsigned int)pthread_self(), (unsigned int)tid[j], rc);
        exit(1);
      }
      free(tmp_expr[tcount]);
    }
    int result = *rst;
    free(rst);
    free(isfst_done);

    if ( oprd_count < 2 )
    {
      printf("THREAD %u: ERROR: not enough operands\n", (unsigned int)pthread_self());
      exit(1);
    }
    printf( "THREAD %u: Ended '%c' operation with result '%d'\n", (unsigned int)pthread_self(), oprt, result );
    return result;
  }
}

int main( int argc, char* argv[])
{
  if (argc != 2) {
    printf("ERROR: Invalid arguments\nUSAGE: ./a.out <input-file>\n");
    return EXIT_FAILURE;
  }

  int fd = open( argv[1], O_RDONLY );
  if ( fd == -1 )
  {
    perror( "open() failed\n" );
    return EXIT_FAILURE;
  }
  char expr_buf[100];
  int rc = read(fd, expr_buf, sizeof(expr_buf));
  if ( rc == -1 )
  {
      perror("read() failed\n");
      return EXIT_FAILURE;
  }
  if ( expr_buf[rc-1] == '\n' )
  {
    expr_buf[rc-1] = '\0';
  }
  else
  {
    expr_buf[rc] = '\0';
  }

  int rst = parse_expr(expr_buf);
  printf("THREAD %u: Final answer is '%d'\n", (unsigned int)pthread_self(), rst);

  return EXIT_SUCCESS;
}
