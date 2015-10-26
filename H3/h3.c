#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>

#define MAX 100
#define ERROR 999999

int parse_expr( char expr[], int process_id );
int pipe_calc(char* expr);

int pipe_calc(char* expr)
{

  int p[2];  /* array to hold the two pipe file descriptors
            (p[0] is the read end; p[1] is the write end) */
  int rc = pipe( p );
  if ( rc == -1 )
  {
    perror( "pipe() failed\n" );
    return EXIT_FAILURE;
  }

  int pid = fork();
  if ( pid == -1 )
  {
    perror( "fork() failed\n" );
    return EXIT_FAILURE;
  }

  if ( pid == 0 )
  {
    close( p[0] );   /* close the read end of the pipe */
    p[0] = -1;

    char rst_buf[MAX];
    int rst = parse_expr( expr, (int)getpid() );
    // printf("CHILD %d: result is %d\n", (int)getpid(), rst);
    sprintf(rst_buf, "%d", rst);
    write( p[1], rst_buf, strlen(rst_buf) );
    if ( rst != ERROR )
    {
      printf("PROCESS %d: Sending '%s' on pipe to parent\n", (int)getpid(), rst_buf);
    }
    exit(0);
  }
  else /* pid > 0 */
  {
    close( p[1] );  /* close the write end of the pipe */
    p[1] = -1;

    /* wait for the child process to terminate */
    int status, rst = 0;
    pid_t child_pid = wait( &status );

    /* wait() blocks indefinitely */
    if ( WIFSIGNALED( status ) )   /* core dump or kill or kill -9 */
    {
        printf("PARENT %d: child %d terminated abnormally\n", (int)getpid(), (int)child_pid);
    }
    else if ( WIFEXITED(status) ) /* child called return or exit() */
    {
        int rc = WEXITSTATUS(status);
        if ( rc != 0 )
        {
            printf("PARENT %d: child %d terminated with nonzero exit status %d\n", (int)getpid(), (int)child_pid, rc);
        }
        else
        {
          /* read data from the pipe */
          char exp_buf[MAX];
          int bytes_read = read( p[0], exp_buf, MAX - 1 );   /* BLOCKING */
          exp_buf[bytes_read] = '\0';
          rst = atoi(exp_buf);
          // printf("PARENT %d: I got a result %s from pipe\n", (int)getpid(), exp_buf);
        }
    }
    return rst;
  }

}

// recursive function
int parse_expr( char expr[], int process_id )
{
  int rst;
  if ( expr[0] != '(')
  {
    rst = atoi(expr);
  }
  else
  {
    char oprt; // operator
    int oprd[MAX]; // operand
    int oprd_count = 0;
    int i = 1, ret, j;
    char tmp_buf[MAX];
    while ( expr[i] == ' ' ) i++;
    oprt = expr[i];
    i++;

    if (oprt != '+' && oprt != '-' && oprt != '*' && oprt != '/')
    {
      printf("PROCESS %d: ERROR: unknown '%c' operator\n", (int)getpid(), oprt);
      // exit(1);
      return ERROR;
    }
    printf("PROCESS %d: Starting '%c' operation\n", (int)getpid(), oprt);

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
      // fork & pipe, return the result
      memcpy( tmp_buf, expr + start_index, end_index - start_index);
      tmp_buf[end_index - start_index] = '\0';

      ret = pipe_calc(tmp_buf);
      if ( ret == ERROR )
      {
        return ERROR;
      }

      oprd[oprd_count] = ret;
      oprd_count += 1;
      i++;
    }

    if ( oprd_count < 2 )
    {
      printf("ERROR: not enough operands\n");
      return ERROR;
    }

    // compute
    rst = oprd[0];
    if ( oprt == '+')
    {
      for ( j = 1; j < oprd_count; j++)
      {
        rst += oprd[j];
      }
    }
    else if ( oprt == '-')
    {
      for ( j = 1; j < oprd_count; j++)
      {
        rst -= oprd[j];
      }
    }
    else if ( oprt == '*')
    {
      for ( j = 1; j < oprd_count; j++)
      {
        rst *= oprd[j];
      }
    }
    else if (oprt == '/')
    {
      for ( j = 1; j < oprd_count; j++)
      {
        rst /= oprd[j];
      }
    }
  }
  return rst;
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

  int rst = parse_expr(expr_buf, 0);
  if ( rst == ERROR )
  {
    return EXIT_FAILURE;
  }
  printf("PROCESS %d: Final answer is '%d'\n", (int)getpid(), rst);

  return EXIT_SUCCESS;
}
