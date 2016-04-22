CFLAGS=-Wall -Werror -pthread -g

all: search

search: search.c
	gcc $(CFLAGS) search.c -o search

clean:
	rm search