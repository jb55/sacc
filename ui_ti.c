#include <stdio.h>
#include <term.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>

#include "common.h"

static struct termios tsave;

void
uisetup(void)
{
	struct termios traw;

	tcgetattr(0, &tsave);
	traw = tsave;
	cfmakeraw(&traw);
	tcsetattr(0, TCSAFLUSH, &traw);

	setupterm(NULL, 1, NULL);
}

void
uicleanup(void)
{
	tcsetattr(0, TCSAFLUSH, &tsave);
	putp(tparm(clear_screen));
}

static void
printitem(Item *item)
{
	printf("%-4s%c %s\r",
	       item->type != 'i' ? typedisplay(item->type) : "",
	       item->type > '1' ? '|' : '+', item->username);
}

static void
help(void)
{
	return;
}

void
display(Item *item)
{
	Item **items;
	size_t i, curln, lastln, nitems, printoff;

	if (item->type > '1')
		return;

	putp(tparm(clear_screen));
	putp(tparm(save_cursor));

	switch (item->type) {
	case '0':
		puts(item->raw);
		break;
	case '1':
		items = item->dir->items;
		nitems = item->dir->nitems;
		printoff = item->printoff;
		curln = item->curline;
		lastln = printoff + lines;

		for (i = printoff; i < nitems && i < lastln; ++i) {
			if (item = items[i]) {
				if (i != printoff)
					putp(tparm(cursor_down));
				if (i == curln) {
					putp(tparm(save_cursor));
					putp(tparm(enter_standout_mode));
				}
				printitem(item);
				putp(tparm(column_address, 0));
				if (i == curln)
					putp(tparm(exit_standout_mode));
			}
		}
		break;
	}

	putp(tparm(restore_cursor));
	fflush(stdout);
}

static void
movecurline(Item *item, int l)
{
	size_t nitems;
	ssize_t curline, offline;

	if (item->dir == NULL)
		return;

	curline = item->curline + l;
	nitems = item->dir->nitems;
	if (curline < 0 || curline >= nitems)
		return;

	printitem(item->dir->items[item->curline]);
	item->curline = curline;

	if (l > 0) {
		offline = item->printoff + lines;
		if (curline - item->printoff >= lines / 2 && offline < nitems) {
			putp(tparm(save_cursor));

			putp(tparm(cursor_address, lines, 0));
			putp(tparm(scroll_forward));
			printitem(item->dir->items[offline]);

			putp(tparm(restore_cursor));
			item->printoff += l;
		}
	} else {
		offline = item->printoff + l;
		if (curline - offline <= lines / 2 && offline >= 0) {
			putp(tparm(save_cursor));

			putp(tparm(cursor_address, 0, 0));
			putp(tparm(scroll_reverse));
			printitem(item->dir->items[offline]);
			putchar('\n');

			putp(tparm(restore_cursor));
			item->printoff += l;
		}
	}
	
	putp(tparm(cursor_address, curline - item->printoff, 0));
	putp(tparm(enter_standout_mode));
	printitem(item->dir->items[curline]);
	putp(tparm(exit_standout_mode));
	fflush(stdout);
}

Item *
selectitem(Item *entry)
{
	char c;
	Item *hole;
	int item, nitems;

	for (;;) {
		switch (c = getchar()) {
		case 'b':
			return entry->entry;
		case 'g':
			if (entry->dir->items[entry->curline]->type < '2')
				return entry->dir->items[entry->curline];
			continue;
		case 'n':
			movecurline(entry, 1);
			continue;
		case 'p':
			movecurline(entry, -1);
			continue;
		case 'q':
			return NULL;
		case '!':
			if (entry->raw)
				continue;
			return entry;
		case 'h': /* FALLTHROUGH */
			help();
		default:
			continue;

		}
	}
}