// vim: set et sw=2 ts=2 sts=2:
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "vector.h"

typedef struct dirNode {
  char *name;
  struct dirNode *parent;
  vector children;
} dirNode;

char *mkstring(char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char *res;
  vasprintf(&res, fmt, args);
  va_end(args);
  return res;
}

dirNode *newDirNode(char *name, dirNode *parent) {
  dirNode *new = malloc(sizeof(dirNode));
  new->name = name;
  new->parent = parent;
  vectorInit(&new->children);
  return new;
}

void parseFileList(FILE* file) {
  int ret, size, day;
  char filePath[100000];
  char month[4] = {0};
  char yearOrTime[10];
  // format: inode #blocks permissions ?? user group size month day {year or time} name
  while ((ret = fscanf(file, " %*d %*d %*s %*d %*s %*s %d %s %d %s %[^\n]\n",
          &size, month, &day, yearOrTime, filePath)) != EOF) {
    printf("%s\n\tsize: %d\n\tdate: %s %d %s\n", filePath, size, month, day, yearOrTime);

    char *datestr = mkstring("%s %d %s", month, day, yearOrTime);
    char *res;
    struct tm date = {0};
    if (yearOrTime[2] == ':') { // time, 12:34
      res = strptime(datestr, "%b %d %H:%M", &date);
    } else { // year 2017
      res = strptime(datestr, "%b %d %Y", &date);
    }
    free(datestr);
    if (!res)
      perror("strptime");
    if (date.tm_year == 0) { // when we have the time, but not the year
      time_t now = time(NULL);
      struct tm *nowt = localtime(&now);
      date.tm_year = nowt->tm_year;
    }
    printf("\tparsed date = %s\n", asctime(&date));
    printf("\n");
  }
}

void mkdir(const char *path, dirNode *root) {
  char *delimLoc = strchr(path, '/');

  if (delimLoc) { // slash in path
    int partLen = delimLoc - path; // length of the current part of the path
    char *part = strndup(path, partLen); // the current part of the path

    if (strcmp(part, ".") == 0) { // basically ignore `.'
      mkdir(delimLoc + 1, root);
      free(part);
      return;
    }

    // look for directory to go into
    for (int i = 0; i < vectorLen(&root->children); ++i) {
      dirNode *child = root->children.items[i];
      if (strcmp(child->name, part) == 0) {
        mkdir(delimLoc + 1, child);
        free(part);
        return;
      }
    }

    // didn't find directory, so make it
    dirNode *new = newDirNode(part, root);
    vectorAdd(&root->children, new);
    mkdir(delimLoc + 1, new);

    free(part);
  } else { // no slash, create dir node
    dirNode *new = newDirNode(strdup(path), root);
    vectorAdd(&root->children, new);
  }
}

dirNode *parseDirs(FILE* dirs) {
  dirNode *root = newDirNode("/", NULL);

  char *line = NULL;
  size_t len = 0;
  ssize_t nread;
  while ((nread = getline(&line, &len, dirs)) != -1) {
    if (line[nread-1] == '\n')
      line[nread-1] = '\0';
    mkdir(line, root);
  }

  free(line);
  return root;
}

  }
}

void printUsage(char *argv0) {
  fprintf(stderr, "Usage: %s -f file_list.txt -d dir_list.txt -s <disk size> -b <block size>\n", argv0);
}

int main(int argc, char *argv[]) {
  FILE *fileList = NULL, *dirList = NULL;
  int diskSize = 0, blockSize = 0;

  int opt;
  while ((opt = getopt(argc, argv, "f:d:s:b:")) != -1) {
    switch (opt) {
      case 'f':
        fileList = fopen(optarg, "r");
        if (!fileList) {
          perror("fopen");
          return 1;
        }
        break;
      case 'd':
        dirList = fopen(optarg, "r");
        if (!dirList) {
          perror("fopen");
          return 1;
        }
        break;
      case 's':
        diskSize = atoi(optarg);
        break;
      case 'b':
        blockSize = atoi(optarg);
        break;
      default:
        printUsage(argv[0]);
        return 1;
    }
  }

  if (!fileList || !dirList || !diskSize || !blockSize) {
    printUsage(argv[0]);
    return 1;
  }

  dirNode *root = parseDirs(dirList);
  parseFileList(fileList);

  fclose(fileList);
  fclose(dirList);
}
