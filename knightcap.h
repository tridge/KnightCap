#if LARGE_ETYPE
typedef float etype;
#define PAWN_VALUE 100
#else
typedef short etype;
#define PAWN_VALUE 100
#define LARGE_ETYPE 0
#endif

extern etype coefficients[];

#include "eval.h"

#define DEFAULT_HASH_TABLE_SIZE 8 /* in Mb */

#define TD_LAMBDA 0.7
#define TD_ALPHA (10.0/(EVAL_SCALE))
#ifndef RAMP_FACTOR
#define RAMP_FACTOR 1.0
#endif

#define CRAFTY 1
#define KNIGHTCAP 2

#ifndef SHORT_GAME
#define SHORT_GAME 10
#endif

#define CONTENTION_DEPTH 1

#ifndef USE_HASH_TABLES
#define USE_HASH_TABLES 1
#endif
#ifndef USE_ETTC_HASH
#define USE_ETTC_HASH 1
#endif
#define USE_PAWN_DEEPENING 1
#define USE_CHECK_DEEPENING 1
#define USE_RECAPTURE_DEEPENING 1
#define USE_BEST_CAPTURE_DEEPENING 0
#define USE_HUNG_CAPTURE_DEEPENING 0
#define USE_SINGULAR_EXTENSION 1

#define USE_NULL_MOVE 1
#define USE_NULLMOVE_FIXUP 1
#define CONFIRM_NULL_MOVES 1

#define USE_RAZORING 1
#define USE_ASYMMETRIC_RAZORING 0
#define USE_EVAL_SHORTCUT 0
#define USE_QUIESCE_SHORTCUT 1
#define USE_PESSIMISTIC_PRUNING 0
#define USE_ASYMMETRIC_EXTENSIONS 1
#define USE_ASYMMETRIC_EVAL 1
#define USE_ASYMMETRIC_NULLMOVE 0


#ifndef USE_SLIDING_CONTROL
#define USE_SLIDING_CONTROL 1
#endif

#define USE_FAST_QUIESCE 0

#define TEST_QUIESCE 0
#define TEST_EVAL_SHORTCUT 0
#define TEST_NULL_MOVE 0

#define TEST_MTD_FAST 0

#define FUTILE_THRESHOLD (1.3*PAWN_VALUE)

#define USE_INTERNAL_PRUNING 0
#define USE_HALF_PRUNING 0

#ifndef STORE_LEAF_POS
#define STORE_LEAF_POS 0
#endif

#ifndef USE_PBRAIN
#define USE_PBRAIN 1
#endif

#define BRAIN_FILE "brain.dat"

#define BOOK_FILE "book.dat"

#ifndef POSITIONAL_FACTOR
#define POSITIONAL_FACTOR 1
#endif

#ifndef RESIGN_VALUE
#define RESIGN_VALUE (ROOK_VALUE + PAWN_VALUE)
#endif 

#define NULL_MOVE_PLY 3
#define PONDER_NULL_MOVE_PLY 3

#define MAX_EXTENSIONS 20

#define RAZOR_THRESHOLD (0.9*PAWN_VALUE)

#define NULL_MOVE_THRESHOLD 6

#ifndef MTD_THRESHOLD
#define MTD_THRESHOLD (0.07*PAWN_VALUE)
#endif

#define MAX_CELLS 64

#if USE_SMP
#define TIMER_NODES 100
#else
#define TIMER_NODES 1000
#endif

#define CAPTURE_SEARCH_DEPTH 0

#define USE_FORWARD_PRUNING 0
#define PRUNE_THRESHOLD (PAWN_VALUE/4)

#define PAR_SPLIT_LEVEL 1

#define TREE_PRINT_PLY 0

#define MAX_DEPTH 50

#define WALL_CLOCK 1

#define HASH_LEVELS 2

#define HASH_MISS 0
#define HASH_HIT 1
#define HASH_AVOID_NULL 2

#define NUM_SQUARES (8*8)
#define MAX_MOVES (306) /* the maximum number of moves that can be
			   made in one move - probably a gross
			   overestimate */
#ifndef MAX_GAME_MOVES
#define MAX_GAME_MOVES 1000 /* the players should give up by then :-) */
#endif
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

#define FPAWN 0
#define GPAWN 1
#define HPAWN 2
#define HGPAWN 3
#define GHPAWN 4
#define FGPAWN 5
#define APAWN 6
#define BPAWN 7
#define CPAWN 8
#define ABPAWN 9
#define BAPAWN 10
#define CBPAWN 11

#define WHITE_MASK (0x0000FFFF)
#define BLACK_MASK (0xFFFF0000)
#define WKING_MASK (0x00000001)
#define BKING_MASK (0x00010000)
#define KING_MASK  (0x00010001)
#define WQUEEN_MASK (0x00000002)
#define BQUEEN_MASK (0x00020000)
#define QUEEN_MASK  (0x00020002)
#define WROOK_MASK (0x0000000C)
#define WKROOK_MASK (0x00000004)
#define WQROOK_MASK (0x00000008)
#define WBISHOP_MASK (0x00000030)
#define WKBISHOP_MASK (0x00000010)
#define WQBISHOP_MASK (0x00000020)
#define BBISHOP_MASK (0x00300000)
#define BKBISHOP_MASK (0x00100000)
#define BQBISHOP_MASK (0x00200000)
#define BISHOP_MASK   (0x00300030)
#define WKNIGHT_MASK (0x000000C0)
#define BKNIGHT_MASK (0x00C00000)
#define BROOK_MASK (0x000C0000)
#define BKROOK_MASK (0x00040000)
#define BQROOK_MASK (0x00080000)
#define WPAWN_MASK (0x0000FF00)
#define BPAWN_MASK (0xFF000000)
#define WPIECE_MASK (0x000000FF)
#define BPIECE_MASK (0x00FF0000)

#define WMINOR_MASK (WKNIGHT_MASK | WBISHOP_MASK)
#define BMINOR_MASK (BKNIGHT_MASK | BBISHOP_MASK)
#define MINOR_MASK  (WMINOR_MASK | BMINOR_MASK)
#define WMAJOR_MASK (WPIECE_MASK & ~WMINOR_MASK)
#define BMAJOR_MASK (BPIECE_MASK & ~BMINOR_MASK)
#define MAJOR_MASK  (WMAJOR_MASK | BMAJOR_MASK)

#undef INFINITY
#define INFINITY (40.50*PAWN_VALUE)
#define ILLEGAL (-40*PAWN_VALUE)
#define WIN (-(ILLEGAL+1))
#define FORCED_WIN (WIN-50)
#define MAX_EVAL (FORCED_WIN-20)
#define MAX_MATERIAL (30.00*PAWN_VALUE)

#define STALEMATE 2
#define TIME_FORFEIT 3
#define UNKNOWN 4

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
#define FLAG_PROMOTE (FLAG_WHITE_PROMOTE | FLAG_BLACK_PROMOTE)
#define FLAG_ACCEPT_DRAW (1<<13)
#define FLAG_EVAL_DONE   (1<<14)
#define FLAG_WHITE_KINGSIDE  (1<<15)
#define FLAG_WHITE_QUEENSIDE  (1<<16)
#define FLAG_BLACK_KINGSIDE  (1<<17)
#define FLAG_BLACK_QUEENSIDE  (1<<18)
#define FLAG_NEED_PART2       (1<<20)
#define FLAG_FORCED_DRAW      (1<<21)
#define FLAG_BAD_EVAL         (1<<22)
#define FLAG_BRAIN_EVAL       (1<<23)
#define FLAG_GAME_OVER        (1<<24)
#define FLAG_LOST             (1<<25)
#define FLAG_EXTENDED         (1<<26)
#define FLAG_COMPUTER_WHITE   (1<<27)
#define FLAG_DONE_TACTICS     (1<<28)
#define FLAG_FUTILE           (1<<29)

#define FLAG_KQ_SIDE (FLAG_BLACK_QUEENSIDE | FLAG_BLACK_KINGSIDE | FLAG_WHITE_QUEENSIDE | FLAG_WHITE_KINGSIDE)

#define FLAG_WHITE_CAN_CASTLE (WHITE_CASTLE_SHORT | WHITE_CASTLE_LONG)
#define FLAG_BLACK_CAN_CASTLE (BLACK_CASTLE_SHORT | BLACK_CASTLE_LONG)
#define FLAG_CAN_CASTLE (FLAG_WHITE_CAN_CASTLE | FLAG_BLACK_CAN_CASTLE)

#define REPITITION_LENGTH 101

#define mat_value(x) coefficients[IPIECE_VALUES + (x)]

#define KNIGHT_VALUE mat_value(KNIGHT)
#define BISHOP_VALUE mat_value(BISHOP)
#define ROOK_VALUE mat_value(ROOK)
#define QUEEN_VALUE mat_value(QUEEN)
#define KING_VALUE mat_value(KING)

#define INITIAL_MATERIAL (KING_VALUE + QUEEN_VALUE + 2*ROOK_VALUE + 2*BISHOP_VALUE + 2*KNIGHT_VALUE + 8*PAWN_VALUE)

typedef short int16;
typedef unsigned short uint16;
typedef unsigned uint32;
typedef unsigned long long uint64;

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

#ifndef DEBUG_FRIENDLY
#define DEBUG_FRIENDLY 1
#endif
#if DEBUG_FRIENDLY
typedef SquareT Square;
typedef PieceT Piece;
#else
typedef unsigned char Square;
typedef signed char Piece;
#endif

typedef enum {OPENING=0, MIDDLE, ENDING, MATING} GameStage;

typedef struct {
	Square from, to;
	int v;
} Move;

#if STORE_LEAF_POS
typedef struct {
	Piece board[NUM_SQUARES];
	uint32 flags;
	int stage;
	short move_num;
	Square enpassent;
	unsigned char fifty_count;
} SmallPos;
#endif

#if STORE_LEAF_POS
typedef struct {
	/* use a short to hold evaluations */
	etype v;
	SmallPos pos;
} Eval;
#define EV(e) ((e).v)
#else
typedef etype Eval;
#define EV(e) (e)
#endif

#if STORE_LEAF_POS
typedef struct {
	etype v;
	uint32 flags;
} LeafEval;
#endif


typedef struct {
	Piece p;
	Square pos;
} PieceStruct;

struct hash_entry {
	uint32 hash1;
	uint32 hash2;
	Eval low;
	Eval high;
	Eval low2;
	Eval high2;
	unsigned char depth_low;
	unsigned char depth_high;
	unsigned char depth_low2;
	unsigned char depth_high2;
	unsigned from:6;
	unsigned to:6;
	unsigned tag:4;
};

typedef struct Position {
	/* this section needs to be copied in do_move */
	uint32 hash1, hash2;
	uint32 sliding_mask; /* mask of pieces that can "slide"
				  (bishops, rooks, queens) */
	uint32 pawns7th;     /* mask of pawns on the 7th rank */
	uint32 piece_mask;   /* mask of which pieces are not pawns */
	uint32 material_mask; /* mask of all pieces on the board */
	etype w_material;       /* whites material value */
	etype b_material;       /* blacks material value */
	uint32 piece_change; /* a bitmap of which pieces need to be
				  recalculated */
	uint32 hung_mask;    /* a bitmap of pieces which are hung. This means
				  they are on a square that the opponent
				  controls */
	uint32 pinned_mask;    /* a bitmap of pieces which are pinned. */
	uint32 attacking_mask; /* a bitmap of pieces which are attacking
				    some enemy piece, and are thus being
				    useful */
	Piece board[NUM_SQUARES]; /* the board itself */
	char pboard[NUM_SQUARES]; /* pointers into the piece list */
	PieceStruct pieces[32];   /* the piece list - this is never
				     reordered as otherwise the masks
				     would become invalid */
	etype control[NUM_SQUARES]; /* the board control value for 
				      each square */
	char safe_mobility[32]; /* mobility value for each piece */
	char mobility[32]; /* mobility value for each piece */
	int white_moves;            /* whites total mobility */
	int black_moves;            /* blacks total mobility */
	etype piece_values[32];      /* the value of each piece at the last
				       eval call */
	int stage;             /* stage of the game */
	etype board_control;   /* the previous board control value */
	etype expensive;       /* the expensive eval components from 
				 the last eval */
	etype evaluation;      /* evaluation after a search */
	uint32 flight[NUM_SQUARES]; /* a bitmap for each square
				       which says what pieces can use
				       this square as a safe retreat */
#if 1
	uint32 wpassed_pawn_mask; /* a mask of all the white passed pawns */
	uint32 bpassed_pawn_mask; /* a mask of all the black passed pawns */
	uint32 null_stoppable_pawn; /* a mask of the currently
				       unstoppable pawns that would
				       become stoppable if we null
				       move */
	uint32 null_unstoppable_pawn; /* a mask of the currently
				       stoppable pawns that would
				       become unstoppable if we null
				       move */
#endif
	Move best_capture;
	/* and this section doesn't get copied by do_move */
	unsigned part1_marker; /* only copy up to here for part1 */
	unsigned dont_copy; /* dummy variable */
	uint32 topieces[NUM_SQUARES]; /* a bitmap for each square
					   which says what pieces are
					   supporting/attacking the
					   square. This is the core of
					   the move generator and the
					   board control function */
#if USE_SLIDING_CONTROL
	uint32 xray_topieces[NUM_SQUARES]; /* a bitmap for each square
					   which says what pieces are
					   xray-supporting/attacking the
					   square. */
#endif
	Move *moves;
	int num_moves;
	Move last_move;
	etype tactics;         /* the tactical eval component */
	uint32 flags;
	int enpassent;
	int promotion;
	int fifty_count;
	int move_num;
	unsigned winner;
	struct hash_entry h_entry;
	struct Position *oldb;
	etype eval_result;
} Position;

#if USE_SMP
struct slave {
	unsigned index;
	int depth;
	int testv, v;
	int nodes;
	int mo_hits, mo_misses;
	int maxply;
	int busy;
	short x;
	Move move;
	int done;
};
#endif

struct state {
	Position position;
	int computer;
	int moved;
	int pondering;
	float curquat[4];
	float scale;
	int quit;
	int stop_search;
	float move_time;
	float time_limit;
	int always_think;
	int use_mulling;
	int use_pbrain;
	int flip_view;
	int ics_robot;
	int autoplay;
	int hash_table_size;
	int need_reset;
	Move game_record[MAX_GAME_MOVES];
	uint32 hash_list[MAX_GAME_MOVES]; /* for repitition */
	int won;
	int colour;
	int rating_change;
	int tuned;
	int nps;
	int ics_otim;
	int ics_time;
#if STORE_LEAF_POS
	LeafEval leaf_eval[MAX_GAME_MOVES];
	etype grad[MAX_GAME_MOVES*__TOTAL_COEFFS__];
	short predicted_move[MAX_GAME_MOVES];
	int stored_move_num;
#endif
#if USE_SMP
	int num_cpus;
	volatile struct slave slavep[MAX_CPUS];
	int nodes_total[MAX_CPUS];
#endif
	double eval_weakness;
	double max_move_time;
	unsigned max_search_depth;
	double dementia;
};

#define WHITEPIECES(b) ((b)->pieces)
#define BLACKPIECES(b) ((b)->pieces + 16)
#define PIECES(b,player) ((player)>0?WHITEPIECES(b):BLACKPIECES(b))


#include "proto.h"

#if defined(i386) && !defined(NO_ASM)
static inline int ff_one(uint32 word)
{
	__asm__("bsfl %1,%0"
		:"=r" (word)
		:"r" (word));
	return word;
}

static inline int fl_one(uint32 word)
{
	__asm__("bsrl %1,%0"
		:"=r" (word)
		:"r" (word));
	return word;
}
#elif defined(__powerpc__) && !defined(NO_ASM)
static inline int ff_one(uint32 word)
{
	int ret;

	word &= ~(word - 1);
	__asm__("cntlzw %0,%1" : "=r" (ret) : "r" (word));
	return 31 - ret;
}

static inline int fl_one(uint32 word)
{
	int ret;

	__asm__("cntlzw %0,%1" : "=r" (ret) : "r" (word));
	return 31 - ret;
}
#else
static inline int ff_one(uint32 mask)
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

static inline int fl_one(uint32 mask)
{
	static const int last[16] = {-1, 0, 1, 1, 2, 2, 2, 2, 3, 
				     3, 3, 3, 3, 3, 3, 3};
	int ret=0;
	if (mask & 0xFFFF0000) {
		mask >>= 16;
		ret += 16;
	}

	if (mask & 0xFF00) {
		mask >>= 8;
		ret += 8;
	}

	if (mask & 0xF0) {
		mask >>= 4;
		ret += 4;
	}

	return ret + last[mask & 0xF];
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

#define SIGN(x) ((x)<0?-1:1)

#define RAMP(x) ((x)<0?(x):RAMP_FACTOR*(x))

static inline int imin(int i1, int i2)
{
	return i1 > i2? i2 : i1;
}

static inline int imax(int i1, int i2)
{
	return i1 > i2? i1 : i2;
}

static inline etype emin(etype i1, etype i2)
{
	return i1 > i2? i2 : i1;
}

static inline etype emax(etype i1, etype i2)
{
	return i1 > i2? i1 : i2;
}

static inline etype shiftr1(etype x)
{
#if FLOAT_ETYPE 
	return x*0.5;
#else
	return x >> 1;
#endif
}
		
static inline etype shiftr2(etype x)
{
#if FLOAT_ETYPE
	return x*0.25;
#else
	return x >> 2;
#endif
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


static inline int is_zero_move(Move *move)
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

static inline int white_square(Square sq)
{
	return ((sq ^ (sq>>3)) & 1);
}

static inline int black_square(Square sq)
{
	return !white_square(sq);
}

static inline int blacks_move(Position *b)
{
	return (b->move_num & 1);
}

static inline int whites_move(Position *b)
{
	return (!blacks_move(b));
}

static inline int next_to_play(Position *b)
{
	if (whites_move(b)) return 1;
	return -1;
}

static inline uint32 player_mask(Position *b)
{
	if (whites_move(b)) return WHITE_MASK;
	return BLACK_MASK;
}

static inline Eval flip(Eval v)
{
	EV(v) = -(EV(v) - 1);
	return v;
}

static inline Eval seteval(Eval v, etype value)
{
	EV(v) = value;
	return v;
}

static inline Eval makeeval(Position *b, etype value)
{
	Eval v;
	EV(v) = value;
#if STORE_LEAF_POS
	memcpy(v.pos.board, b->board, sizeof(v.pos.board));
	v.pos.flags = b->flags;
	v.pos.move_num = b->move_num;
	v.pos.enpassent = b->enpassent;
	v.pos.fifty_count = b->fifty_count;
#endif
	return v;
}

static inline int bit_count(uint32 x)
{
	int count;
	for (count=0; x; count++)
		x &= (x-1);
	return count;
}

static inline struct hash_entry *hash_ptr(uint32 hash1)
{
	extern struct hash_entry *hash_table;
	extern unsigned hash_table_size;
	uint32 hashindex = hash1 % hash_table_size;

#if HASH_LEVELS
	hashindex  &= ~(HASH_LEVELS - 1);
#endif	

	return &hash_table[hashindex];
}

static inline void invert(etype *p1, const etype *p2)
{
	int i;
	
	for (i=A1; i<=H8; i++)
		p1[i] = p2[mirror_square(i)];
}

/* this determines if it is the computers move. This function is only
   valid during a search as the flag is setup at the start of the search
   code. This function is used to implement asymmetric search and eval
   factors */
static inline int computers_move(Position *b)
{
	if (next_to_play(b) == 1)
		return ((b->flags & FLAG_COMPUTER_WHITE) != 0);

	return ((b->flags & FLAG_COMPUTER_WHITE) == 0);
}

extern char capture_map[2*KING+1][NUM_SQUARES][NUM_SQUARES];
extern char sliding_map[NUM_SQUARES][NUM_SQUARES];
extern char edge_square[NUM_SQUARES];
extern uint64 same_line_mask[NUM_SQUARES][NUM_SQUARES];

/* this checks whether 3 squares are in a line. Note that the order is
   important. same_line(A1, B2, C3) is true whereas same_line(B2, C3,
   A1) isn't. The answer is also always false if two of the squares
   are the same */
static inline int same_line(Square p1, Square p2, Square p3)
{
	return sliding_map[p1][p2] &&
		sliding_map[p1][p2] == sliding_map[p2][p3];
}


/* given a direction of travel, will we move off the board by going
   one square in that direction. This relies on the legal values that
   a direction can take. */
static inline int off_board(Square sq, int dir)
{	
	dir = abs(edge_square[sq] + dir);
	return dir != 7 && dir & ~9;
}

/* this gives the distance from A1 or H8, whichever
   is closer */
static inline int corner_distance(Square sq)
{
	static const char dist[64] = {
		0, 1, 2, 3, 4, 5, 6, 7,
		1, 1, 2, 3, 4, 5, 6, 6,
		2, 2, 2, 3, 4, 5, 5, 5,
		3, 3, 3, 3, 4, 4, 4, 4,
		4, 4, 4, 4, 3, 3, 3, 3,
		5, 5, 5, 4, 3, 2, 2, 2,
		6, 6, 5, 4, 3, 2, 1, 1,
		7, 6, 5, 4, 3, 2, 1, 0};
	return dist[sq];
}


struct hashvalue_entry {
	uint32 v1[2*KING+1][NUM_SQUARES];
	uint32 v2[2*KING+1][NUM_SQUARES];
};

extern struct hashvalue_entry hash_values;

static inline void add_hash(Position *b, Square pos, Piece p)
{
	b->hash1 ^= hash_values.v1[p+KING][pos];
	b->hash2 ^= hash_values.v2[p+KING][pos];
}

static inline void remove_hash(Position *b, Square pos, Piece p)
{
	b->hash1 ^= hash_values.v1[p+KING][pos];
	b->hash2 ^= hash_values.v2[p+KING][pos];
}

static inline int distance(Square sq1, Square sq2)
{
	int xdist = abs(XPOS(sq1) - XPOS(sq2));
	int ydist = abs(YPOS(sq1) - YPOS(sq2));
	return imax(xdist, ydist);
}

extern int pop_count[256];
static inline int pop_count16(uint32 x)
{
	return pop_count[(x)&0xFF] + pop_count[((x)>>8)&0xFF];
}

static inline int pop_count32(uint32 x)
{
	return pop_count16((x) & 0xFFFF) + pop_count16((x) >> 16);
}


#define __fast __attribute__ ((__section__ (".text.fast")))
#define __fastdata __attribute__ ((__section__ (".data.fast")))
#define __fastfunc(__argfast) \
        __argfast __fast; \
        __argfast
