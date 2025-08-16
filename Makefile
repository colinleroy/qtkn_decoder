all: qtkn_decoder

clean:
	rm -f qtkn_decoder

qtkn_decoder: *.c
	gcc ${CFLAGS} -o $@ $^
