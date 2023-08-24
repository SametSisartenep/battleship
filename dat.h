enum {
	Twater,
	Tship,
	Thit,
	Tmiss,
	NTILES,

	OH, /* horizontal */
	OV, /* vertical */

	Waiting0 = 0,
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

typedef struct Input Input;
typedef struct Ship Ship;
typedef struct Board Board;
typedef struct Player Player;
typedef struct Playerq Playerq;
typedef struct Chanpipe Chanpipe;

struct Input
{
	Mousectl *mc;
	Keyboardctl *kc;
};

struct Ship
{
	Point2 p; /* board cell */
	Rectangle bbox;
	int orient;
	int ncells;
	int *hit; /* |hit| = ncells and hit ∈ {0,1} */
	int sunk;
};

struct Board
{
	RFrame;
	char map[17][17];
	Rectangle bbox;
};

struct Player
{
	int fd;
	int sfd;
	int state;
};

struct Playerq
{
	QLock;
	Player **players;
	ulong cap;
	ulong nplayers;
};

struct Chanpipe
{
	Channel *c;
	int fd;
};
