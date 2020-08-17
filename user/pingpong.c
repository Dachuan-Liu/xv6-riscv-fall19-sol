#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int parent_fd[2];
  int child_fd[2];
  char buff[64];
  pipe(parent_fd);
  pipe(child_fd);
  int pid = fork();
  if( pid == 0)
  {
    read(parent_fd[0], buff, 1);
    fprintf(1, "%d: received ping\n", getpid());
    write(child_fd[1], "0", 1);
  }
  else
  {
    write(parent_fd[1], "0", 1);
    read(child_fd[0], buff, 1);
    fprintf(1, "%d: received pong\n", getpid());
  }
  exit();
}