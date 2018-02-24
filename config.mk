# Install paths
PREFIX = /usr/local
MANDIR = $(PREFIX)/share/man/man1

# UI type
# txt (textual)
#UI=txt
# ti (screen-oriented)
UI=ti
LIBS=-lcurses

# Stock FLAGS
SACCCFLAGS = -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 -D_BSD_SOURCE $(CFLAGS)

.c.o:
	$(CC) $(SACCCFLAGS) -c $<
