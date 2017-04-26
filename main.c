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

void findOrCreateChild(char* path, dirNode *root) {
  const char s[2] = "/";
  char *token;
  token = strtok(path, s);

  dirNode *current = root;

  while(token != NULL) {
    bool nodeFound = false;
    if (strcmp(token, ".")) {
      // at root directory
      printf("Skipping root node\n");
    } else {
      printf("Checking for %s\n", token);
      // check all children of current node for token, if found, set current to that child, else create node
      for (int i = 0; i < vectorLen(&current->children); i++) {
        if(strcmp((((dirNode*)(current->children.items[i]))->name), token)) {
          current = &current->children.items[i];
          printf("Found node %s\n", current->name);
          nodeFound = true;
          break;
        }
      }
      if (nodeFound == false) {
        //create new node
        dirNode new = {
          .name = token,
          .parent = current
        };
        vectorAdd(&current->children, &new);
        current = &new;
        printf("Creating node %s\n", current->name);
      }
      // printf("%s\n", token);
    }
    token = strtok(NULL, s);

  }
}

void parseDirectoryStructure(FILE* file) {
  dirNode root = {
    .name = "/",
    .parent = NULL
  };
  vectorInit(&root.children);
  int ret;
  char str[10000];
  while ((ret = fscanf(file, "%s\n", str)) != EOF) {
    findOrCreateChild(str, &root);
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

  parseFileList(fileList);
  parseDirectoryStructure(dirList);

  fclose(fileList);
  fclose(dirList);
}
