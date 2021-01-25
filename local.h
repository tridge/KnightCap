
#if MID_KNIGHT
#define NO_RESIGN
#endif

#define ICS_WHISPER "whisper"

/* this file is used to conveniently add local definitions */
#if LEARN_EVAL
#define RAMP_FACTOR 0
#define LARGE_ETYPE 1
#define RESIGN_VALUE INFINITY
/* for self play learning set USE_PBRAIN to 0. For ICS learning 
   set it to 1 */
#define USE_PBRAIN 1
#define SHORT_GAME 20
#define STORE_LEAF_POS 1
#define MAX_GAME_MOVES 600
#define EVAL_DIFF 0.05 /* max acceptable eval deviation 
			  between root and leaf (tanh1 - tanh2)*/
#define EVAL_SCALE (atanh(0.25)/PAWN_VALUE) /* tanh(1 pawn up) = 0.25 */
#define USE_SLIDING_CONTROL 1
#endif


#if TRIDGE
#define USE_SLIDING_CONTROL 1
#define POSITIONAL_FACTOR 1
#define USE_BOOK 1
#define CONST_EVAL const
#define LARGE_ETYPE 0
#define STORE_LEAF_POS 0
#define FLOAT_ETYPE 0
#define BOOK_LEARN 0
#define PERFECT_EVAL 0
#define USE_HASH_TABLES 1
#define MTD_THRESHOLD (0.07*PAWN_VALUE)
#define RANDOM_ORDERING 0
#define NO_QUIESCE 0
#define NO_QUIESCE_HASH 0
#define USE_ETTC_HASH 1
#endif

#define MAX_CPUS 20

