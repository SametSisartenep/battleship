enum {
	Twater,
	Tship,
	Thit,
	Tmiss,
	NTILES,

	Boardmargin = 50,
	TW = 16,
	TH = TW,
	MAPW = 17,
	MAPH = MAPW,
	SCRW = Boardmargin+MAPW*TW+Boardmargin,
	SCRH = Boardmargin+MAPH*TH+TH+MAPH*TH+Boardmargin,
};

typedef struct Input Input;
typedef struct Board Board;

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
