#include<stdio.h>
#include<stdlib.h>
#include<string.h>

int main(int argc, char* argv[])
{
  if (argc != 3) 
  {
    printf("args error\n");
    return EXIT_FAILURE;
  }
  
  /*pre-allocate the memory*/
  #define BASE_SIZE 20
  char **words = (char**)calloc(BASE_SIZE, sizeof(char*)); 
  char **newwords = NULL;
  char *word = NULL;
  char buffer[1024]; /*buffer to store each word*/ 
  int i, j, len, count = 0, current_size = BASE_SIZE;
  /*read the file into memory*/
  FILE *fp;
  fp = fopen(argv[1], "r"); 
  if (fp == 0) /* fopen returns 0, the NULL pointer, on failure */
  {
    printf( "Could not open file\n" );
    return EXIT_FAILURE;
  }
  else 
  {
    printf("Allocated initial array of %d character pointers.\n", BASE_SIZE);
    count = 0;
    fscanf(fp, "%s ", buffer);
    while ((len = strlen(buffer)) > 0)
    {
      word = (char*)malloc((len+1)*sizeof(char));
      if (!word)
      {
        printf("Failed to allocate space.\n");
        return EXIT_FAILURE;
 
      }
      memcpy(word, buffer, len);
      word[len] = 0;
      if (current_size <= count)
      {
        current_size = 2*current_size;
        newwords = (char**)realloc(words, current_size*sizeof(char*));
        if(!newwords)
        {
          printf("Failed to reallocate new space.\n");
          return EXIT_FAILURE;
        }
        words = newwords;
        printf("Reallocated array of %d character pointers.\n", current_size);
      }
      words[count] = word;
      memset(buffer, 0, 1024);
      fscanf(fp, "%s ", buffer);
      count++;
    }
    fclose(fp); 
  }
  
 /* printf("\nwords array:\n");
  for(i=0;i<count;i++)
  {
    printf("%s ", words[i]);
  }  
  printf("\n");
 */

  /*match words*/
  char* pre_fix = argv[2];
  int pre_len = strlen(pre_fix);
  for(i=0;i<count;i++)
  {
    for(j=0;j<pre_len;j++)
    {
      if(words[i][j] != pre_fix[j])
      {
        break;	
      }
    }
    if(j==pre_len) printf("%s\n", words[i]); /*print out the matched words*/
  }
  
  /*free dynamic address*/ 
  for(i=0;i<count;i++)
  {
    free(words[i]);
    words[i] = NULL;
  }
  free(words);
  words = NULL;

  return EXIT_SUCCESS;
}
