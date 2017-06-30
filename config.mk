# Install paths
PREFIX = /usr/local

# UI type, txt
UI=txt

# Stock FLAGS
SACCCFLAGS = -D_POSIX_C_SOURCE=200809L $(CFLAGS)
SACCLDFLAGS = $(LDFLAGS)

.c.o:
	$(CC) $(SACCCFLAGS) -c $<
