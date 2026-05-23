.PHONY: all clean

CFLAGS := -Wall -Wextra -Wno-format -std=c99 -pedantic -lm -O

SRC = $(wildcard src/*.c)

all: arrow

arrow:
	$(CC) $(CFLAGS) -o arrow $(SRC)

clean:
	rm -f arrow *.o *.s
