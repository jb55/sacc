/* See LICENSE file for copyright and license details. */
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
	Item *entry;
	Dir  *dir;
};

struct dir {
	Item **items;
	size_t nitems;
};

static void die(const char *, ...);

void
die(const char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	vfprintf(stderr, fmt, arg);
	va_end(arg);
	fputc('\n', stderr);

	exit(1);
}

void *
xreallocarray(void *m, const size_t n, const size_t s)
{
	void *nm;

	if (s && n > (size_t)-1/s)
		die("realloc: overflow");
	if (!(nm = realloc(m, n * s)))
		die("realloc: %s", strerror(errno));

	return nm;
}

void *
xmalloc(const size_t n)
{
	void *m = malloc(n);

	if (!m)
		die("malloc: %s\n", strerror(errno));

	return m;
}

char *
xstrdup(const char *str)
{
	char *s;

	if (!(s = strdup(str)))
		die("strdup: %s\n", strerror(errno));

	return s;
}

void
usage(void)
{
	die("usage: sacc URL");
}

void
help(void)
{
	puts("Commands:\n"
             "N = [1-9]...: browse item N.\n"
             "0: browse previous item.\n"
	     "!: refetch failed item.\n"
             "^D, q: quit.\n"
             "h: this help.");
}

const char *
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

void
display(Item *item)
{
	Item **items;
	size_t i;

	switch (item->type) {
	case '0':
		puts(item->raw);
		break;
	case '1':
		items = item->dir->items;
		for (i = 0; i < item->dir->nitems; ++i) {
			printf("[%d]%.4s: %s\n", i+1,
			       typedisplay(items[i]->type), items[i]->username);
		}
		break;
	}
}

char *
pickfield(char **s)
{
	char *c, *f = *s;

	/* loop until we reach the end of the string, or a tab, or CRLF */
	for (c = *s; *c; ++c) {
		switch (*c) {
		case '\t':
			break;
		case '\r': /* FALLTHROUGH */
			if (*(c+1) == '\n') {
				*c++ = '\0';
				break;
			}
		default:
			continue;
		}
		break;
	}

	if (!*c)
		die("Can't parse directory item: %s", f);
	*c = '\0';
	*s = c+1;

	return f;
}

Dir *
molddiritem(char *raw)
{
	Item *item, **items = NULL;
	char *crlf, *p;
	Dir *dir;
	size_t i, nitems;

	for (crlf = raw, nitems = 0; p = strstr(crlf, "\r\n"); ++nitems)
		crlf = p+2;
	if (--nitems < 1)
		return NULL;
	if (strcmp(crlf-3, ".\r\n"))
		return NULL;

	dir = xmalloc(sizeof(Dir));
	items = xreallocarray(items, nitems, sizeof(Item*));

	for (i = 0; i < nitems; ++i) {
		item = xmalloc(sizeof(Item));

		item->type = *raw++;
		item->username = pickfield(&raw);
		item->selector = pickfield(&raw);
		item->host = pickfield(&raw);
		item->port = pickfield(&raw);
		item->raw = NULL;
		item->entry = NULL;
		item->dir = NULL;

		items[i] = item;
	}

	dir->items = items;
	dir->nitems = nitems;

	return dir;
}

char *
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

int
sendselector(int sock, const char *selector)
{
	if (write(sock, selector, strlen(selector)) < 0)
		die("Can't send message: %s", strerror(errno));
	if (write(sock, "\r\n", strlen("\r\n")) < 0)
		die("Can't send message: %s", strerror(errno));

	return 1;
}

int
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

int
dig(Item *entry, Item *item)
{
	int sock;

	if (item->raw)     /* already in cache */
		return 1;

	if (!item->entry)
		item->entry = entry;

	if (item->type > '1') /* not supported */
		return 0;

	sock = connectto(item->host, item->port);
	sendselector(sock, item->selector);
	item->raw = getrawitem(sock);

	if (!*item->raw) {    /* empty read */
		free(item->raw);
		item->raw = NULL;
		return 0;
	}

	if (item->type == '1' &&
	    !(item->dir = molddiritem(item->raw)))
		return 0;
	return 1;
}

Item *
selectitem(Item *entry)
{
	char buf[BUFSIZ], nl;
	Item *hole;
	int item, nitems;

	nitems = entry->dir ? entry->dir->nitems : 0;

	do {
		printf("%d items (h for help): ", nitems);

		if (!fgets(buf, sizeof(buf), stdin)) {
			putchar('\n');
			return NULL;
		}
		if (!strcmp(buf, "q\n"))
			return NULL;

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

void
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

Item *
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
