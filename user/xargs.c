#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int readOneWord(int index, char buf[MAXARG][MAXARG], int *pstatus);
int readOneLine(int n_base, char buf[MAXARG][MAXARG], int *pstatus); // return num of extra arguments, -1 for EOF

void printArgv(char *argv[])
{
  for(int i = 0; i < MAXARG && strlen(argv[i]) > 0; ++i)
  {
    if(i > 0) fprintf(1, " ");
    fprintf(1, " %s", argv[i]);
  }
  fprintf(1, " \n");
  
}

int
main(int argc, char *argv[])
{
  //int arg;
  char argv_buf[MAXARG][MAXARG];
  char *param[MAXARG];
  int n, status;
  for(int i = 1; i < argc; ++i) strcpy(argv_buf[i - 1],  argv[i]);
  for(int i = argc - 1; i < MAXARG; ++i) argv_buf[i][0] = '\0';
  while(1)
  {
    n = readOneLine(argc - 1, argv_buf, &status);
    
    if(n > 0 || (n == 0 && status == 1))
    {
      memset(param, 0, sizeof(param));
      for(int i = 0; i < argc + n; ++i) param[i] = argv_buf[i];
      if(fork() == 0)
      {
        exec(param[0], param);
        fprintf(1, "error executing ");
        printArgv(param);
        exit();
      }
      else
      {
        wait();
      }
    }
    
    /*
    for(int i = 0; i < argc - 1 + n; ++i)
    {
      if(i > 0) fprintf(1, " ");
      fprintf(1, "%s", argv_buf[i]);
    }
    fprintf(1, "\n");*/
    if(status == 2) break;
    
  }
  
  exit();
}

int readOneLine(int n_base, char buf[MAXARG][MAXARG], int *pstatus)
{
  for(int i = n_base; i < MAXARG; ++i)
  {
    int n = readOneWord(i, buf, pstatus);
    if(n <= 0 || *pstatus != 0)
    {
      return i - n_base;
    }
  }
  return MAXARG - n_base;
}

int readOneWord(int index, char buf[MAXARG][MAXARG], int *pstatus)
{
  *pstatus = 0;
  int n;
  char c;
  int cnt = 0;
  int isInWord = 0;
  while(1)
  {
    n = read(0, &c, sizeof(char));
    if(n <= 0) // EOF
    {
      *pstatus = 2;
      break;
    }
    if(c == '\n') // end of line
    {
      //fprintf(1, "new line read\n");
      *pstatus = 1;
      break;
    }
    if(c == ' ')
    {
      if(isInWord) break;
      else continue;
    }
    if(!isInWord) isInWord = 1;
    buf[index][cnt] = c;
    ++cnt;
  }
  buf[index][cnt] = '\0';
  return cnt;
}
