/* See LICENSE file for copyright and license details. */

/* Screen UI navigation keys */
#define _key_lndown	'j' /* move one line down */
#define _key_entrydown	'J' /* move to next link */
#define _key_lnup	'k' /* move one line up */
#define _key_entryup	'K' /* move to previous link */
#define _key_pgdown	' ' /* move one screen down */
#define _key_pgup	'b' /* move one screen up */
#define _key_home	'g' /* move to the top of page */
#define _key_end	'G' /* move to the bottom of page */
#define _key_pgnext	'l' /* view highlighted item */
#define _key_pgprev	'h' /* view previous item */
#define _key_cururi	'U' /* print page uri */
#define _key_seluri	'u' /* print item uri */
#define _key_fetch	'L' /* refetch current item */
#define _key_help	'?' /* display help */
#define _key_quit	'q' /* exit sacc */
#define _key_search	'/' /* search */
#define _key_searchnext	'n' /* search same string forward */
#define _key_searchprev	'N' /* search same string backward */

#define PLAIN ""

static struct type_def _types[] = {
	DEFTYPE(TYPE_TEXT, '0', "Text", CYAN BOLD, BOLD),
	DEFTYPE(TYPE_DIR,  '1', "Dir ", YELLOW BOLD, BOLD),
	DEFTYPE(TYPE_CSO,  '2', "CSO ", BOLD, BOLD),
	DEFTYPE(TYPE_ERR,  '3', "Err ", BOLD, BOLD),
	DEFTYPE(TYPE_MACF, '4', "Macf", BOLD, BOLD),
	DEFTYPE(TYPE_DOSF, '5', "DOSf", BOLD, BOLD),
	DEFTYPE(TYPE_UUEF, '6', "UUEf", BOLD, BOLD),
	DEFTYPE(TYPE_FIND, '7', "Find", BOLD, BOLD),
	DEFTYPE(TYPE_TELN, '8', "Tlnt", BOLD, BOLD),
	DEFTYPE(TYPE_BINF, '9', "Binf", BOLD, BOLD),
	DEFTYPE(TYPE_MIRR, '+', "Mirr", BOLD, BOLD),
	DEFTYPE(TYPE_IBMT, 'T', "IBMt", BOLD, BOLD),
	DEFTYPE(TYPE_GIF,  'g', "GIF ", BOLD, BOLD),
	DEFTYPE(TYPE_IMG,  'I', "Img ", BOLD, BOLD),
	DEFTYPE(TYPE_HTML, 'h', "HTML", BOLD, BOLD),
	DEFTYPE(TYPE_LIT,  'i', "    ", PLAIN, PLAIN),

	DEFTYPE(TYPE_RESV, '\0', "!   ", BOLD, BOLD),
	DEFTYPE(TYPE_UNKN, '\0', "UNKN", BOLD, BOLD),
};

/* default plumber */
static char *plumber = "xdg-open";

/* temporary directory */
static char *tmpdir = "/tmp/sacc";
