# this is the Makefile of the jexecutor.
# for sqlite3 see https://www.sqlite.org/howtocompile.html
all: jexec

jexec: jexec.o sqlite3.o
	$(CC) -o $@ $^ -lpthread `pkg-config --cflags --libs hiredis`

jexec.o:	jexec.c
	$(CC) -c $< `pkg-config --cflags --libs hiredis`

sqlite3.o:	sqlite3.c
	$(CC) -c $< -DSQLITE_OMIT_LOAD_EXTENSION -lpthread

clean:
	rm -f sqlite3.o jexec.o jexec

