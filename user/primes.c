#include "kernel/types.h"
#include "user/user.h"

static void
primes(int fd)
{
  int p[2];
  int i;
  int prime;
  read(fd, &prime, 4);
  fprintf(1, "prime %d\n", prime);
  int created = 0;
  while (read(fd, &i, 4) != 0)
  {
    if (!created)
    {
      created = 1;
      pipe(p);
      if (fork() != 0)
      {
        close(p[0]);
      }
      else
      {
        close(p[1]);
        primes(p[0]);
        return;
      }
    }
    if (i % prime != 0)
    {
      write(p[1], &i, 4);
    }
  }
  close(fd);
  close(p[1]);
  wait(0);
}

int
main(int argc, char *argv[])
{
  int p[2];
  pipe(p);

  if (fork() != 0)
  {
    close(p[0]);
    for (int i = 2; i <= 35; i++)
    {
      write(p[1], &i, 4);
    }
    close(p[1]);
    wait(0);
  }
  else
  {
    close(p[1]);
    primes(p[0]);
    close(p[0]);
  }

  exit(0);
}
