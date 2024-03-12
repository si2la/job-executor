all: rqueue

rqueue:	rqueue.c
	$(CC) -o $@ $< `pkg-config --cflags --libs hiredis`
