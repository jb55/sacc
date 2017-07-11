#define clear(p)	do { void **_p = (void **)(p); free(*_p); *_p = NULL; } while (0);

typedef struct item Item;
typedef struct dir Dir;

struct item {
	char type;
	char *username;
	char *selector;
	char *host;
	char *port;
	char *raw;
	size_t printoff;
	size_t curline;
	Item *entry;
	Dir  *dir;
};

struct dir {
	Item **items;
	size_t nitems;
};

void die(const char *fmt, ...);
void display(Item *item);
Item *selectitem(Item *entry);
const char *typedisplay(char t);
void uicleanup(void);
char *uiprompt(char *s);
void uisetup(void);
