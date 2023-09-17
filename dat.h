enum {
	Twater,
	Tship,
	Thit,
	Tmiss,
	NTILES,

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
};

typedef struct Ship Ship;
typedef struct Map Map;
typedef struct Board Board;
typedef struct Chanpipe Chanpipe;
typedef struct Player Player;
typedef struct Match Match;
typedef struct Msg Msg;
typedef struct Stands Stands;

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

typedef struct Mentry Mentry;
typedef struct Mlist Mlist;
typedef struct Matchlist Matchlist;

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

struct Matchlist
{
	Mlist;
	int selected;	/* [-1,nitems) where -1 is none */
};
