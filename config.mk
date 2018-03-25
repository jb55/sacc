# Install paths
PREFIX = /usr/local
MANDIR = $(PREFIX)/share/man/man1

# UI type
# txt (textual)
#UI=txt
# ti (screen-oriented)
UI=ti
LIBS=-lcurses

# Define NEED_ASPRINTF in your cflags is your system doesn't provide asprintf()
#CFLAGS = -DNEED_ASPRINTF
