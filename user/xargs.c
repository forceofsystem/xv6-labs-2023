#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int _argc = 0;
  char *args[MAXARG];

  if (argc < 2)
  {
    fprintf(2, "usage: xargs command\n");
    exit(1);
  }

  char buf[1024];
  char *p;
  char *tmp;
  
  // while (read(0, buf, sizeof(buf)) != 0)
  // Last line will make a bug
  for (gets(buf, 1024); buf[0] != '\0'; gets(buf, 1024))
  {
    _argc = 0;
    for (int i = 1; i < argc; i++)
    {
      args[_argc++] = argv[i];
    }
    p = buf;
    while (_argc <= MAXARG && *p != '\0')
    {
      tmp = strchr(p, ' ');
      if (tmp == 0)
      {
        tmp = strchr(p, '\n');
        if (tmp != 0)
          *tmp = '\0';
        args[_argc++] = p;
        break;
      }
      *tmp = '\0';
      args[_argc++] = p;
      p = ++tmp;
    }

    if (fork() == 0)
      exec(args[0], args);
    else
      wait(0);
  }

  exit(0);
}
