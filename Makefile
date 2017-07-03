# sacc: saccomys gopher client
# See LICENSE file for copyright and license details.
.POSIX:

include config.mk

BIN = sacc
OBJ = $(BIN:=.o) ui_$(UI).o

all: $(BIN)

$(BIN): config.mk common.h $(OBJ)
	$(CC) $(OBJ) $(SACCLDFLAGS) -o $@

clean:
	rm -f $(BIN) $(OBJ)

install: $(BIN)
	mkdir -p $(PREFIX)/bin/
	cp -f $(BIN) $(PREFIX)/bin/
	chmod 555 $(PREFIX)/bin/$(BIN)

uninstall:
	rm -f $(PREFIX)/bin/$(BIN)
