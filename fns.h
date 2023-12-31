#define HZ2MS(hz)	(1000/(hz))

/*
 * alloc
 */
void *emalloc(ulong);
void *erealloc(void*, ulong);
char *estrdup(char*);
Image *eallocimage(Display*, Rectangle, ulong, int, ulong);

/*
 * util
 */
int isoob(Point2);
int cell2coords(char*, ulong, Point2);
Point2 coords2cell(char*);
int gettile(Map*, Point2);
void settile(Map*, Point2, int);
void settiles(Map*, Point2, int, int, int);
void fprintmap(int, Map*);
int countshipcells(Map*);
int shiplen(int);
char *shipname(int);
char *statename(int);
int min(int, int);
int max(int, int);
int bitpackmap(uchar*, ulong, Map*);
int bitunpackmap(Map*, uchar*, ulong);
int chanvprint(Channel*, char*, va_list);
ulong getrand(ulong);

/*
 * menulist
 */
Menulist *newmenulist(int, char*);
void delmenulist(Menulist*);

/*
 * parse
 */
Cmdbuf *parsecmd(char*, int);
Cmdtab *lookupcmd(Cmdbuf*, Cmdtab*, int);

/*
 * andy
 */
Andy *newandy(Player*);
void freeandy(Andy*);
