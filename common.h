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
	char *tag;
	void *dat;
	Item *entry;
};

struct dir {
	Item *items;
	size_t nitems;
	size_t printoff;
	size_t curline;
};

#ifndef asprintf
int asprintf(char **s, const char *fmt, ...);
#endif /* asprintf */
void die(const char *fmt, ...);
size_t mbsprint(const char *s, size_t len);
const char *typedisplay(char t);
void uidisplay(Item *entry);
Item *uiselectitem(Item *entry);
void uistatus(char *fmt, ...);
void uicleanup(void);
char *uiprompt(char *fmt, ...);
void uisetup(void);
void uisigwinch(int signal);
