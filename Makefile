SRC := my_route_lookup.c io.c utils.c
INC := io.h utils.h
CFLAGS = -Wall -Wextra -pedantic -std=c11 -ggdb

all: my_route_lookup

my_route_lookup: $(SRC)
	gcc $(CFLAGS) $(SRC) -o my_route_lookup -lm

%.c: %.h

.PHONY: clean

clean:
	rm -f my_route_lookup

#RL Lab 2020 Switching UC3M
