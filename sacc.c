/* See LICENSE file for copyright and license details. */
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__) || \
    defined(___FreeBSD__)
#include <sys/ioctl.h>
#else
#include <stropts.h>
#endif
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "common.h"

static char *mainurl;
static Item *mainentry;

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

static void *
xcalloc(size_t n)
{
	char *m = xmalloc(n);

	while (n)
		m[--n] = 0;

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

const char *
typedisplay(char t)
{
	switch (t) {
	case '0':
		return "Text+";
	case '1':
		return "Dir +";
	case '2':
		return "CSO |";
	case '3':
		return "Err |";
	case '4':
		return "Macf|";
	case '5':
		return "DOSf|";
	case '6':
		return "UUEf|";
	case '7':
		return "Find|";
	case '8':
		return "Tlnt|";
	case '9':
		return "Binf|";
	case '+':
		return "Mirr|";
	case 'T':
		return "IBMt|";
	case 'g':
		return "GIF |";
	case 'I':
		return "Img |";
	case 'h':
		return "HTML|";
	case 'i':
		return "    |";
	case 's':
		return "Snd |";
	default:
		return "!   |";
	}
}

static void
displaytextitem(Item *item)
{
	FILE *pagerin;
	int pid, wpid, status;

	uicleanup();
	switch (pid = fork()) {
	case -1:
		fprintf(stderr, "Couldn't fork.\n");
		return;
	case 0:
		pagerin = popen("$PAGER", "we");
		fputs(item->raw, pagerin);
		status = pclose(pagerin);
		fputs("[ Press Enter to continue ]", stdout);
		getchar();
		exit(status);
	default:
		while ((wpid = wait(NULL)) >= 0 && wpid != pid)
			;
	}
	uisetup();
}

static char *
pickfield(char **raw, char sep)
{
	char *c, *f = *raw;

	for (c = *raw; *c && *c != sep; ++c)
		;

	*c = '\0';
	*raw = c+1;

	return f;
}

static char *
invaliditem(char *raw)
{
	char c;
	int tabs;

	for (tabs = 0; (c = *raw) && c != '\n'; ++raw) {
		if (c == '\t')
			++tabs;
	}
	if (c)
		*raw++ = '\0';

	return (tabs == 3) ? NULL : raw;
}

static Item *
molditem(char **raw)
{
	Item *item;
	char *next;

	if (!*raw)
		return NULL;

	item = xcalloc(sizeof(Item));

	if ((next = invaliditem(*raw))) {
		item->username = *raw;
		*raw = next;
		return item;
	}

	item->type = *raw[0]++;
	item->username = pickfield(raw, '\t');
	item->selector = pickfield(raw, '\t');
	item->host = pickfield(raw, '\t');
	item->port = pickfield(raw, '\r');
	if (!*raw[0])
		++*raw;

	return item;
}

static Dir *
molddiritem(char *raw)
{
	Item **items = NULL;
	char *s, *nl, *p;
	Dir *dir;
	size_t i, nitems;

	for (s = nl = raw, nitems = 0; p = strchr(nl, '\n'); ++nitems) {
		s = nl;
		nl = p+1;
	}
	if (!strcmp(s, ".\r\n") || !strcmp(s, ".\n"))
		--nitems;
	if (!nitems)
		return NULL;

	dir = xmalloc(sizeof(Dir));
	items = xreallocarray(items, nitems, sizeof(Item*));

	for (i = 0; i < nitems; ++i)
		items[i] = molditem(&raw);

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
fetchitem(Item *item)
{
	int sock;

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

	return (item->raw != NULL);
}

static int
dig(Item *entry, Item *item)
{
	if (item->raw) /* already in cache */
		return item->type;

	if (!item->entry)
		item->entry = entry;

	switch (item->type) {
	case '0':
		if (!fetchitem(item))
			return 0;
		break;
	case '1':
		if (!fetchitem(item) || !(item->dir = molddiritem(item->raw))) {
			fputs("Couldn't parse dir item\n", stderr);
			return 0;
		}
		break;
	default:
		fprintf(stderr, "Type %c (%s) not supported\n",
		        item->type, typedisplay(item->type));
		return 0;
	}

	return item->type;
}

static void
delve(Item *hole)
{
	Item *entry = hole;

	while (hole) {
		switch (dig(entry, hole)) {
		case '0':
			displaytextitem(hole);
			break;
		case '1':
			if (hole->dir)
				entry = hole;
			break;
		case 0:
			fprintf(stderr, "Couldn't get %s:%s/%c%s\n", hole->host,
			                hole->port, hole->type, hole->selector);
		}

		display(entry);
		hole = selectitem(entry);
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
	entry->curline = 0;
	entry->raw = NULL;
	entry->dir = NULL;

	return entry;
}

static void
cleanup(void)
{
	free(mainentry); /* TODO free all tree recursively */
	free(mainurl);
	uicleanup();
}

static void
setup(void)
{
	setenv("PAGER", "more", 0);
	atexit(cleanup);
	uisetup();
}

int
main(int argc, char *argv[])
{
	if (argc != 2)
		usage();

	setup();

	mainurl = xstrdup(argv[1]);

	mainentry = moldentry(mainurl);
	delve(mainentry);

	exit(0);
}
