#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"

static void
find(char *path, char *name)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(path, O_RDONLY)) < 0)
  {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0)
  {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if (st.type != T_DIR)
  {
    fprintf(2, "find: %s is not a directory\n", path);
    close(fd);
    return;
  }

  if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
  {
    fprintf(2, "find: path too long\n");
    close(fd);
    return;
  }
  strcpy(buf, path);
  p = buf + strlen(buf);
  *p++ = '/';

  while (read(fd, &de, sizeof(de)) == sizeof(de))
  {
    if (de.inum == 0)
      continue;
    if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
      continue;
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;
    if (stat(buf, &st) < 0)
    {
      fprintf(2, "find: cannot stat %s\n", path);
      continue;
    }
    switch (st.type)
    {
    case T_FILE:
      if (strcmp(de.name, name) == 0)
      {
        //write(1, buf, strlen(buf));
        //write(1, "\n", 1);
        fprintf(1, "%s\n", buf);
      }
      break;
    case T_DIR:
      find(buf, name);
      break;
    }
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if (argc != 3)
  {
    fprintf(2, "Usage: find <path> <name>\n");
    exit(1);
  }

  find(argv[1], argv[2]);

  exit(0);
}
