#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>

#include "config.h"
#include "common.h"

#define C(c) #c
#define S(c) C(c)

static char bufout[256];
static struct termios tsave;
static struct termios tsacc;
static Item *curentry;
#if defined(__NetBSD__)
#undef tparm
#define tparm tiparm
#endif

void
uisetup(void)
{
	tcgetattr(0, &tsave);
	tsacc = tsave;
	tsacc.c_lflag &= ~(ECHO|ICANON);
	tcsetattr(0, TCSANOW, &tsacc);

	setupterm(NULL, 1, NULL);
	putp(tparm(clear_screen));
	putp(tparm(save_cursor));
	putp(tparm(change_scroll_region, 0, lines-2));
	putp(tparm(restore_cursor));
	fflush(stdout);
}

void
uicleanup(void)
{
	putp(tparm(change_scroll_region, 0, lines-1));
	putp(tparm(clear_screen));
	tcsetattr(0, TCSANOW, &tsave);
	fflush(stdout);
}

char *
uiprompt(char *fmt, ...)
{
	va_list ap;
	char *input = NULL;
	size_t n;
	ssize_t r;

	putp(tparm(save_cursor));

	putp(tparm(cursor_address, lines-1, 0));
	putp(tparm(clr_eol));
	putp(tparm(enter_standout_mode));

	va_start(ap, fmt);
	if (vsnprintf(bufout, sizeof(bufout), fmt, ap) >= sizeof(bufout))
		bufout[sizeof(bufout)-1] = '\0';
	va_end(ap);
	printf("%.*s", columns, bufout);

	putp(tparm(exit_standout_mode));

	if ((n = strlen(bufout)) < columns)
		printf("%*s", columns - n, " ");

	putp(tparm(cursor_address, lines-1, n));

	tsacc.c_lflag |= (ECHO|ICANON);
	tcsetattr(0, TCSANOW, &tsacc);
	fflush(stdout);

	n = 0;
	r = getline(&input, &n, stdin);

	tsacc.c_lflag &= ~(ECHO|ICANON);
	tcsetattr(0, TCSANOW, &tsacc);
	putp(tparm(restore_cursor));
	fflush(stdout);

	if (r < 0) {
		clearerr(stdin);
		clear(&input);
	} else if (input[r - 1] == '\n') {
		input[--r] = '\0';
	}

	return input;
}

static void
printitem(Item *item)
{
	if (snprintf(bufout, sizeof(bufout), "%s %s", typedisplay(item->type),
	    item->username) >= sizeof(bufout))
		bufout[sizeof(bufout)-1] = '\0';
	printf("%.*s\r", columns, bufout);
}

static Item *
help(Item *entry)
{
	static Item item = {
		.type = '0',
		.raw = "Commands:\n"
		       "Down, " S(_key_lndown) ": move one line down.\n"
		       "Up, " S(_key_lnup) ": move one line up.\n"
		       "PgDown, " S(_key_pgdown) ": move one page down.\n"
		       "PgUp, " S(_key_pgup) ": move one page up.\n"
		       "Home, " S(_key_home) ": move to top of the page.\n"
		       "End, " S(_key_end) ": move to end of the page.\n"
		       "Right, " S(_key_pgnext) ": view highlighted item.\n"
		       "Left, " S(_key_pgprev) ": view previous item.\n"
		       S(_key_search) ": search current page.\n"
		       S(_key_search_next) ": search string forward.\n"
		       S(_key_search_prev) ": search string backward.\n"
		       S(_key_uri) ": print item uri.\n"
		       S(_key_help) ": show this help.\n"
		       "^D, " S(_key_quit) ": exit sacc.\n"
	};

	item.entry = entry;

	return &item;
}

void
uistatus(char *fmt, ...)
{
	va_list ap;
	size_t n;

	putp(tparm(save_cursor));

	putp(tparm(cursor_address, lines-1, 0));
	putp(tparm(enter_standout_mode));

	va_start(ap, fmt);
	n = vsnprintf(bufout, sizeof(bufout), fmt, ap);
	va_end(ap);

	if (n < sizeof(bufout)-1) {
		n += snprintf(bufout + n, sizeof(bufout) - n,
		              " [Press a key to continue ☃]");
	}
	if (n >= sizeof(bufout))
		bufout[sizeof(bufout)-1] = '\0';

	printf("%.*s", columns, bufout);
	putp(tparm(exit_standout_mode));
	if ((n = strlen(bufout)) < columns)
		printf("%*s", columns - n, " ");

	putp(tparm(restore_cursor));
	fflush(stdout);

	getchar();
}

static void
displaystatus(Item *item)
{
	Dir *dir = item->dat;
	char *fmt;
	size_t n, nitems = dir ? dir->nitems : 0;
	unsigned long long printoff = dir ? dir->printoff : 0;

	putp(tparm(save_cursor));

	putp(tparm(cursor_address, lines-1, 0));
	putp(tparm(enter_standout_mode));
	fmt = (strcmp(item->port, "70") && strcmp(item->port, "gopher")) ?
	      "%1$3lld%%| %2$s:%5$s/%3$c%4$s" : "%3lld%%| %s/%c%s";
	if (snprintf(bufout, sizeof(bufout), fmt,
	             (printoff + lines-1 >= nitems) ? 100 :
	             (printoff + lines-1) * 100 / nitems,
	             item->host, item->type, item->selector, item->port)
	    >= sizeof(bufout))
		bufout[sizeof(bufout)-1] = '\0';
	printf("%.*s", columns, bufout);
	putp(tparm(exit_standout_mode));
	if ((n = strlen(bufout)) < columns)
		printf("%*s", columns - n, " ");

	putp(tparm(restore_cursor));
	fflush(stdout);
}

static void
displayuri(Item *item)
{
	char *fmt;
	size_t n;

	if (item->type == 0 || item->type == 'i')
		return;

	putp(tparm(save_cursor));

	putp(tparm(cursor_address, lines-1, 0));
	putp(tparm(enter_standout_mode));
	switch (item->type) {
	case 'h':
		n = snprintf(bufout, sizeof(bufout), "%s: %s",
		             item->username, item->selector);
		break;
	default:
		fmt = strcmp(item->port, "70") ?
		      "%1$s: gopher://%2$s:%5$s/%3$c%4$s" :
		      "%s: gopher://%s/%c%s";
		n = snprintf(bufout, sizeof(bufout), fmt,
		             item->username, item->host, item->type,
		             item->selector, item->port);
		break;
	}

	if (n >= sizeof(bufout))
		bufout[sizeof(bufout)-1] = '\0';

	printf("%.*s", columns, bufout);
	putp(tparm(exit_standout_mode));
	if ((n = strlen(bufout)) < columns)
		printf("%*s", columns - n, " ");

	putp(tparm(restore_cursor));
	fflush(stdout);
}

void
uidisplay(Item *entry)
{
	Item *items;
	Dir *dir;
	size_t i, curln, lastln, nitems, printoff;

	if (!entry ||
	    !(entry->type == '1' || entry->type == '+' || entry->type == '7'))
		return;

	curentry = entry;

	putp(tparm(clear_screen));
	displaystatus(entry);

	if (!(dir = entry->dat))
		return;

	putp(tparm(save_cursor));

	items = dir->items;
	nitems = dir->nitems;
	printoff = dir->printoff;
	curln = dir->curline;
	lastln = printoff + lines-1; /* one off for status bar */

	for (i = printoff; i < nitems && i < lastln; ++i) {
		if (i != printoff)
			putp(tparm(cursor_down));
		if (i == curln) {
			putp(tparm(save_cursor));
			putp(tparm(enter_standout_mode));
		}
		printitem(&items[i]);
		putp(tparm(column_address, 0));
		if (i == curln)
			putp(tparm(exit_standout_mode));
	}

	putp(tparm(restore_cursor));
	fflush(stdout);
}

static void
movecurline(Item *item, int l)
{
	Dir *dir = item->dat;
	size_t nitems;
	ssize_t curline, offline;
	int plines = lines-2;

	if (dir == NULL)
		return;

	curline = dir->curline + l;
	nitems = dir->nitems;
	if (curline < 0 || curline >= nitems)
		return;

	printitem(&dir->items[dir->curline]);
	dir->curline = curline;

	if (l > 0) {
		offline = dir->printoff + lines-1;
		if (curline - dir->printoff >= plines / 2 && offline < nitems) {
			putp(tparm(save_cursor));

			putp(tparm(cursor_address, plines, 0));
			putp(tparm(scroll_forward));
			printitem(&dir->items[offline]);

			putp(tparm(restore_cursor));
			dir->printoff += l;
		}
	} else {
		offline = dir->printoff + l;
		if (curline - offline <= plines / 2 && offline >= 0) {
			putp(tparm(save_cursor));

			putp(tparm(cursor_address, 0, 0));
			putp(tparm(scroll_reverse));
			printitem(&dir->items[offline]);
			putchar('\n');

			putp(tparm(restore_cursor));
			dir->printoff += l;
		}
	}

	putp(tparm(cursor_address, curline - dir->printoff, 0));
	putp(tparm(enter_standout_mode));
	printitem(&dir->items[curline]);
	putp(tparm(exit_standout_mode));
	displaystatus(item);
	fflush(stdout);
}

static void
jumptoline(Item *entry, ssize_t line, int absolute)
{
	Dir *dir = entry->dat;
	size_t lastitem;
	int lastpagetop, plines = lines-2;

	if (!dir)
		return;
	lastitem = dir->nitems-1;

	if (line < 0)
		line = 0;
	if (line > lastitem)
		line = lastitem;

	if (dir->curline == line)
		return;

	if (lastitem <= plines) {              /* all items fit on one page */
		dir->curline = line;
	} else if (line == 0) {                /* jump to top */
		if (absolute || dir->curline > plines || dir->printoff == 0)
			dir->curline = 0;
		dir->printoff = 0;
	} else if (line + plines < lastitem) { /* jump before last page */
		dir->curline = line;
		dir->printoff = line;
	} else {                               /* jump within the last page */
		lastpagetop = lastitem - plines;
		if (dir->printoff == lastpagetop || absolute)
			dir->curline = line;
		else if (dir->curline < lastpagetop)
			dir->curline = lastpagetop;
		dir->printoff = lastpagetop;
	}

	uidisplay(entry);
	return;
}

void
searchinline(const char *searchstr, Item *entry, int pos)
{
	Dir *dir;
	int i;

	if (!searchstr || !(dir = entry->dat))
		return;

	if (pos > 0) {
		for (i = dir->curline + 1; i < dir->nitems; ++i) {
			if (strstr(dir->items[i].username, searchstr)) {
				jumptoline(entry, i, 1);
				break;
			}
		}
	} else {
		for (i = dir->curline - 1; i > -1; --i) {
			if (strstr(dir->items[i].username, searchstr)) {
				jumptoline(entry, i, 1);
				break;
			}
		}
	}
}

static ssize_t
nearentry(Item *entry, int direction)
{
	Dir *dir = entry->dat;
	size_t item, lastitem;

	if (!dir)
		return -1;
	lastitem = dir->nitems;
	item = dir->curline + direction;

	for (; item >= 0 && item < lastitem; item += direction) {
		if (dir->items[item].type != 'i')
			return item;
	}

	return dir->curline;
}

Item *
uiselectitem(Item *entry)
{
	Dir *dir;
	char *searchstr = NULL;
	int plines = lines-2;

	if (!entry || !(dir = entry->dat))
		return NULL;

	for (;;) {
		switch (getchar()) {
		case 0x1b: /* ESC */
			switch (getchar()) {
			case 0x1b:
				goto quit;
			case '[':
				break;
			default:
				continue;
			}
			switch (getchar()) {
			case '4':
				if (getchar() != '~')
					continue;
				goto end;
			case '5':
				if (getchar() != '~')
					continue;
				goto pgup;
			case '6':
				if (getchar() != '~')
					continue;
				goto pgdown;
			case 'A':
				goto lnup;
			case 'B':
				goto lndown;
			case 'C':
				goto pgnext;
			case 'D':
				goto pgprev;
			case 'H':
				goto home;
			case 0x1b:
				goto quit;
			}
			continue;
		case _key_pgprev:
		pgprev:
			return entry->entry;
		case _key_pgnext:
		case '\n':
		pgnext:
			if (dir)
				return &dir->items[dir->curline];
			continue;
		case _key_lndown:
		lndown:
			movecurline(entry, 1);
			continue;
		case _key_entrydown:
			jumptoline(entry, nearentry(entry, 1), 1);
			continue;
		case _key_pgdown:
		pgdown:
			jumptoline(entry, dir->printoff + plines, 0);
			continue;
		case _key_end:
		end:
			jumptoline(entry, dir->nitems, 0);
			continue;
		case _key_lnup:
		lnup:
			movecurline(entry, -1);
			continue;
		case _key_entryup:
			jumptoline(entry, nearentry(entry, -1), 1);
			continue;
		case _key_pgup:
		pgup:
			jumptoline(entry, dir->printoff - plines, 0);
			continue;
		case _key_home:
		home:
			jumptoline(entry, 0, 0);
			continue;
		case _key_search:
		search:
			free(searchstr);
			if ((searchstr = uiprompt("Search for: ")) &&
			    searchstr[0])
				goto searchnext;
			clear(&searchstr);
			continue;
		case _key_searchnext:
		searchnext:
			searchinline(searchstr, entry, +1);
			continue;
		case _key_searchprev:
			searchinline(searchstr, entry, -1);
			continue;
		case _key_quit:
		quit:
			return NULL;
		case _key_fetch:
		fetch:
			if (entry->raw)
				continue;
			return entry;
		case _key_uri:
			if (dir)
				displayuri(&dir->items[dir->curline]);
			continue;
		case _key_help: /* FALLTHROUGH */
			return help(entry);
		default:
			continue;
		}
	}
}

void
uisigwinch(int signal)
{
	setupterm(NULL, 1, NULL);
	putp(tparm(change_scroll_region, 0, lines-2));

	uidisplay(curentry);
}
