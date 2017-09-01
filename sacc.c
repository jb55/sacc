/* See LICENSE file for copyright and license details. */
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__) || \
    defined(__FreeBSD__)
#include <sys/ioctl.h>
#else
#include <stropts.h>
#endif
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "common.h"

#include "config.h"

static char *mainurl;
static Item *mainentry;
static int devnullfd;
static int parent = 1;

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

static void
clearitem(Item *item)
{
	Dir *dir;
	Item **items;
	char *tag;
	size_t i;

	if (!item)
		return;

	if (dir = item->dat) {
		items = dir->items;
		for (i = 0; i < dir->nitems; ++i) {
			clearitem(items[i]);
			free(items[i]);
		}
		free(items);
		clear(&item->dat);
	}

	if (parent && (tag = item->tag) &&
	    !strncmp(tag, tmpdir, strlen(tmpdir)))
		unlink(tag);

	clear(&item->tag);
	clear(&item->raw);
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
		return "Macf+";
	case '5':
		return "DOSf+";
	case '6':
		return "UUEf+";
	case '7':
		return "Find+";
	case '8':
		return "Tlnt|";
	case '9':
		return "Binf+";
	case '+':
		return "Mirr+";
	case 'T':
		return "IBMt|";
	case 'g':
		return "GIF +";
	case 'I':
		return "Img +";
	case 'h':
		return "HTML+";
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
	int pid, wpid;

	uicleanup();
	switch (pid = fork()) {
	case -1:
		uistatus("Couldn't fork.");
		return;
	case 0:
		parent = 0;
		pagerin = popen("$PAGER", "we");
		fputs(item->raw, pagerin);
		exit(pclose(pagerin));
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
	if (!nitems) {
		uistatus("Couldn't parse dir item");
		return NULL;
	}

	dir = xmalloc(sizeof(Dir));
	items = xreallocarray(items, nitems, sizeof(Item*));

	for (i = 0; i < nitems; ++i)
		items[i] = molditem(&raw);

	dir->items = items;
	dir->nitems = nitems;
	dir->printoff = dir->curline = 0;

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

	*buf = '\0';

	if (n < 0) {
		uistatus("Can't read socket: %s", strerror(errno));
		clear(&raw);
	}

	return raw;
}

static int
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
		uistatus("Can't send message: %s", strerror(errno));

	return n;
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

	if (r = getaddrinfo(host, port, &hints, &addrs)) {
		uistatus("Can't resolve hostname “%s”: %s", host, gai_strerror(r));
		return -1;
	}

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
	if (sock < 0) {
		uistatus("Can't open socket: %s", strerror(errno));
		return -1;
	}
	if (r < 0) {
		uistatus("Can't connect to: %s:%s: %s", host, port, strerror(errno));
		return -1;
	}

	freeaddrinfo(addrs);

	return sock;
}

static int
download(Item *item, int dest)
{
	char buf[BUFSIZ];
	ssize_t r, w;
	int src;

	if (!item->tag) {
		if ((src = connectto(item->host, item->port)) < 0 ||
		    sendselector(src, item->selector) < 0)
			return 0;
	} else if ((src = open(item->tag, O_RDONLY)) < 0) {
		printf("Can't open source file %s: %s",
		       item->tag, strerror(errno));
		errno = 0;
		return 0;
	}

	while ((r = read(src, buf, BUFSIZ)) > 0) {
		while ((w = write(dest, buf, r)) > 0)
			r -= w;
	}

	if (r < 0 || w < 0) {
		printf("Error downloading file %s: %s",
		       item->selector, strerror(errno));
		errno = 0;
	}

	close(src);

	return (r == 0 && w == 0);
}

static void
downloaditem(Item *item)
{
	char *file, *path, *tag;
	mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP;
	int dest;

	if (file = strrchr(item->selector, '/'))
		++file;
	else
		file = item->selector;

	if (!(path = uiprompt("Download to [%s] (^D cancel): ", file)))
		return;

	if (!path[0])
		path = xstrdup(file);

	if (tag = item->tag) {
		if (access(tag, R_OK) < 0) {
			clear(&item->tag);
		} else if (!strcmp(tag, path)) {
			goto cleanup;
		}
	}

	if ((dest = open(path, O_WRONLY|O_CREAT|O_EXCL, mode)) < 0) {
		uistatus("Can't open destination file %s: %s",
		         path, strerror(errno));
		errno = 0;
		goto cleanup;
	}

	if (!download(item, dest))
		goto cleanup;

	if (!item->tag)
		item->tag = path;
	return;
cleanup:
	free(path);
	return;
}

static int
fetchitem(Item *item)
{
	int sock;

	if ((sock = connectto(item->host, item->port)) < 0 ||
	    sendselector(sock, item->selector) < 0)
		return 0;
	item->raw = getrawitem(sock);
	close(sock);

	if (item->raw && !*item->raw) {
		uistatus("Empty response from server");
		clear(&item->raw);
	}

	return (item->raw != NULL);
}

static void
plumb(char *url)
{
	switch (fork()) {
	case -1:
		uistatus("Couldn't fork.");
		return;
	case 0:
		parent = 0;
		dup2(devnullfd, 1);
		dup2(devnullfd, 2);
		if (execlp(plumber, plumber, url, NULL) < 0)
			uistatus("execlp: plumb(%s): %s", url, strerror(errno));
	}
}

static void
plumbitem(Item *item)
{
	char *file, *path, *tag;
	mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP;
	int n, dest, plumbitem;

	if (file = strrchr(item->selector, '/'))
		++file;
	else
		file = item->selector;

	path = uiprompt("Download %s to (^D cancel, <empty> plumb): ",
	                file);
	if (!path)
		return;

	if ((tag = item->tag) && access(tag, R_OK) < 0) {
		clear(&item->tag);
		tag = NULL;
	}

	plumbitem = path[0] ? 0 : 1;

	if (!path[0]) {
		clear(&path);
		if (!tag) {
			n = snprintf(NULL, 0, "%s/%s", tmpdir, file);
			path = xmalloc(++n);
			snprintf(path, n, "%s/%s", tmpdir, file);
		}
	}

	if (path && (!tag || strcmp(tag, path))) {
		if ((dest = open(path, O_WRONLY|O_CREAT|O_EXCL, mode)) < 0) {
			uistatus("Can't open destination file %s: %s",
			         path, strerror(errno));
			errno = 0;
			goto cleanup;
		}
		if (!download(item, dest) || tag)
			goto cleanup;
	}

	if (!tag)
		item->tag = path;

	if (plumbitem)
		plumb(item->tag);

	return;
cleanup:
	free(path);
	return;
}

static int
dig(Item *entry, Item *item)
{
	if (item->raw) /* already in cache */
		return item->type;
	if (!item->entry)
		item->entry = entry ? entry : item;

	switch (item->type) {
	case 'h': /* fallthrough */
		if (!strncmp(item->selector, "URL:", 4)) {
			plumb(item->selector+4);
			return 0;
		}
	case '0':
		if (!fetchitem(item))
			return 0;
		break;
	case '1':
	case '+':
	case '7':
		if (!fetchitem(item) || !(item->dat = molddiritem(item->raw)))
			return 0;
		break;
	case '4':
	case '5':
	case '6':
	case '9':
		downloaditem(item);
		return 0;
	case 'g':
	case 'I':
		plumbitem(item);
		return 0;
	default:
		uistatus("Type %c (%s) not supported",
		        item->type, typedisplay(item->type));
		return 0;
	}

	return item->type;
}

static char *
searchselector(Item *item)
{
	char *pexp, *exp, *tag, *selector = item->selector;
	size_t n = strlen(selector);

	if ((tag = item->tag) && !strncmp(tag, selector, n))
		pexp = tag + n+1;
	else
		pexp = "";

	if (!(exp = uiprompt("Enter search string (^D cancel) [%s]: ", pexp)))
		return NULL;

	if (exp[0] && strcmp(exp, pexp)) {
		n += strlen(exp) + 2;
		tag = xmalloc(n);
		snprintf(tag, n, "%s\t%s", selector, exp);
	}

	free(exp);
	return tag;
}

static int
searchitem(Item *entry, Item *item)
{
	char *sel, *selector;

	if (!(sel = searchselector(item)))
		return 0;

	if (sel != item->tag) {
		clearitem(item);
		selector = item->selector;
		item->selector = item->tag = sel;
		dig(entry, item);
		item->selector = selector;
	}
	return (item->dat != NULL);
}

static void
delve(Item *hole)
{
	Item *entry = NULL;

	while (hole) {
		switch (hole->type) {
		case 'h':
		case '0':
			if (dig(entry, hole))
				displaytextitem(hole);
			break;
		case '1':
		case '+':
			if (dig(entry, hole) && hole->dat)
				entry = hole;
			break;
		case '7':
			if (searchitem(entry, hole))
				entry = hole;
			break;
		case '4':
		case '5':
		case '6': /* TODO decode? */
		case '9':
		case 'g':
		case 'I':
			dig(entry, hole);
			break;
		case 0:
			uistatus("Couldn't get %s:%s/%c%s", hole->host,
			         hole->port, hole->type, hole->selector);
		}

		if (!entry)
			return;

		do {
			uidisplay(entry);
			hole = uiselectitem(entry);
		} while (hole == entry);
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

	if (*host == '\0' || *port == '\0' || ipv6 || *gopherpath == '\0')
		die("Can't parse url");

	entry = xcalloc(sizeof(Item));
	entry->type = gopherpath[0];
	entry->username = entry->selector = ++gopherpath;
	entry->host = host;
	entry->port = port;
	entry->entry = entry;

	return entry;
}

static void
cleanup(void)
{
	clearitem(mainentry);
	if (parent)
		rmdir(tmpdir);
	free(mainentry);
	free(mainurl);
	uicleanup();
}

static void
setup(void)
{
	int fd;

	setenv("PAGER", "more", 0);
	atexit(cleanup);
	/* reopen stdin in case we're reading from a pipe */
	if ((fd = open("/dev/tty", O_RDONLY)) < 0)
		die("open: /dev/tty: %s", strerror(errno));
	if (dup2(fd, 0) < 0)
		die("dup2: /dev/tty, stdin: %s", strerror(errno));
	close(fd);
	if ((devnullfd = open("/dev/null", O_WRONLY)) < 0)
		die("open: /dev/null: %s", strerror(errno));
	if (mkdir(tmpdir, S_IRWXU) < 0 && errno != EEXIST)
		die("mkdir: %s: %s", tmpdir, strerror(errno));
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
