#include <stdio.h>
#include <stdlib.h>

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
  return 0;
}
