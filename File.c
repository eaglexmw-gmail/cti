#include <glob.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "File.h"


ArrayU8 * File_load_data(String * filename)
{
  long len;
  long n;
  int procflag = 0;
  FILE *f = fopen(s(filename), "rb");

  if (!f) {
    perror(s(filename));
    return 0L;
  }

  if (strncmp("/proc/", s(filename), strlen("/proc/")) == 0) {
    len = 32768;
    procflag = 1;
  }
  else {
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
  }

  ArrayU8 *a = ArrayU8_new();
  ArrayU8_extend(a, len);
  n = fread(a->data, 1, len, f);
  if (n < len && !procflag) {
    fprintf(stderr, "warning: only read %ld of %ld expected bytes from %s\n", n, len, s(filename));
  }
  a->data[n] = 0;

  fclose(f);

  return a;
}


String * File_load_text(String * filename)
{
  ArrayU8 *a = File_load_data(filename);
  String *s = ArrayU8_to_string(&a);
  return s;
}


String_list *Files_glob(String *path, String *pattern)
{
  glob_t g;
  int i;
  String_list *slst = String_list_new();
  //fprintf(stderr, "%s\n", s(path));
  String *full_pattern = String_sprintf("%s/%s", s(path), s(pattern));
  int rc = glob(s(full_pattern), 0,
		NULL,
		&g);
  if (rc == 0) {
    for (i=0; i < g.gl_pathc; i++) {
      String *tmp = String_new(g.gl_pathv[i]);
      String_list_add(slst, &tmp);
    }
    globfree(&g);
  }
  return slst;
}


int File_exists(String *path)
{
  return access(s(path), R_OK) == 0 ? 1 : 0;
}
