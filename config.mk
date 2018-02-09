# Install paths
PREFIX = /usr/local

# UI type
# txt (textual)
UI=txt
# ti (screen-oriented)
#UI=ti
#LIBS=-lcurses

# Stock FLAGS
SACCCFLAGS = -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 -D_BSD_SOURCE $(CFLAGS)

.c.o:
	$(CC) $(SACCCFLAGS) -c $<
