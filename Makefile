CFLAGS = -D_POSIX_C_SOURCE=200809L -std=c99 -g -Wall -Wextra

main: main.c

clean:
	$(RM) main

.PHONY: clean
