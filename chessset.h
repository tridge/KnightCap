#define USE_HASH_TABLES 1
#define USE_CAPTURE_DEEPENING 1
#define USE_PAWN_DEEPENING 1
#define USE_PREVCAPTURE_DEEPENING 0
#define USE_CHECK_DEEPENING 1
#define USE_PREVCHECK_DEEPENING 0
#define USE_KING_DEEPENING 0
#define USE_HUNG_DEEPENING 0
#define USE_EVAL_SHORTCUT 1
#define USE_NULL_MOVE 0
#define USE_SAFE_NULL_MOVE 0
#define USE_HOTSQUARE_EVAL 0
#define USE_POSITIONAL_VALUE 1
#define USE_BOARD_CONTROL 1
#define USE_PAWN_LOC 1
#define USE_RANDOM_EVAL 0
#define USE_HASH_ALPHA 0
#define USE_DEEP_CELLS 0

#define MTD_THRESHOLD 4

#define MAX_CELLS 64

#define CAPTURE_SEARCH_DEPTH 0

#define USE_FORWARD_PRUNING 0
#define PRUNE_THRESHOLD (PAWN_VALUE/5)

#define PAR_SPLIT_LEVEL 1

#define TREE_PRINT_PLY 0

#define MAX_DEPTH 25

#define EVAL_SHORTCUT_THRESHOLD (PAWN_VALUE)

#define WALL_CLOCK 1

#define NUM_SQUARES (8*8)
#define MAX_MOVES (16*28) /* the maximum number of moves that can be
			     made in one move - probably a gross
			     overestimate */
#define MAX_GAME_MOVES 1000 /* the players should give up by then :-) */
#define PIECE_NONE 0
#define PIECE_WHITE 1
#define PIECE_BLACK -1

#define IKING 0
#define IQUEEN 1
#define IKROOK 2
#define IQROOK 3
#define IKBISHOP 4
#define IQBISHOP 5
#define IKKNIGHT 6
#define IQKNIGHT 7

#define IQRPAWN 8
#define IQNPAWN 9
#define IQBPAWN 10
#define IQPAWN 11
#define IKPAWN 12
#define IKBPAWN 13
#define IKNPAWN 14
#define IKRPAWN 15

#define WHITE_MASK (0x0000FFFF)
#define BLACK_MASK (0xFFFF0000)
#define WKING_MASK (0x00000001)
#define BKING_MASK (0x00010000)
#define WROOK_MASK (0x0000000C)
#define BROOK_MASK (0x000C0000)
#define WPAWN_MASK (0x0000FF00)
#define BPAWN_MASK (0xFF000000)
#define WPIECE_MASK (0x000000FF)
#define BPIECE_MASK (0x00FF0000)

#define INFINITY (4010)
#define ILLEGAL (10-INFINITY)
#define WIN (-(ILLEGAL+1))
#define MAX_MATERIAL (3500)

#define STALEMATE 2

#define DEFAULT_MOVE_TIME 10

#define POSN(x,y) ((y) + (x)*8)
#define XPOS(p) ((p)>>3)
#define YPOS(p) ((p)&7)

#undef NORTH
#undef SOUTH
#undef EAST
#undef WEST

#define NORTH (1)
#define SOUTH (-1)
#define EAST  (8)
#define WEST  (-8)
#define NORTH_EAST (NORTH+EAST)
#define NORTH_WEST (NORTH+WEST)
#define SOUTH_EAST (SOUTH+EAST)
#define SOUTH_WEST (SOUTH+WEST)

#define WHITE_CASTLE_SHORT (1<<0)
#define WHITE_CASTLE_LONG (1<<1)
#define BLACK_CASTLE_SHORT (1<<2)
#define BLACK_CASTLE_LONG (1<<3)
#define WHITE_CASTLED (1<<4)
#define BLACK_CASTLED (1<<5)
#define FLAG_CHECK (1<<7)
#define FLAG_PREV_CHECK (1<<8)
#define FLAG_WHITE_PROMOTE (1<<11)
#define FLAG_BLACK_PROMOTE (1<<12)

#define FLAG_WHITE_CAN_CASTLE (WHITE_CASTLE_SHORT | WHITE_CASTLE_LONG)
#define FLAG_BLACK_CAN_CASTLE (BLACK_CASTLE_SHORT | BLACK_CASTLE_LONG)
#define FLAG_CAN_CASTLE (FLAG_WHITE_CAN_CASTLE | FLAG_BLACK_CAN_CASTLE)

#define REPITITION_LENGTH 100

/* lots of things are relative to the material value of a pawn */
#define PAWN_VALUE 100
#define KNIGHT_VALUE 400
#define BISHOP_VALUE 420
#define ROOK_VALUE 600
#define QUEEN_VALUE 1200
#define KING_VALUE 0 /* a king value is never needed */

typedef enum {B_KING = -6,B_QUEEN = -5,B_ROOK = -4,B_BISHOP = -3,
	      B_KNIGHT = -2,B_PAWN = -1,
	      NONE=0, 
	      PAWN=1, KNIGHT=2, BISHOP=3, ROOK=4, QUEEN=5, KING=6} PieceT;

typedef enum {A1=0, A2, A3, A4, A5, A6, A7, A8,
	      B1, B2, B3, B4, B5, B6, B7, B8,
	      C1, C2, C3, C4, C5, C6, C7, C8,
	      D1, D2, D3, D4, D5, D6, D7, D8,
	      E1, E2, E3, E4, E5, E6, E7, E8,
	      F1, F2, F3, F4, F5, F6, F7, F8,
	      G1, G2, G3, G4, G5, G6, G7, G8,
	      H1, H2, H3, H4, H5, H6, H7, H8} SquareT;

#define DEBUG_FRIENDLY 1
#if DEBUG_FRIENDLY
typedef SquareT Square;
typedef PieceT Piece;
#else
typedef char Square;
typedef char Piece;
#endif

typedef enum {OPENING=0, MIDDLE, ENDING} GameStage;

/* use a short to hold evaluations */
typedef short Eval;

typedef struct {
	Square from, to;
} Move;

typedef struct {
	Piece p;
	Square pos;
} PieceStruct;


struct hash_entry {
	unsigned hash2;
	int low:13;
	int high:13;
	int depth:5;
	unsigned tag:1;
};

typedef struct {
	/* this section needs to be copied in do_move */
	unsigned long hash1, hash2;
	unsigned sliding_mask;
	unsigned pawns7th;
	unsigned piece_mask;
	Piece board[NUM_SQUARES];
	char pboard[NUM_SQUARES];
	PieceStruct pieces[32];
	int w_material;
	int b_material;
	int positional_value;
	int stage;

	/* and this section doesn't */
	unsigned dont_copy;
	unsigned topieces[NUM_SQUARES];
	int last_dest;
	int prev_capture;
	int capture;
	unsigned flags;
	int enpassent;
	int promotion;
	int fifty_count;
	int num_moves;
	unsigned winner;
	struct hash_entry h_entry;
} Position;

struct state {
	Position position;
	int next_to_play;
	int computer;
	int moved;
	float curquat[4];
	float scale;
	int quit;
	int stop_search;
	int move_time;
	int bonus_time[3];
	int always_think;
	int flip_view;
	int ics_robot;
	Move game_record[MAX_GAME_MOVES];
	unsigned long hash_list[MAX_GAME_MOVES]; /* for repitition */
};

#define WHITEPIECES(b) ((b)->pieces)
#define BLACKPIECES(b) ((b)->pieces + 16)
#define PIECES(b,player) ((player)>0?WHITEPIECES(b):BLACKPIECES(b))


#include "proto.h"

#ifdef i386
static inline int ff_one(unsigned word)
{
	__asm__("bsfl %1,%0"
		:"=r" (word)
		:"r" (word));
	return word;
}
#else
static inline int ff_one(unsigned mask)
{
	static const int first[16] = {-1, 0, 1, 0, 2, 0, 1, 0, 3, 
				      0, 1, 0, 2, 0, 1, 0};
	int ret=0;
	if (!(mask & 0xFFFF)) {
		mask >>= 16;
		ret += 16;
	}

	if (!(mask & 0xFF)) {
		mask >>= 8;
		ret += 8;
	}

	if (!(mask & 0xF)) {
		mask >>= 4;
		ret += 4;
	}

	return ret + first[mask & 0xF];
}
#endif

/* determine if two pieces or players are the same color */
static inline int same_color(Piece p1, Piece p2)
{
	return ((p1 ^ p2) >= 0);
}

static inline int is_white(Piece p1)
{
	return p1 > 0;
}

static inline int is_black(Piece p1)
{
	return p1 < 0;
}

static inline Square mirror_square(Square sq)
{
	return sq ^ 7;
}

#define ABS(x) ((x)<0?-(x):(x))

static inline int imin(int i1, int i2)
{
	return i1 > i2? i2 : i1;
}

static inline int imax(int i1, int i2)
{
	return i1 > i2? i1 : i2;
}

static inline PieceStruct *get_pboard(Position *b, Square pos)
{
	if (b->pboard[pos] == -1) return NULL;
	return b->pieces + b->pboard[pos];
}

static inline void set_pboard(Position *b, Square pos, PieceStruct *piece)
{
	if (piece == NULL) 
		b->pboard[pos] = -1;
	else
		b->pboard[pos] = piece - b->pieces;
}


static inline int null_move(Move *move)
{
	return (move->from == A1 && move->to == A1);
}

static inline int same_move(Move *m1, Move *m2)
{
	return (m1->from == m2->from && m1->to == m2->to);
}

static inline void zero_move(Move *move)
{
	move->from = move->to = A1;
}

static inline int bad_square(Square sq)
{
	return ((sq & ~63) != 0);
}


extern char movement_map[2*KING+1][NUM_SQUARES][NUM_SQUARES];
extern char capture_map[2*KING+1][NUM_SQUARES][NUM_SQUARES];


