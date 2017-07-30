#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include "common.h"

int lines;

static int
termlines(void)
{
	struct winsize ws;

	if (ioctl(1, TIOCGWINSZ, &ws) < 0) {
		die("Could not get terminal resolution: %s",
		    strerror(errno));
	}

	return ws.ws_row-1; /* one off for status bar */
}

void
uisetup(void) {
	lines = termlines();
}

void
uicleanup(void) {
	return;
}

void
help(void)
{
	puts("Commands:\n"
	     "N = [1-9]...: browse item N.\n"
	     "0: browse previous item.\n"
	     "n: show next page.\n"
	     "p: show previous page.\n"
	     "t: go to the top of the page\n"
	     "b: go to the bottom of the page\n"
	     "!: refetch failed item.\n"
	     "^D, q: quit.\n"
	     "h, ?: this help.");
}

static int
ndigits(size_t n)
{
	return (n < 10) ? 1 : (n < 100) ? 2 : 3;
}

void
uistatus(char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	vprintf(fmt, arg);
	va_end(arg);

	printf(" <Press Key to continue>");
	fflush(stdout);

	getchar();
}

static void
printstatus(Item *item, char c)
{
	Dir *dir = item->dat;
	size_t nitems = dir ? dir->nitems : 0;
	unsigned long long printoff = dir ? dir->printoff : 0;

	printf("%3lld%%%*c %s:%s%s [%c]: ",
	       (printoff + lines >= nitems) ? 100 :
	       (printoff + lines) * 100 / nitems, ndigits(nitems)+2, '|',
	       item->host, item->port, item->selector, c);
}

char *
uiprompt(char *fmt, ...)
{
	va_list ap;
	char *input = NULL;
	size_t n = 0;
	ssize_t r;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);

	fflush(stdout);

	if ((r = getline(&input, &n, stdin)) < 0)
		clearerr(stdin);
	else if (input[r - 1] == '\n')
		input[--r] = '\0';

	return input;
}

void
uidisplay(Item *entry)
{
	Item **items;
	Dir *dir;
	size_t i, lines, nitems;
	int nd;

	if (!entry ||
	    !(entry->type == '1' || entry->type == '+' || entry->type == '7') ||
	    !(dir = entry->dat))
		return;

	items = dir->items;
	nitems = dir->nitems;
	lines = dir->printoff + termlines();
	nd = ndigits(nitems);

	for (i = dir->printoff; i < nitems && i < lines; ++i) {
		printf("%*zu %s %s\n",
		       nd, i+1, typedisplay(items[i]->type), items[i]->username);
	}

	fflush(stdout);
}

Item *
uiselectitem(Item *entry)
{
	Dir *dir;
	static char c;
	char buf[BUFSIZ], nl;
	int item, nitems, lines;

	if (!entry || !(dir = entry->dat))
		return NULL;

	nitems = dir ? dir->nitems : 0;
	if (!c)
		c = 'h';

	do {
		item = -1;
		printstatus(entry, c);
		fflush(stdout);

		if (!fgets(buf, sizeof(buf), stdin)) {
			putchar('\n');
			return NULL;
		}
		if (isdigit(*buf))
			c = '\0';
		else if (!strcmp(buf+1, "\n"))
			c = *buf;

		switch (c) {
		case '\0':
			break;
		case 'q':
			return NULL;
		case 'n':
			lines = termlines();
			if (lines < nitems - dir->printoff &&
			    lines < (size_t)-1 - dir->printoff)
				dir->printoff += lines;
			return entry;
		case 'p':
			lines = termlines();
			if (lines <= dir->printoff)
				dir->printoff -= lines;
			else
				dir->printoff = 0;
			return entry;
		case 'b':
			lines = termlines();
			if (nitems > lines)
				dir->printoff = nitems - lines;
			else
				dir->printoff = 0;
			return entry;
		case 't':
			dir->printoff = 0;
			return entry;
		case '!':
			if (entry->raw)
				continue;
			return entry;
		case 'h':
		case '?':
			help();
			continue;
		default:
			c = 'h';
			continue;
		}

		if (*buf < '0' || *buf > '9')
			continue;

		nl = '\0';
		if (sscanf(buf, "%d%c", &item, &nl) != 2 || nl != '\n')
			item = -1;
	} while (item < 0 || item > nitems);

	if (item > 0)
		return dir->items[item-1];

	return entry->entry;
}
