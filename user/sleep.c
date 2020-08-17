#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    
  int wrong = 0;
  if(argc < 2) wrong = 1;
  int n = atoi(argv[1]);
  if(n <= 0) wrong = 1;
  else sleep(n);
  if(wrong)
  {
    printf("usage: sleep time_to_sleep\n");
  }
  exit();
}