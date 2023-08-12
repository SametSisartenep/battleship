enum {
	Twater,
	Tship,
	Thit,
	Tmiss,
	NTILES,

	Waiting0 = 0,
	Outlaying,
	Waiting1,
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
typedef struct Board Board;
typedef struct Ship Ship;
typedef struct Player Player;
typedef struct Playerq Playerq;
typedef struct Chanpipe Chanpipe;

struct Input
{
	Mousectl *mc;
	Keyboardctl *kc;
};

struct Board
{
	RFrame;
	char map[17][17];
	Rectangle bbox;
};

struct Ship
{
	RFrame;
	int ncells;
	int sunk;
};

struct Player
{
	int fd;
	int sfd;
	Player *o; /* opponent */
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
