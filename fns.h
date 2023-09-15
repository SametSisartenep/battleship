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
char *cell2coords(Point2);
Point2 coords2cell(char*);
int gettile(Map*, Point2);
void settile(Map*, Point2, int);
void settiles(Map*, Point2, int, int, int);
void fprintmap(int, Map*);
int countshipcells(Map*);
int shiplen(int);
char *shipname(int);
char *statename(int);
