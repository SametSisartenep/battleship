enum {
	Twater,
	Tship,
	Thit,
	Tmiss,
	NTILES,

	TBITS = 2, /* ceil(log(NTILES)/log(2)) */
	TMASK = (1<<TBITS) - 1,

	Scarrier = 0,
	Sbattleship,
	Scruiser,
	Ssubmarine,
	Sdestroyer,
	NSHIPS,

	OH, /* horizontal */
	OV, /* vertical */

	Waiting0 = 0,
	Watching,
	Ready,
	Outlaying,
	Waiting,
	Playing,

	Boardmargin = 50,
	TW = 16,
	TH = TW,
	MAPW = 17,
	MAPH = MAPW,
	SCRW = Boardmargin+MAPW*TW+Boardmargin,
	SCRH = Boardmargin+MAPH*TH+TH+MAPH*TH+Boardmargin,

	KB = 1024,
	BY2MAP = TBITS*MAPW*MAPH/8+1,
};

typedef struct Ship Ship;
typedef struct Map Map;
typedef struct Board Board;
typedef struct Chanpipe Chanpipe;
typedef struct Player Player;
typedef struct Match Match;
typedef struct Msg Msg;
typedef struct Stands Stands;
typedef struct MatchInfo MatchInfo;

struct Ship
{
	Point2 p;	/* board cell */
	Rectangle bbox;
	int orient;
	int ncells;
	int *hit;	/* |hit| = ncells and hitᵢ ∈ {0,1} */
};

struct Map
{
	char map[MAPW][MAPH];
};

struct Board
{
	RFrame;
	Map;
	Rectangle bbox;
};

struct Chanpipe
{
	Channel *in;
	Channel *out;
	Channel *ctl;
	int fd;
};

struct Player
{
	Map;
	char name[8+1];
	int state;
	Match *battle;
	NetConnInfo *nci;
	Chanpipe io;
	Channel *ctl;
};

struct Match
{
	RWLock;
	int id;
	Player *pl[2];
	Channel *data;
	Channel *ctl;
	Match *prev;
	Match *next;
};

struct Msg
{
	Player *from;
	char *body;
};

struct Stands
{
	Player **seats;
	ulong nused;
	ulong cap;
};

struct MatchInfo
{
	int id;
	char *pl[2];
	Board *bl[2];
};

typedef struct Mentry Mentry;
typedef struct Mlist Mlist;
typedef struct Menulist Menulist;

struct Mentry
{
	int id;
	char *title;
};

struct Mlist
{
	Mentry *entries;
	int nentries;
	int filling;
};

struct Menulist
{
	Mlist;
	char *title;
	Rectangle r, sr;	/* content and scroll rects */
	int high;		/* [-1,nitems) where -1 is none */
	int off;		/* entry offset ∈ [0, nitems-Maxvisitems] */

	void (*add)(Menulist*, int, char*);
	void (*clear)(Menulist*);
	int (*update)(Menulist*, Mousectl*, Channel*);
	void (*draw)(Menulist*, Image*);
};

/*
 * Kernel-style command parser
 */
typedef struct Cmdbuf Cmdbuf;
typedef struct Cmdtab Cmdtab;

struct Cmdbuf
{
	char	*buf;
	char	**f;
	int	nf;
};

struct Cmdtab
{
	int	index;	/* used by client to switch on result */
	char	*cmd;	/* command name */
	int	narg;	/* expected #args; 0 ==> variadic */
};
