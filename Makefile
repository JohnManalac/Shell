CFLAGS =-Wall -pedantic -g

mysh: mysh.o 
	gcc -o $@ $^

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $^

.PHONY: clean
clean:
	rm -f mysh mysh.o 