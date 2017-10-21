/* See LICENSE file for copyright and license details. */

/* Screen UI navigation keys */
#define _key_lndown	'j' /* move one line down */
#define _key_entrydown	'J' /* move to next link */
#define _key_lnup	'k' /* move one line up */
#define _key_entryup	'K' /* move to next link */
#define _key_pgdown	' ' /* move one screen down */
#define _key_pgup	'b' /* move one screen up */
#define _key_home	'g' /* move to the top of page */
#define _key_end	'G' /* move to the bottom of page */
#define _key_pgnext	'l' /* view highlighted item */
#define _key_pgprev	'h' /* view previous item */
#define _key_uri	'u' /* print item uri */
#define _key_fetch	'L' /* refetch current item */
#define _key_help	'?' /* display help */
#define _key_quit	'q' /* exit sacc */
#define _key_search	'/' /* search */
#define _key_searchnext	'n' /* search same string forward */
#define _key_searchprev	'N' /* search same string backward */

/* default plumber */
static char *plumber = "xdg-open";

/* temporary directory */
static char *tmpdir = "/tmp/sacc";
