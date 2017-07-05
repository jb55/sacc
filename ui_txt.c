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
printstatus(Item *item)
{
	size_t nitems = item->dir->nitems;

	printf("%3lld%%%*c %s:%s%s (h for help): ",
	       (item->printoff + lines >= nitems) ? 100 :
	       ((unsigned long long)item->printoff + lines) * 100 / nitems,
	       ndigits(nitems)+2, '|', item->host, item->port, item->selector);
}

void
display(Item *entry)
{
	Item *item, **items;
	size_t i, lines, nitems;
	int nd;

	if (entry->type != '1')
		return;

	items = entry->dir->items;
	nitems = entry->dir->nitems;
	lines = entry->printoff + termlines();
	nd = ndigits(nitems);

	for (i = entry->printoff; i < nitems && i < lines; ++i) {
		if (item = items[i]) {
			printf("%*zu %-4s%c %s\n", nd, i+1,
			       item->type != 'i' ?
			       typedisplay(item->type) : "",
			       item->type > '1' ? '|' : '+',
			       items[i]->username);
		} else {
			printf("%*zu  !! |\n", nd, i+1);
		}
	}

	fflush(stdout);
}

Item *
selectitem(Item *entry)
{
	char buf[BUFSIZ], nl;
	int item, nitems, lines;

	nitems = entry->dir ? entry->dir->nitems : 0;

	do {
		printstatus(entry);
		fflush(stdout);

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
