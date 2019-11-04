#define clear(p)	do { void **_p = (void **)(p); free(*_p); *_p = NULL; } while (0);
#define ARRAY_SIZE(x) ((int)(sizeof(x) / sizeof((x)[0])))

#define CYAN   "\x1b[36m"
#define YELLOW "\x1b[33m"
#define BOLD   "\x1b[1m"
#define RESET  "\x1b[0m"

#define DEFTYPE(type_, id_, label_, typestyle_, textstyle_)				\
	[type_] = ((struct type_def){ .id = id_, .label = label_,	\
			.style = typestyle_, .textstyle = textstyle_ })

typedef struct item Item;
typedef struct dir Dir;

enum type {
	TYPE_TEXT,
	TYPE_DIR,
	TYPE_CSO,
	TYPE_ERR,
	TYPE_MACF,
	TYPE_DOSF,
	TYPE_UUEF,
	TYPE_FIND,
	TYPE_TELN,
	TYPE_BINF,
	TYPE_MIRR,
	TYPE_IBMT,
	TYPE_GIF,
	TYPE_IMG,
	TYPE_HTML,
	TYPE_LIT,
	TYPE_RESV,
	TYPE_UNKN,
};

struct item {
	char type;
	char redtype;
	char *username;
	char *selector;
	char *host;
	char *port;
	char *raw;
	char *tag;
	void *dat;
	Item *entry;
};

struct type_def {
	char id;
	char *label;
	char *style;
	char *textstyle;
};

struct dir {
	Item *items;
	size_t nitems;
	size_t printoff;
	size_t curline;
};

void die(const char *fmt, ...);
size_t mbsprint(const char *s, size_t len);
#ifdef NEED_STRCASESTR
char *strcasestr(const char *h, const char *n);
#endif /* NEED_STRCASESTR */
const char *typedisplay(char t);
void uicleanup(void);
void uidisplay(Item *entry);
char *uiprompt(char *fmt, ...);
Item *uiselectitem(Item *entry);
void uisetup(void);
void uisigwinch(int signal);
void uistatus(char *fmt, ...);
enum type parse_type(char t);
