SRC := my_route_lookup.c io.c utils.c node.c
INC := io.h utils.h node.h
CFLAGS = -Wall -Wextra -pedantic -std=c11 -O2

all: my_route_lookup

my_route_lookup: $(SRC)
	gcc $(CFLAGS) $(SRC) -o my_route_lookup -lm

%.c: %.h

.PHONY: clean

clean:
	rm -f my_route_lookup

#RL Lab 2020 Switching UC3M
