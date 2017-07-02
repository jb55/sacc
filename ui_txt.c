#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include "common.h"

void
uisetup(void) { return; }
void
uicleanup(void) { return; }

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
termlines(void)
{
	struct winsize ws;

	if (ioctl(1, TIOCGWINSZ, &ws) < 0) {
		die("Could not get terminal resolution: %s",
		    strerror(errno));
	}

	return ws.ws_row-1;
}

void
display(Item *item)
{
	Item **items;
	size_t i, lines, nitems;
	int ndigits;

	if (item->type != '1')
		return;

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
}

Item *
selectitem(Item *entry)
{
	char buf[BUFSIZ], nl;
	int item, nitems, lines;

	nitems = entry->dir ? entry->dir->nitems : 0;

	do {
		printf("%d items (h for help): ", nitems);
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
