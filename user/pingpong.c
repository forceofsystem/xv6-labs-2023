#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int p[2];
  if (argc > 1){
    fprintf(2, "Usage: pingpong\n");
    exit(1);
  }

  pipe(p);
  if (fork() == 0) {
    // child
    char buf[1];
    read(p[0], buf, 1);
    printf("%d: received ping\n", getpid());
    write(p[1], "p", 1);
    exit(0);
  } else {
    // parent
    write(p[1], "p", 1);
    char buf[1];
    read(p[0], buf, 1);
    printf("%d: received pong\n", getpid());
  }
  
  exit(0);
}
