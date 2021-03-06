/*
 * Ben Iofel and Philip Fishbein
 * I pledge my honor that I have abided by the Stevens Honor System.
 */

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
#include <math.h>
#include <assert.h>
#include <locale.h>
#include "vector.h"

#define POINTER_TO_INT(x) ((int)(long)(x))
#define INT_TO_POINTER(x) ((void*)(long)(x))

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/*
 * ldisk
 * indexed linked list for disk space
 */
typedef struct ldisk {
  struct ldisk *next;
  unsigned long blockid;
  unsigned long nblocks;
  bool used;
} ldisk;

/*
 * lfile
 * indexed linked list for files
 */
typedef struct lfile {
  struct lfile *next;
  unsigned long addr;
  int bytesUsed;
} lfile;

typedef enum {
  DIR_NODE,
  FILE_NODE
} nodeType;

/*
 * node
 * node struct for files and dirs
 */
typedef struct node {
  nodeType type;
  char *name;
  struct node *parent;
  struct tm time;
  union {
    struct {
      vector children;
    }; // dir
    struct {
      unsigned long size;
      lfile *blocks;
    }; // file
  };
} node;

struct tm now() {
  time_t x = time(NULL);
  struct tm *t = localtime(&x);
  return *t;
}

/*
 * newDirNode
 * name: name of directory to create
 * parent: node pointing to parent of new node
 * returns new node
 */
node *newDirNode(char *name, node *parent) {
  assert(parent == NULL || parent->type == DIR_NODE);
  node *new = malloc(sizeof(node));
  new->type = DIR_NODE;
  new->name = name;
  new->parent = parent;
  new->time = now();
  vectorInit(&new->children);
  return new;
}

/*
 * freeLdisk
 * ldisk: disk node to free
 * frees ldisk node and re-establishes linked list
 */
void freeLdisk(ldisk *node) {
  while (node != NULL) {
    ldisk *next = node->next;
    free(node);
    node = next;
  }
}

/*
 * freeLfile
 * lfile: file node to free
 * frees lfile node and re-establishes linked list
 */
void freeLfile(lfile *node) {
  while (node != NULL) {
    lfile *next = node->next;
    free(node);
    node = next;
  }
}

/*
 * freeFSTree
 * root: root node of file system tree
 * frees tree starting at given root
 */
void freeFSTree(node *root) {
  if (root->type == DIR_NODE) {
    for (int i = 0; i < vectorLen(&root->children); ++i)
      freeFSTree(root->children.items[i]);
    vectorFree(&root->children);
  } else {
    freeLfile(root->blocks);
  }

  free(root->name);
  free(root);
}

/*
 * insertFileNode
 * root: root node of file system tree
 * path: path to where file belongs in the structure
 * file: file node
 */
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

    assert(false && "Couldn't find directory for file. Were your file_list.txt and dir_list.txt generated together?");
  } else { // no slash, so insert file node
    file->parent = root;
    vectorAdd(&root->children, file);
  }
}

/*
 * makeLfiles
 * from: starting LFile block
 * blockSize: blocksize for given file
 * last: last file in current lfile
 * returns created LFile
 */
lfile *makeLfiles(int from, int n, int blockSize, lfile **last) {
  if (n == 0) {
    return NULL;
  }

  lfile *first = malloc(sizeof(lfile));
  first->addr = from * blockSize;
  first->bytesUsed = blockSize;
  first->next = NULL;

  lfile *prev = first;
  for (int i = 1; i < n; ++i){
    // one lfile node per block used
    lfile *fileBlock = malloc(sizeof(lfile));
    fileBlock->addr = (from + i) * blockSize;
    fileBlock->bytesUsed = blockSize;
    fileBlock->next = NULL;
    prev->next = fileBlock;
    prev = fileBlock;
  }

  if (last)
    *last = prev;

  return first;
}

void *ALLOC_ERROR = (void*)0xdeadbeef;
ldisk *freeBlock(ldisk *disk, unsigned block);

/*
 * allocBlocks
 * disk: pointer to ldisk
 * size: size of file
 * blockSize: size per block
 * returns lfile after proper allocation
 */
lfile *allocBlocks(ldisk *disk, unsigned long size, int blockSize) {
  if (size == 0)
    return NULL;

  // skip used blocks
  while (disk && disk->used)
    disk = disk->next;

  if (!disk) {
    printf("Out of space\n"); // TODO handle errors
    return ALLOC_ERROR;
  }

  unsigned long blocksNeeded = size / blockSize;
  if (size % blockSize != 0)
    blocksNeeded++;

  if (blocksNeeded >= disk->nblocks) { // if we can use all the blocks in this node
    disk->used = true;

    lfile *last;
    lfile *list = makeLfiles(disk->blockid, disk->nblocks, blockSize, &last);

    last->next = allocBlocks(disk->next, size - disk->nblocks * blockSize, blockSize);
    if (last->next == ALLOC_ERROR) {
      for (int i = disk->nblocks - 1; i >= 0; --i) {
        freeBlock(disk, disk->blockid + i);
      }
      lfile *freeme = list;
      while (freeme != ALLOC_ERROR) { // go through list freeing blocks
        lfile *next = freeme->next;
        free(freeme);
        freeme = next;
      }
      return ALLOC_ERROR;
    }

    return list;
  } else { // only use a part of this node
    int usedNodeSize = blocksNeeded;
    int freeNodeSize = disk->nblocks - blocksNeeded;

    // don't create ldisk nodes of size 0
    assert(usedNodeSize != 0);
    assert(freeNodeSize != 0);

    // split nodes
    ldisk *next = disk->next;
    ldisk *freeBlocks = malloc(sizeof(ldisk));

    // set up links
    disk->next = freeBlocks;
    freeBlocks->next = next;

    freeBlocks->blockid = disk->blockid + usedNodeSize;
    freeBlocks->nblocks = freeNodeSize;
    disk->nblocks = usedNodeSize;

    disk->used = true;
    freeBlocks->used = false;

    if (size % blockSize != 0) { // use part of a block
      lfile *last = NULL;
      lfile *list = makeLfiles(disk->blockid, usedNodeSize - 1, blockSize, &last);
      lfile *partial = malloc(sizeof(lfile));

      partial->addr = (disk->blockid + disk->nblocks - 1) * blockSize;
      assert(!last || last->addr != partial->addr);
      partial->bytesUsed = size % blockSize;
      partial->next = NULL;

      // when we use less than 1 block, list & last are null
      if (last)
        last->next = partial;
      return list ? list : partial;
    } else { // use all of `used' node
      lfile *list = makeLfiles(disk->blockid, usedNodeSize, blockSize, NULL);
      return list;
    }
  }
}

/*
 * ldiskMerge
 * disk: pointer to ldisk
 */
void ldiskMerge(ldisk *disk) {
  while (disk->next != NULL) {
    if (disk->used == disk->next->used) { // merge
      ldisk *newnext = disk->next->next;
      disk->nblocks += disk->next->nblocks;
      free(disk->next);
      disk->next = newnext;
      // don't go to next in this branch in case we need to merge again
    } else { // next
      disk = disk->next;
    }
  }
}

/*
 * parseFileList
 * file: text file with file listing
 * node: root of file system tree
 * disk: pointer to disk
 * blockSize: size of blocks
 * performs initial file system setup for individual files
 */
void parseFileList(FILE* file, node *root, ldisk *disk, int blockSize) {
  assert(root->type == DIR_NODE);
  assert(root->parent == NULL);

  int ret, day;
  unsigned long size;
  char filePath[100000];
  char month[4] = {0};
  char yearOrTime[10];
  // format: inode #blocks permissions ?? user group size month day {year or time} name
  while ((ret = fscanf(file, " %*d %*d %*s %*d %*s %*s %lu %s %d %s %[^\n]\n",
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
      date.tm_year = now().tm_year;
    }

    node *file = malloc(sizeof(node));
    file->type = FILE_NODE;
    char *lastslash = strrchr(filePath, '/');
    assert(lastslash);
    file->name = strdup(lastslash + 1);
    file->size = size;
    file->time = date;
    file->blocks = allocBlocks(disk, size, blockSize);
    ldiskMerge(disk);
    if (file->blocks == ALLOC_ERROR) {
      free(file->name);
      free(file);
      break;
    } else {
      insertFileNode(root, filePath, file);
    }
  }
}

/*
 * mkdir
 * path: path to directory
 * root: root of file system
 * creates new directory in the file system
 */
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
    node *new = newDirNode(strdup(part), root);
    vectorAdd(&root->children, new);
    mkdir(delimLoc + 1, new);

    free(part);
  } else if (strcmp(path, ".") != 0) { // no slash, so create dir node, don't create `.' nodes
    node *new = newDirNode(strdup(path), root);
    vectorAdd(&root->children, new);
  }
}

/*
 * parseDirs
 * dirs: text file of directory listings
 * returns node pointing to newly created file system
 */
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

/*
 * dirNodePath
 * d: directory pointer
 * returns path to directory as a vector of dirNode*s
 */
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

/*
 * fragmentation
 * root: pointer to root node
 * blockSize: size of blocks
 * returns fragmentation of the given filesystem tree
 */
unsigned long fragmentation(node *root, int blockSize) {
  if (root->type == FILE_NODE) {
    lfile *last = root->blocks;
    if (!last) // empty file
      return 0;
    while (last->next != NULL)
      last = last->next;
    return blockSize - last->bytesUsed;
  } else {
    unsigned long sum = 0;
    for (int i = 0; i < vectorLen(&root->children); ++i) {
      sum += fragmentation(root->children.items[i], blockSize);
    }
    return sum;
  }
}

/*
 * printBlocks
 * disk: disk pointer
 * prints block usage in given disk
 */
void printBlocks(ldisk *disk) {
  while (disk != NULL) {
    printf("%s: %lu-%lu\n",
        disk->used ? "In use" : "Free",
        disk->blockid,
        disk->blockid + disk->nblocks - 1);

    disk = disk->next;
  }
}

/*
 * printDirPath
 * dir: directory pointer
 * prints path to given directory node
 */
void printDirPath(node *dir) {
  assert(dir->type == DIR_NODE);

  vector path = dirNodePath(dir);
  // delete root dir, it causes an extra /
  vectorDelete(&path, 0);
  while (vectorLen(&path) != 0) {
    printf("/%s", ((node*) path.items[0])->name);
    vectorDelete(&path, 0);
  }
  vectorFree(&path);
  printf("/");
}

void prdiskCmd(ldisk *disk, node *root, int blockSize) {
  printBlocks(disk);
  printf("fragmentation: %lu bytes\n", fragmentation(root, blockSize));
}

void prfilesShow(node *file, unsigned long blockSize) {
  assert(file->type == FILE_NODE);
  printDirPath(file->parent);
  printf("%s:\n", file->name);
  printf("\tsize = %lu\n", file->size);

  char timestr[100] = {0};
  strftime(timestr, sizeof(timestr)-1, "%c", &file->time);
  printf("\ttime = %s\n", timestr);

  printf("\tblocks = ");
  if (file->blocks) {
    vector blocks;
    vectorInit(&blocks);
    for (lfile *block = file->blocks;
         block != NULL;
         block = block->next) {
      vectorAdd(&blocks, INT_TO_POINTER(block->addr / blockSize));
    }

    if (vectorLen(&blocks) == 0) {
      printf("none");
    } else {
      int from = POINTER_TO_INT(blocks.items[0]);
      int to = from;
      for (int i = 1; i < vectorLen(&blocks); ++i) {
        int next = POINTER_TO_INT(blocks.items[i]);
        if (next - to == 1) { // increase range
          to = next;
        } else {
          printf("%d-%d,", from, to);
          from = to = next;
        }
      }
      if (from != to)
        printf("%d-%d", from, to);
      else
        printf("%d", from);
    }
    vectorFree(&blocks);
  } else {
    printf("none");
  }
  printf("\n");
}

/*
 * prfilesCmd
 * root: pointer to file tree root
 * blockSize: size of blocks
 * prints all files in given structure and their relevant information
 */
void prfilesCmd(node *root, unsigned long blockSize) {
  for (int i = 0; i < vectorLen(&root->children); ++i) {
    node *child = root->children.items[i];
    if (child->type == DIR_NODE) {
      prfilesCmd(child, blockSize);
    } else {
      prfilesShow(child, blockSize);
    }
  }
}

/*
 * dirCmd
 * root: pointer to file tree root
 * prints directorys breadth first
 */
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

/*
 * printUsage
 * prints usage if improper args given
 */
void printUsage(char *argv0) {
  fprintf(stderr, "Usage: %s -f file_list.txt -d dir_list.txt -s <disk size> -b <block size>\n", argv0);
}

void lsCmd(node *dir) {
  assert(dir->type == DIR_NODE);
  for (int i = 0; i < vectorLen(&dir->children); ++i) {
    node* child = dir->children.items[i];
    printf("%s\n", child->name);
  }
}

/**
 * path - the path to follow
 * root - the current directory
 * original - the original current directory to return in case of error
 */
node* findNodeFromPath(char* path, node *root, node *original) {
  char *delimLoc = strchr(path, '/');
  if (!delimLoc) {
    //look in current dir!!
    for (int i = 0; i < vectorLen(&root->children); ++i) {
      node* child = root->children.items[i];
      if(strcmp(path, child->name) == 0) {
        return child;
      }
    }
    printf("No such file or directory `%s'\n", path);
    return original;
  }
  int partLen = delimLoc - path;
  char *part = strndup(path, partLen);

  for (int i = 0; i < vectorLen(&root->children); ++i) {
    node *child = root->children.items[i];
    if (child->type == DIR_NODE && strcmp(child->name, part) == 0) {
      free(part);
      return findNodeFromPath(delimLoc + 1, child, original);
    }
  }
  printf("No such file or directory `%s'\n", part);
  free(part);
  return original;
}

node* cdCmd(char* path, node *curdir) {
  node *it = findNodeFromPath(path, curdir, curdir);
  if (it->type == FILE_NODE) {
    printf("`%s' is a file, cannot cd into it\n", it->name);
    return curdir;
  }
  return it;
}

/*
 * createCmd
 * path: path to file to be created
 * curdir: parent pointer
 * creates file in the given directory
 */
void createCmd(char *path, node *curdir) {
  node *file = malloc(sizeof(node));
  file->type = FILE_NODE;
  file->size = 0;
  file->name = strdup(path);
  file->time = now();
  file->blocks = NULL;
  insertFileNode(curdir, path, file);
}

/*
 * freeBlock
 * disk: ldisk pointer
 * block: block ID
 * frees relevant block
 */
ldisk *freeBlock(ldisk *disk, unsigned block) {
  // find block
  while (!(disk->blockid <= block && block < disk->blockid + disk->nblocks)) {
    disk = disk->next;
    assert(disk);
  }

  assert(disk->used);

  ldisk *oldnext = disk->next;
  unsigned long oldnblocks = disk->nblocks;

  // split into [used]->[deleted/free]->[used]

  ldisk *before = disk;
  ldisk *freeNode = malloc(sizeof(ldisk));
  ldisk *after = malloc(sizeof(ldisk));

  before->next = freeNode;
  freeNode->next = after;
  after->next = oldnext;

  before->used = true;
  freeNode->used = false;
  after->used = true;

  // before->blockid unchanged
  freeNode->blockid = block;
  after->blockid = block + 1;

  before->nblocks = block - disk->blockid;
  freeNode->nblocks = 1;
  after->nblocks = oldnblocks - before->nblocks - freeNode->nblocks;

  if (before->nblocks == 0) {
    // get rid of before block without having to fix prev block
    *before = *freeNode;
    free(freeNode);
    freeNode = NULL;
  }

  if (after->nblocks == 0) {
    assert(!after->next || after->blockid == after->next->blockid);
    if (freeNode)
      freeNode->next = after->next;
    else
      before->next = after->next;
    free(after);
  }

  return before;
}

/*
 * deleteCmd
 * file: file to delete
 * disk: ldisk pointer
 * blockSize: size of blocks
 * deletes given file
 */
void deleteCmd(node *file, ldisk *disk, int blockSize) {
  if (!file) {
    return;
  }

  if (file->type == DIR_NODE) {
    if (vectorLen(&file->children) != 0) {
      printf("Error: directory is not empty\n");
      return;
    }
  } else {
    // delete lfile blocks
    ldisk *last = disk;
    for (lfile *block = file->blocks;
         block != NULL;
         block = block->next) {
      assert(block->addr % blockSize == 0);
      last = freeBlock(last, block->addr / blockSize);
    }
    ldiskMerge(disk);
  }

  // delete file/dir from fs tree
  node *parent = file->parent;
  assert(parent->type == DIR_NODE);
  int childIndex = -1; // index of file in parent
  for (int i = 0; i < vectorLen(&parent->children); ++i) {
    if (parent->children.items[i] == file) {
      childIndex = i;
      break;
    }
  }
  assert(childIndex != -1);
  vectorDelete(&parent->children, childIndex);

  if (file->type == FILE_NODE) // TODO: do we update the parent timestamp for deleted dirs?
    parent->time = now();

  if (file->type == DIR_NODE)
    vectorFree(&file->children);

  free(file->name);
  free(file);
}

void appendCmd(node *curdir, char *filename, int bytes, ldisk *disk, unsigned long blockSize) {
  assert(curdir->type == DIR_NODE);
  node *file = findNodeFromPath(filename, curdir, NULL);
  if (!file || file->type == DIR_NODE) {
    printf("%s: no such file\n", filename);
    return;
  }

  lfile *last = file->blocks;
  if (!last) { // append to empty file
    file->blocks = allocBlocks(disk, bytes, blockSize);
    ldiskMerge(disk);
    if (file->blocks == ALLOC_ERROR) {
      file->blocks = NULL;
      return;
    }
  } else {
    while (last->next != NULL)
      last = last->next;

    int roomInLastBlock = blockSize - last->bytesUsed;
    int fillLastBlock = MIN(roomInLastBlock, bytes);
    last->next = allocBlocks(disk, bytes - fillLastBlock, blockSize);
    ldiskMerge(disk);
    if (last->next == ALLOC_ERROR) {
      last->next = NULL;
      return;
    }
    last->bytesUsed += fillLastBlock;
  }

  file->size += bytes;
  file->time = now();
}

void shrinkFile(ldisk *disk, node *file, int bytes, unsigned long blockSize) {
  if (bytes == 0) {
    return;
  }

  lfile *prev = NULL,
        *last = file->blocks;
  while (last->next) {
    prev = last;
    last = last->next;
  }
  if (last->bytesUsed > bytes) {
    last->bytesUsed -= bytes;
  } else {
    int newsize = bytes - last->bytesUsed;
    assert(last->addr % blockSize == 0);
    freeBlock(disk, last->addr / blockSize);
    ldiskMerge(disk);
    free(last);
    if (prev)
      prev->next = NULL;
    shrinkFile(disk, file, newsize, blockSize);
  }
}

void removeCmd(node *curdir, char *filename, int bytes, ldisk *disk, unsigned long blockSize) {
  assert(curdir->type == DIR_NODE);
  node *file = findNodeFromPath(filename, curdir, NULL);
  if (!file || file->type == DIR_NODE) {
    printf("%s: no such file\n", filename);
    return;
  }

  lfile *blocks = file->blocks;
  if (!blocks) {
    printf("%s is an empty file\n", filename);
    return;
  }

  if (file->size < (unsigned)bytes) {
    printf("you cannot remove %d bytes from a %s (size = %lu)\n",
        bytes,
        filename,
        file->size);
    return;
  }

  shrinkFile(disk, file, bytes, blockSize);
  file->size -= bytes;
  if (file->size == 0)
    file->blocks = NULL;
  file->time = now();
}

int main(int argc, char *argv[]) {
  setlocale(LC_ALL, ""); // to print times correctly

  FILE *fileList = NULL, *dirList = NULL;
  unsigned long diskSize = 0;
  unsigned blockSize = 0;

  int opt;
  while ((opt = getopt(argc, argv, "f:d:s:b:")) != -1) {
    switch (opt) {
      case 'f':
        fileList = fopen(optarg, "r");
        if (!fileList) {
          perror(optarg);
          return 1;
        }
        break;
      case 'd':
        dirList = fopen(optarg, "r");
        if (!dirList) {
          perror(optarg);
          return 1;
        }
        break;
      case 's':
        diskSize = strtoul(optarg, NULL, 0);
        break;
      case 'b':
        blockSize = strtoul(optarg, NULL, 0);
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

  // initial Ldisk
  ldisk disk = {
    .next = NULL,
    .blockid = 0,
    .nblocks = diskSize / blockSize,
    .used = false
  };

  // create dirs in tree
  node *root = parseDirs(dirList);
  // create files in tree
  parseFileList(fileList, root, &disk, blockSize);

  // input loop
  node *currentDir = root;
  char *command = NULL;
  size_t len = 0;
  ssize_t nread;

  printDirPath(currentDir);
  printf(" > ");

  while ((nread = getline(&command, &len, stdin)) != -1) {
    if (command[nread-1] == '\n')
      command[nread-1] = '\0';

    if (strcmp(command, "exit") == 0) {
      printf("bye bye\n");
      break;
    } else if (strcmp(command, "ls") == 0) {
      lsCmd(currentDir);
    } else if (strncmp(command, "cd ", 3) == 0) {
      currentDir = cdCmd(command + 3, currentDir);
    } else if (strcmp(command, "cd..") == 0) {
      if (currentDir->parent)
        currentDir = currentDir->parent;
    } else if (strcmp(command, "dir") == 0) {
      dirCmd(currentDir);
    } else if (strncmp(command, "mkdir ", 6) == 0) {
      mkdir(command + 6, currentDir);
    } else if (strncmp(command, "create ", 7) == 0) {
      createCmd(command + 7, currentDir);
    } else if (strncmp(command, "delete ", 7) == 0) {
      deleteCmd(findNodeFromPath(command + 7, currentDir, NULL), &disk, blockSize);
    } else if (strncmp(command, "append ", 7) == 0) {
      char *args = command + 7;
      char *space = strchr(args, ' ');
      if (!space) {
        printf("Usage: append <filename> <bytes>\n");
      } else {
        *space = '\0';
        appendCmd(currentDir, args, atoi(space + 1), &disk, blockSize);
      }
    } else if (strncmp(command, "remove ", 7) == 0) {
      char *args = command + 7;
      char *space = strchr(args, ' ');
      if (!space) {
        printf("Usage: remove <filename> <bytes>\n");
      } else {
        *space = '\0';
        removeCmd(currentDir, args, atoi(space + 1), &disk, blockSize);
      }
    } else if (strcmp(command, "prdisk") == 0) {
      prdiskCmd(&disk, root, blockSize);
    } else if (strcmp(command, "prfiles") == 0) {
      prfilesCmd(root, blockSize);
    } else {
      printf("Unknown command `%s'\n", command);
    }

    // we may have changed currentDir above
    assert(currentDir->type == DIR_NODE);

    printDirPath(currentDir);
    printf(" > ");
  }
  free(command);

  freeFSTree(root);
  freeLdisk(disk.next);

  fclose(fileList);
  fclose(dirList);

}
