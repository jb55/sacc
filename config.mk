# Install paths
PREFIX = /usr/local

# UI type
# txt (textual)
UI=txt
# ti (screen-oriented)
#UI=ti
#LIBS=-lcurses

# Stock FLAGS
SACCCFLAGS = -D_DEFAULT_SOURCE $(CFLAGS)

.c.o:
	$(CC) $(SACCCFLAGS) -c $<
