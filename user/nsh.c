#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/fcntl.h"
#include "user/user.h"

void printCmd(char argv_buf[MAXARG][MAXARG], int left, int right);
void printArgv(char *argv[]);
int stripNewline(char *p, int index);
int parseCmd(char linebuf[MAXARG * MAXARG], char argv_buf[MAXARG][MAXARG]);
void runCmd(char argv_buf[MAXARG][MAXARG], int left, int right); // argv_buf[left, right]
int Fork(void);
int Pipe(int*);
int Close(int);
int Open(const char*, int);

int getOneLine(char linebuf[MAXARG * MAXARG])
{
  int n = 0;
  int cnt = 0;
  while((n = read(0, linebuf + cnt, sizeof(char))) > 0)
  {
    if(linebuf[cnt] == '\n')
    {
      linebuf[cnt] = '\0';
      break;
    }
    ++cnt;
  }
  return cnt;
}

int
main(int argc, char *argv[])
{
  char argv_buf[MAXARG][MAXARG];
  char linebuf[MAXARG * MAXARG];
  //char extrabuf[MAXARG * MAXARG];
  int n;
  
  memset(linebuf, 0, sizeof(linebuf));
  //memset(extrabuf, 0, sizeof(extrabuf));
  fprintf(1, "@ ");
  while((n = getOneLine(linebuf)) > 0)
  {
    
    //int curr = 0, prev = 0;
    
    
    //if( (curr = stripNewline(extrabuf, prev)) > 0)
    
      //strcpy(linebuf, extrabuf + prev);
      //fprintf(2, "cmd: %s\n", linebuf);
      memset(argv_buf, 0, sizeof(argv_buf));
      //fprintf(2, "before parsing\n");
      int numArg = parseCmd(linebuf, argv_buf);
      //fprintf(2, "after parsing\n");
      if(Fork() == 0)
      {
        runCmd(argv_buf, 0, numArg - 1);
        exit(0);
      }
      wait(0);
      //prev = curr;
    
    
    fprintf(1, "@ ");
  }
  exit(0);
}

void printArgv(char *argv[])
{
  int cnt = 1;
  fprintf(2, "%s:", argv[0]);
  for(cnt = 1; strlen(argv[cnt]) > 0; ++cnt)
  {
    if(cnt > 1) fprintf(2, ",");
    fprintf(2, " %s", argv[cnt]);
  }
  fprintf(2, " %d arguments\n", cnt);
  
}
void printCmd(char argv_buf[MAXARG][MAXARG], int left, int right)
{
  fprintf(2, "%s:", argv_buf[left]);
  for(int i = left + 1; i <= right && strlen(argv_buf[i]) > 0; ++i)
  {
    if(i > left + 1) fprintf(2, ",");
    fprintf(2, " %s", argv_buf[i]);
  }
  fprintf(2, " %d arguments\n", right - left + 1);
}
int stripNewline(char *p, int index)
{
  while(1)
  {
    if(p[index] == '\n')
    {
      p[index] = '\0';
      return index + 1;
    }
    if(p[index] == '\0') return -1;
    ++index;
  }
}
int parseCmd(char linebuf[MAXARG * MAXARG], char argv_buf[MAXARG][MAXARG])
{
  //fprintf(2, "parsing\n");
  int len = strlen(linebuf);
  char buf[MAXARG * MAXARG];
  strcpy(buf, linebuf);
  for(int i = 0; i < MAXARG * MAXARG; ++i)
  {
    if(buf[i] == ' ') buf[i] = '\0';
  }
  int cntArg = 0;
  int inWord = 0;
  int i = 0;
  while(i < len)
  {
    if(buf[i] == '\0')
    {
      if(inWord) inWord = 0;
      ++i;
      continue;
    }
    strcpy(argv_buf[cntArg++], &buf[i]);
    i += strlen(&buf[i]);
  }
  //printCmd(argv_buf, 0, cntArg - 1);
  return cntArg;
}

void runCmd(char argv_buf[MAXARG][MAXARG], int left, int right)
{
  int pipe_fd[2];
  char *argv_exec[MAXARG];
  //fprintf(2, "processing ");
  //printCmd(argv_buf, left, right);
  int indexPipe = -1;
  for(int i = left; i <= right; ++i)
  {
    if(!strcmp(argv_buf[i], "|"))
    {
      //fprintf(2, "| found at argv_buf[%d]\n", i);
      indexPipe = i;
      break;
    }
  }
  if(indexPipe > 0)
  {
    Pipe(pipe_fd);
    if(Fork() == 0)
    {
      Close(1);
      Close(pipe_fd[0]);
      dup(pipe_fd[1]);
      runCmd(argv_buf, left, indexPipe - 1);
      exit(0);
    }
    if(Fork() == 0)
    {
      Close(0);
      Close(pipe_fd[1]);
      dup(pipe_fd[0]);
      runCmd(argv_buf, indexPipe + 1, right);
      exit(0);
    }
    Close(pipe_fd[0]);
    Close(pipe_fd[1]);
    wait(0);
    //fprintf(2, "wait return from left\n");
    wait(0);
    //fprintf(2, "wait return from right\n");
    
  }
  else
  {
    memset(argv_exec, 0, sizeof(argv_exec));
    int iArg = 0;
    for(int i = left; i <= right; ++i)
    {
      int inputRedirect = !strcmp(argv_buf[i], "<");
      int outputRedirect = !strcmp(argv_buf[i], ">");
      if(inputRedirect)
      {
        int fd = Open(argv_buf[i + 1], O_RDONLY);
        Close(0);
        dup(fd);
        ++i;
        continue;
      }
      if(outputRedirect)
      {
        int fd = Open(argv_buf[i + 1], O_WRONLY|O_CREATE);
        Close(1);
        dup(fd);
        ++i;
        continue;
      }
      argv_exec[iArg++] = argv_buf[i];
      //fprintf(2, "%s\n", argv_exec[i - left]);
    }
    exec(argv_exec[0], argv_exec);
  }
}

int Fork(void)
{
  int val = fork();
  if(val < 0)
  {
    fprintf(2, "fork failed!\n");
    exit(1);
  }
  return val;
}
int Pipe(int* pipe_fd)
{
  int val = pipe(pipe_fd);
  if(val < 0)
  {
    fprintf(2, "pipe failed!\n");
    exit(1);
  }
  return val;
}

int Close(int fd)
{
  int val = close(fd);
  if(val < 0)
  {
    fprintf(2, "close failed!\n");
    exit(1);
  }
  return val;
  
}
int Open(const char* name, int mode)
{
  int val = open(name, mode);
  if(val < 0)
  {
    fprintf(2, "open failed!\n");
    exit(1);
  }
  return val;
}