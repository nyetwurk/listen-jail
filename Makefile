.PHONY: all clean install test

all: listen-jail.so

listen-jail.so: listen-jail.c Makefile
	gcc -Wall -fPIC -shared -o $@ $< -ldl

clean:
	@rm -f *.so

install: listen-jail.so
	@mkdir -p /usr/local/lib/listen-jail
	install $< /usr/local/lib/listen-jail/

test: listen-jail.so
	./test.sh
