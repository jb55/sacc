/* See LICENSE file for copyright and license details. */
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>
#include <termios.h> /* ifdef BSD */
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

typedef struct item Item;
typedef struct dir Dir;

struct item {
	char type;
	char *username;
	char *selector;
	char *host;
	char *port;
	char *raw;
	size_t printoff;
	Item *entry;
	Dir  *dir;
};

struct dir {
	Item **items;
	size_t nitems;
};

static void
die(const char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	vfprintf(stderr, fmt, arg);
	va_end(arg);
	fputc('\n', stderr);

	exit(1);
}

static void *
xreallocarray(void *m, const size_t n, const size_t s)
{
	void *nm;

	if (n == 0 || s == 0) {
		free(m);
		return NULL;
	}
	if (s && n > (size_t)-1/s)
		die("realloc: overflow");
	if (!(nm = realloc(m, n * s)))
		die("realloc: %s", strerror(errno));

	return nm;
}

static void *
xmalloc(const size_t n)
{
	void *m = malloc(n);

	if (!m)
		die("malloc: %s\n", strerror(errno));

	return m;
}

static char *
xstrdup(const char *str)
{
	char *s;

	if (!(s = strdup(str)))
		die("strdup: %s\n", strerror(errno));

	return s;
}

static void
usage(void)
{
	die("usage: sacc URL");
}

static void
help(void)
{
	puts("Commands:\n"
             "N = [1-9]...: browse item N.\n"
             "0: browse previous item.\n"
             "n: show next page.\n"
             "p: show previous page.\n"
	     "!: refetch failed item.\n"
             "^D, q: quit.\n"
             "h: this help.");
}

static int
termlines(void)
{
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0) {
		die("Could not get terminal resolution: %s",
		    strerror(errno));
	}

	return ws.ws_row-1;
}

static const char *
typedisplay(char t)
{
	switch (t) {
	case '0':
		return "Text";
	case '1':
		return "Dir";
	case '2':
		return "CSO";
	case '3':
		return "Err";
	case '4':
		return "Macf";
	case '5':
		return "DOSf";
	case '6':
		return "UUEf";
	case '7':
		return "Find";
	case '8':
		return "Tlnt";
	case '9':
		return "Binf";
	case '+':
		return "Mirr";
	case 'T':
		return "IBMt";
	case 'g':
		return "GIF";
	case 'I':
		return "Img";
	case 'h':
		return "HTML";
	case 'i':
		return "Info";
	case 's':
		return "Snd";
	default:
		return "WRNG";
	}
}

static void
display(Item *item)
{
	Item **items;
	size_t i, lines, nitems;
	int ndigits;

	if (item->type > '1')
		return;

	switch (item->type) {
	case '0':
		puts(item->raw);
		break;
	case '1':
		items = item->dir->items;
		nitems = item->dir->nitems;
		lines = item->printoff + termlines();
		ndigits = (nitems < 10) ? 1 : (nitems < 100) ? 2 : 3;

		for (i = item->printoff; i < nitems && i < lines; ++i) {
			if (item = items[i]) {
				printf("%*d %-4s%c %s\n", ndigits, i+1,
				       item->type != 'i' ?
				       typedisplay(item->type) : "",
				       item->type > '1' ? '|' : '+',
				       items[i]->username);
			} else {
				printf("%*d  !! |\n", ndigits, i+1);
			}
		}
		break;
	}
}

static char *
pickfield(char **raw, char sep)
{
	char *c, *f = *raw;

	for (c = *raw; *c != sep; ++c) {
		switch (*c) {
		case '\t':
			if (sep == '\r')
				*c = '\0';
		case '\n':
		case '\r':
		case '\0':
			return NULL;
		default:
			continue;
		}
		break;
	}

	*c = '\0';
	*raw = c+1;

	return f;
}

static Item *
molditem(char **raw)
{
	Item *item;
	char type, *username, *selector, *host, *port;

	if (!*raw)
		return NULL;

	if (!(type = *raw[0]++) ||
	    !(username = pickfield(raw, '\t')) ||
	    !(selector = pickfield(raw, '\t')) ||
	    !(host = pickfield(raw, '\t')) ||
	    !(port = pickfield(raw, '\r')) ||
	    *raw[0]++ != '\n')
		return NULL;

	item = xmalloc(sizeof(Item));

	item->type = type;
	item->username = username;
	item->selector = selector;
	item->host = host;
	item->port = port;
	item->raw = NULL;
	item->dir = NULL;
	item->entry = NULL;
	item->printoff = 0;

	return item;
}

static Dir *
molddiritem(char *raw)
{
	Item *item, **items = NULL;
	char *crlf, *p;
	Dir *dir;
	size_t i, nitems;

	for (crlf = raw, nitems = 0; p = strstr(crlf, "\r\n"); ++nitems)
		crlf = p+2;
	if (nitems <= 1)
		return NULL;
	if (!strcmp(crlf-3, ".\r\n"))
		--nitems;
	else
		fprintf(stderr, "Parsing error: missing .\\r\\n last line\n");

	dir = xmalloc(sizeof(Dir));
	items = xreallocarray(items, nitems, sizeof(Item*));

	for (i = 0; i < nitems; ++i) {
		if (item = molditem(&raw)) {
			items[i] = item;
		} else {
			fprintf(stderr, "Parsing error: dir entity: %d\n", i+1);
			items = xreallocarray(items, i, sizeof(Item*));
			nitems = i;
			break;
		}
	}

	if (!items)
		return NULL;

	dir->items = items;
	dir->nitems = nitems;

	return dir;
}

static char *
getrawitem(int sock)
{
	char *raw, *buf;
	size_t bn, bs;
	ssize_t n;

	raw = buf = NULL;
	bn = bs = n = 0;

	do {
		bs -= n;
		buf += n;
		if (bs <= 1) {
			raw = xreallocarray(raw, ++bn, BUFSIZ);
			buf = raw + (bn-1) * BUFSIZ;
			bs = BUFSIZ;
		}
	} while ((n = read(sock, buf, bs)) > 0);

	if (n < 0)
		die("Can't read socket: %s", strerror(errno));

	*buf = '\0';

	return raw;
}

static void
sendselector(int sock, const char *selector)
{
	char *msg, *p;
	size_t ln;
	ssize_t n;

	ln = strlen(selector) + 3;
	msg = p = xmalloc(ln);
	snprintf(msg, ln--, "%s\r\n", selector);

	while ((n = write(sock, p, ln)) != -1 && n != 0) {
		ln -= n;
		p += n;
	}
	free(msg);
	if (n == -1)
		die("Can't send message: %s", strerror(errno));
}

static int
connectto(const char *host, const char *port)
{
	static const struct addrinfo hints = {
	    .ai_family = AF_UNSPEC,
	    .ai_socktype = SOCK_STREAM,
	    .ai_protocol = IPPROTO_TCP,
	};
	struct addrinfo *addrs, *addr;
	int sock, r;

	if (r = getaddrinfo(host, port, &hints, &addrs))
		die("Can't resolve hostname “%s”: %s", host, gai_strerror(r));

	for (addr = addrs; addr; addr = addr->ai_next) {
		if ((sock = socket(addr->ai_family, addr->ai_socktype,
		                   addr->ai_protocol)) < 0)
			continue;
		if ((r = connect(sock, addr->ai_addr, addr->ai_addrlen)) < 0) {
			close(sock);
			continue;
		}
		break;
	}
	if (sock < 0)
		die("Can't open socket: %s", strerror(errno));
	if (r < 0)
		die("Can't connect to: %s:%s: %s", host, port, strerror(errno));

	freeaddrinfo(addrs);

	return sock;
}

static int
dig(Item *entry, Item *item)
{
	int sock;

	if (item->raw) /* already in cache */
		return 1;

	if (!item->entry)
		item->entry = entry;

	if (item->type > '1') {
		fprintf(stderr, "Type %c not supported\n", item->type);
		return 0;
	}

	sock = connectto(item->host, item->port);
	sendselector(sock, item->selector);
	item->raw = getrawitem(sock);
	close(sock);

	if (!*item->raw) {
		fputs("Empty response from server\n", stderr);
		free(item->raw);
		item->raw = NULL;
		return 0;
	}

	if (item->type == '1') {
		if (!(item->dir = molddiritem(item->raw))) {
			fputs("Couldn't parse dir item\n", stderr);
			return 0;
		}
	}
	return 1;
}

static Item *
selectitem(Item *entry)
{
	char buf[BUFSIZ], nl;
	Item *hole;
	int item, nitems, lines;

	nitems = entry->dir ? entry->dir->nitems : 0;

	do {
		printf("%d items (h for help): ", nitems);

		if (!fgets(buf, sizeof(buf), stdin)) {
			putchar('\n');
			return NULL;
		}
		if (!strcmp(buf, "q\n"))
			return NULL;

		if (!strcmp(buf, "n\n")) {
			lines = termlines();
			if (lines < entry->dir->nitems - entry->printoff &&
			    lines < (size_t)-1 - entry->printoff)
				entry->printoff += lines;
			return entry;
		}
		if (!strcmp(buf, "p\n")) {
			lines = termlines();
			if (lines <= entry->printoff)
				entry->printoff -= lines;
			else
				entry->printoff = 0;
			return entry;
		}

		if (!strcmp(buf, "!\n")) {
			if (entry->raw)
				continue;
			return entry;
		}

		item = -1;
		if (!strcmp(buf, "h\n")) {
			help();
			continue;
		}
		if (*buf < '0' || *buf > '9')
			continue;

		nl = '\0';
		if (sscanf(buf, "%d%c", &item, &nl) != 2 || nl != '\n')
			item = -1;
	} while (item < 0 || item > nitems);

	if (item > 0)
		return entry->dir->items[item-1];

	return entry->entry;
}

static void
delve(Item *hole)
{
	Item *entry = hole;

	while (hole) {
		if (dig(entry, hole)) {
			display(hole);
		} else {
			fprintf(stderr, "Couldn't get %s:%s/%c%s\n", hole->host,
			                hole->port, hole->type, hole->selector);
		}
		entry = hole;
		hole = selectitem(hole);
	}
}

static Item *
moldentry(char *url)
{
	Item *entry;
	char *p, *host = url, *port = "gopher", *gopherpath = "1";
	int parsed, ipv6;

	if (p = strstr(url, "://")) {
		if (strncmp(url, "gopher", p - url))
			die("Protocol not supported: %.*s", p - url, url);
		host = p + 3;
	}

	if (*host == '[') {
		ipv6 = 1;
		++host;
	} else {
		ipv6 = 0;
	}

	for (parsed = 0, p = host; !parsed && *p; ++p) {
		switch (*p) {
		case ']':
			if (ipv6) {
				*p = '\0';
				ipv6 = 0;
			}
			continue;
		case ':':
			if (!ipv6) {
				*p = '\0';
				port = p+1;
			}
			continue;
		case '/':
			*p = '\0';
			gopherpath = p+1;
			parsed = 1;
			continue;
		}
	}

	if (*host == '\0' || *port == '\0' || ipv6)
		die("Can't parse url");

	if (gopherpath[0] > '1')
		die("Gopher type not supported: %s",
		    typedisplay(gopherpath[0]));

	entry = xmalloc(sizeof(Item));
	entry->type = gopherpath[0];
	entry->username = entry->selector = ++gopherpath;
	entry->host = host;
	entry->port = port;
	entry->entry = entry;
	entry->printoff = 0;
	entry->raw = NULL;
	entry->dir = NULL;

	return entry;
}

int
main(int argc, char *argv[])
{
	Item *entry;
	char *url;

	if (argc != 2)
		usage();

	url = xstrdup(argv[1]);

	entry = moldentry(url);
	delve(entry);

	free(entry); /* TODO free all tree recursively */
	free(url);

	return 0;
}
