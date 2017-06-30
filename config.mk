# Install paths
PREFIX = /usr/local

# UI type
# txt (textual)
UI=txt
# ti (screen-oriented)
#UI=ti
#UIFLAGS=-lcurses

# Stock FLAGS
SACCCFLAGS = -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE $(CFLAGS)
SACCLDFLAGS = $(UIFLAGS) $(LDFLAGS)

.c.o:
	$(CC) $(SACCCFLAGS) -c $<
