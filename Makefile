# sacc: saccomys gopher client
# See LICENSE file for copyright and license details.
.POSIX:

include config.mk

BIN = sacc

all: $(BIN)

clean:
	rm -f $(BIN)

install:
	mkdir -p $(PREFIX)/bin/
	cp $(BIN) $(PREFIX)/bin/
	chmod 555 $(PREFIX)/bin/$(BIN)

uninstall:
	rm -f $(PREFIX)/bin/$(BIN)
