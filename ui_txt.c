#include <ctype.h>
#include <errno.h>
#include <stdio.h>
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
	     "h: this help.");
}

static int
ndigits(size_t n)
{
	return (n < 10) ? 1 : (n < 100) ? 2 : 3;
}

static void
printstatus(Item *item, char c)
{
	size_t nitems = item->dir ? item->dir->nitems : 0;

	printf("%3lld%%%*c %s:%s%s [%c]: ",
	       (item->printoff + lines >= nitems) ? 100 :
	       ((unsigned long long)item->printoff + lines) * 100 / nitems,
	       ndigits(nitems)+2, '|',
	       item->host, item->port, item->selector, c);
}

void
display(Item *entry)
{
	Item **items;
	size_t i, lines, nitems;
	int nd;

	if (entry->type != '1' || !entry->dir)
		return;

	items = entry->dir->items;
	nitems = entry->dir->nitems;
	lines = entry->printoff + termlines();
	nd = ndigits(nitems);

	for (i = entry->printoff; i < nitems && i < lines; ++i) {
		printf("%*zu %s %s\n",
		       nd, i+1, typedisplay(items[i]->type), items[i]->username);
	}

	fflush(stdout);
}

Item *
selectitem(Item *entry)
{
	static char c;
	char buf[BUFSIZ], nl;
	int item, nitems, lines;

	nitems = entry->dir ? entry->dir->nitems : 0;
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
			if (lines < nitems - entry->printoff &&
			    lines < (size_t)-1 - entry->printoff)
				entry->printoff += lines;
			return entry;
		case 'p':
			lines = termlines();
			if (lines <= entry->printoff)
				entry->printoff -= lines;
			else
				entry->printoff = 0;
			return entry;
		case 'b':
			lines = termlines();
			if (nitems > lines)
				entry->printoff = nitems - lines;
			else
				entry->printoff = 0;
			return entry;
		case 't':
			entry->printoff = 0;
			return entry;
		case '!':
			if (entry->raw)
				continue;
			return entry;
		case 'h':
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
		return entry->dir->items[item-1];

	return entry->entry;
}
