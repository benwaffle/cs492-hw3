// vim: set et sw=2 ts=2 sts=2:
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

void parseFileList(FILE* file) {
  int fileSize = 0;
  int line = 1;
  char filePath[100000];
  int ret = -1;
  // format: inode #blocks permissions ?? user group size month day {year or time} name
  while ((ret = fscanf(file, " %*i %*i %*s %*i %*s %*s %i %*s %*i %*s %[^\n]\n", &fileSize, filePath)) != EOF) {
    printf("Line %d\t\tFile Size: %8i\t\tFile Path: %s\n", line, fileSize, filePath);
    line++;
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
