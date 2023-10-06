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

	GMPvP = 0,
	GMPvAI,

	Waiting0 = 0,
	Watching,
	Ready,
	Outlaying,
	Waiting,
	Playing,

	ASearching = 0,
	ACalibrating,
	ABombing,

	Boardmargin = 50,
	TW = 16,
	TH = TW,
	MAPW = 17,
	MAPH = MAPW,
	SCRW = Boardmargin + Borderwidth+MAPW*TW+Borderwidth + Boardmargin,
	SCRH = Boardmargin+
		Borderwidth+MAPH*TH+Borderwidth+
		TH+
		Borderwidth+MAPH*TH+Borderwidth+
		Boardmargin,

	SBG0 = 0,
	SBG1,
	SBG2,
	SCANNON,
	SWATER,
	SVICTORY,
	SDEFEAT,
	NSOUNDS,

	KB = 1024,
	BY2MAP = (TBITS*MAPW*MAPH+7)/8,
};

typedef struct Ship Ship;
typedef struct Map Map;
typedef struct Board Board;
typedef struct Chanpipe Chanpipe;
typedef struct Player Player;
typedef struct Andy Andy;
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
	int gamemode;
	Match *battle;
	NetConnInfo *nci;
	Chanpipe io;
	Channel *ctl;
};

struct Andy
{
	Map;		/* of the enemy */
	Player *ego;
	int state;
	Point2 lastshot;
	Point2 firsthit;
	Point2 passdir;	/* direction of current pass */
	int ntries;	/* attempts left to find the direction */
	int passes;	/* remaining passes (one per direction) */

	void (*layout)(Andy*, Msg*);
	void (*shoot)(Andy*, Msg*);
	void (*engage)(Andy*);
	void (*disengage)(Andy*);
	void (*registerhit)(Andy*);
	void (*registermiss)(Andy*);
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
	struct {
		char uid[8+1];
		int state;
	} pl[2];
	Board *bl[2];
	char conclusion[16];
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
	int filling;	/* lock-alike */
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
