// vim: set et sw=2 ts=2 sts=2:
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

char *mkstring(char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char *res;
  vasprintf(&res, fmt, args);
  va_end(args);
  return res;
}

void parseFileList(FILE* file) {
  int ret, size, day;
  char filePath[100000];
  char month[4] = {0};
  char yearOrTime[10];
  // format: inode #blocks permissions ?? user group size month day {year or time} name
  while ((ret = fscanf(file, " %*i %*i %*s %*i %*s %*s %i %s %i %s %[^\n]\n",
          &size, month, &day, yearOrTime, filePath)) != EOF) {
    printf("%s\n\tsize: %d\n\tdate: %s %d %s\n\n", filePath, size, month, day, yearOrTime);

    char *datestr = mkstring("%s %d %s", month, day, yearOrTime);
    char *res;
    struct tm date;
    if (yearOrTime[2] == ':') { // time, 12:34
      res = strptime(datestr, "%b %d %H:%M", &date);
    } else { // year 2017
      res = strptime(datestr, "%b %d %Y", &date);
    }
    free(datestr);
    if (!res)
      perror("strptime");
  }
}

int main(int argc, char *argv[]) {
  if (argc != 5) {
    fprintf(stderr, "Usage %s\n"
                    "\t<input files storing information on files>\n"
                    "\t<input files storing information on directories>\n"
                    "\t<disk size>\n"
                    "\t<block size>\n",
                argv[0]);
    return 1;
  }
  FILE *f = fopen(argv[1], "r");
  parseFileList(f);
}
