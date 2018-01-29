# sacc: saccomys gopher client
# See LICENSE file for copyright and license details.
.POSIX:

include config.mk

BIN = sacc
OBJ = $(BIN:=.o) ui_$(UI).o

all: $(BIN)

config.h:
	cp config.def.h config.h

$(BIN): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) $(LIBS) -o $@

$(OBJ): config.h config.mk common.h

clean:
	rm -f $(BIN) $(OBJ)

install: $(BIN)
	mkdir -p $(DESTDIR)$(PREFIX)/bin/
	cp -f $(BIN) $(DESTDIR)$(PREFIX)/bin/
	chmod 555 $(DESTDIR)$(PREFIX)/bin/$(BIN)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)
