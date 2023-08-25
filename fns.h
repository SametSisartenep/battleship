#define HZ2MS(hz)	(1000/(hz))

/*
 * alloc
 */
void *emalloc(ulong);
void *erealloc(void*, ulong);
Image *eallocimage(Display*, Rectangle, ulong, int, ulong);

/*
 * util
 */
char *cell2coords(Point2);
Point2 coords2cell(char*);
