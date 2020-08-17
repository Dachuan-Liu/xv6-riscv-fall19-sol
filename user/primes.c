#include "kernel/types.h"
#include "user/user.h"


int
main(int argc, char *argv[])
{
  int pipe_fd[2];
  pipe(pipe_fd);
  if(fork() != 0)
  {
    close(pipe_fd[0]);
    for(int i = 2; i <= 35; ++i) write(pipe_fd[1], &i, sizeof(int));
    close(pipe_fd[1]);
  }
  else
  {
    close(pipe_fd[1]);
    int rcv_fd = pipe_fd[0];
    int number;
    int cnt = 1;
    int hasChild = 0;
    int base = 1;
    while(read(rcv_fd, &number, sizeof(int)) != 0)
    {
      if(cnt == 1)
      {
        base = number;
        fprintf(1, "prime %d\n", base);
        ++cnt;
        continue;
      }
      int cannotDivide = (number % base != 0);
      if(cannotDivide)
      {
        if(!hasChild)
        {
          pipe(pipe_fd);
          if(fork() == 0)
          {
            close(rcv_fd);
            close(pipe_fd[1]);
            rcv_fd = pipe_fd[0];
            cnt = 1;
            continue;
          }
          close(pipe_fd[0]);
          hasChild = 1;
        }
        write(pipe_fd[1], &number, sizeof(int));
      }
      /*if(cannotDivide && !hasChild)
      {
        pipe(pipe_fd);
        if(fork() != 0)
        {
          close(pipe_fd[0]);
          hasChild = 1;
        }
        else
        {
          close(rcv_fd);
          close(pipe_fd[1]);
          rcv_fd = pipe_fd[0];
          cnt = 1;
          continue;
        }
      }
      if(cannotDivide) write(pipe_fd[1], &number, sizeof(int));*/
      ++cnt;
    }
    close(rcv_fd);
    close(pipe_fd[1]);
  }
  wait();
  exit();
}