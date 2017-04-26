// vim: set et sw=2 ts=2 sts=2:
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE // strptime
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include "vector.h"

typedef enum {
  DIR_NODE,
  FILE_NODE
} nodeType;

typedef struct node {
  nodeType type;
  char *name;
  union {
    struct {
      struct node *parent;
      vector children;
    }; // dir
    struct {
      size_t size;
      struct tm time;
    }; // file
  };
} node;

node *newDirNode(char *name, node *parent) {
  assert(parent == NULL || parent->type == DIR_NODE);
  node *new = malloc(sizeof(node));
  new->type = DIR_NODE;
  new->name = name;
  new->parent = parent;
  vectorInit(&new->children);
  return new;
}

void freeFSTree(node *root) {
  if (root->type == DIR_NODE) {
    for (int i = 0; i < vectorLen(&root->children); ++i)
      freeFSTree(root->children.items[i]);
    vectorFree(&root->children);
  }

  free(root->name);
  free(root);
}

void insertFileNode(node *root, char *path, node *file) {
  assert(root->type == DIR_NODE);
  assert(file->type == FILE_NODE);
  char *delimLoc = strchr(path, '/');

  if (delimLoc) { // slash in path
    int partLen = delimLoc - path; // length of the current part of the path
    char *part = strndup(path, partLen); // the current part of the path

    if (strcmp(part, ".") == 0) { // basically ignore `.'
      insertFileNode(root, delimLoc + 1, file);
      free(part);
      return;
    }

    // look for directory to go into
    for (int i = 0; i < vectorLen(&root->children); ++i) {
      node *child = root->children.items[i];
      if (child->type == DIR_NODE && strcmp(child->name, part) == 0) {
        insertFileNode(child, delimLoc + 1, file);
        free(part);
        return;
      }
    }

    assert(false && "couldn't find directory for file");
  } else { // no slash, so insert file node
    vectorAdd(&root->children, file);
  }
}

void parseFileList(FILE* file, node *root) {
  assert(root->type == DIR_NODE);
  assert(root->parent == NULL);

  int ret, size, day;
  char filePath[100000];
  char month[4] = {0};
  char yearOrTime[10];
  // format: inode #blocks permissions ?? user group size month day {year or time} name
  while ((ret = fscanf(file, " %*d %*d %*s %*d %*s %*s %d %s %d %s %[^\n]\n",
          &size, month, &day, yearOrTime, filePath)) != EOF) {
    //printf("%s\n\tsize: %d\n\tdate: %s %d %s\n", filePath, size, month, day, yearOrTime);

    char datestr[3 + 1 + 2 + 1 + 5 + 1];
    sprintf(datestr, "%s %d %s", month, day, yearOrTime);
    char *res;
    struct tm date = {0};
    if (yearOrTime[2] == ':') { // time, 12:34
      res = strptime(datestr, "%b %d %H:%M", &date);
    } else { // year 2017
      res = strptime(datestr, "%b %d %Y", &date);
    }
    if (!res)
      perror("strptime");
    if (date.tm_year == 0) { // when we have the time, but not the year
      time_t now = time(NULL);
      struct tm *nowt = localtime(&now);
      date.tm_year = nowt->tm_year;
    }

    node *file = malloc(sizeof(node));
    file->type = FILE_NODE;
    char *lastslash = strrchr(filePath, '/');
    assert(lastslash);
    file->name = strdup(lastslash + 1);
    file->size = size;
    file->time = date;
    insertFileNode(root, filePath, file);
    //printf("\tparsed date = %s\n", asctime(&date));
    //printf("\n");
  }
}

void mkdir(const char *path, node *root) {
  assert(root->type == DIR_NODE);
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
      node *child = root->children.items[i];
      if (child->type == DIR_NODE && strcmp(child->name, part) == 0) {
        mkdir(delimLoc + 1, child);
        free(part);
        return;
      }
    }

    // didn't find directory, so make it
    node *new = newDirNode(part, root);
    vectorAdd(&root->children, new);
    mkdir(delimLoc + 1, new);

    free(part);
  } else if (strcmp(path, ".") != 0) { // no slash, so create dir node, don't create `.' nodes
    node *new = newDirNode(strdup(path), root);
    vectorAdd(&root->children, new);
  }
}

node *parseDirs(FILE* dirs) {
  node *root = newDirNode(strdup("/"), NULL);

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

// get path to directory as a vector of dirNode*s
vector dirNodePath(node *d) {
  assert(d->type == DIR_NODE); // TODO: do we need to make work with files?
  vector res;
  vectorInit(&res);

  node *cur = d;
  while (cur) {
    assert(cur->type == DIR_NODE);
    vectorAdd(&res, cur);
    cur = cur->parent;
  }

  // reverse the vector
  int len = vectorLen(&res);
  for (int i = 0; i < len / 2; ++i) {
    void *tmp = res.items[i];
    res.items[i] = res.items[len - i - 1];
    res.items[len - i - 1] = tmp;
  }

  return res;
}

void dirCmd(node *root) {
  assert(root->type == DIR_NODE);
  vector queue;
  vectorInit(&queue);
  vectorAdd(&queue, root);

  while (vectorLen(&queue) != 0) {
    node *next = queue.items[0]; // next node for BFS
    vectorDelete(&queue, 0);

    if (next->type == FILE_NODE)
      continue;

    vector path = dirNodePath(next);
    while (path.items[0] != root)
      vectorDelete(&path, 0);

    vectorDelete(&path, 0); // delete the curdir
    printf("."); // print . for curdir

    while (vectorLen(&path) != 0) {
      printf("/%s", ((node*) path.items[0])->name);
      vectorDelete(&path, 0);
    }
    printf("\n");
    vectorFree(&path);

    for (int i = 0; i < vectorLen(&next->children); ++i) {
      vectorAdd(&queue, next->children.items[i]);
    }
  }

  vectorFree(&queue);
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

  node *root = parseDirs(dirList);
  printf("dirs:\n");
  dirCmd(root);
  parseFileList(fileList, root);

  freeFSTree(root);

  fclose(fileList);
  fclose(dirList);
}
