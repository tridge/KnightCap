/* this contains all the code the the hash tables (transposition tables) 

   The transposition table has 2 levels. The first level is replaced if the new entry 
   has a higher priority than the old entry. The 2nd level is a "always replace" level
   where entries are placed if they are of lower priority than the first level.
 */
#include "includes.h"
#include "knightcap.h"

int hash_hits, hash_lookups, hash_inserts;
static int bad_updates, good_updates;
static unsigned hash_tag=0;

extern struct state *state;

#if APLINUX
static unsigned num_cells;
static unsigned cell;
#endif

#define PAR_HASH_LEVEL -1

struct hash_entry *hash_table;
unsigned hash_table_size;

static int initialised;

void hash_reset_stats(void)
{
	hash_lookups = hash_hits = hash_inserts = 0;
	bad_updates = good_updates = 0;
}

void hash_reset(void)
{
	if (!initialised) return;

	if (!hash_table) {
		init_hash_table();
		return;
	}

	memset(hash_table, 0, 
	       sizeof(hash_table[0])*hash_table_size);
	lprintf(0,"reset hash table\n");
}


void init_hash_table(void)
{
	int size;

	if (initialised) return;
	initialised = 1;

	hash_reset_stats();

	do {
		hash_table_size = (state->hash_table_size * 1024*1024) / 
			sizeof(hash_table[0]);

#if HASH_LEVELS
		hash_table_size &= ~(HASH_LEVELS - 1);
#endif

		size = hash_table_size*sizeof(hash_table[0]);

#if USE_SMP
		hash_table = (struct hash_entry *)shm_allocate(size);
#else
		hash_table = (struct hash_entry *)malloc(size);
#endif

		if (!hash_table) {
			state->hash_table_size--;
		}
	} while (!hash_table);

	printf("hash table size %d MB  %d entries\n", 
	       state->hash_table_size, hash_table_size);

	hash_reset();
	order_reset();
#if APLINUX
	num_cells = getncel();
	cell = getcid();
#endif
}

static inline unsigned get_index(Position *b)
{
	unsigned ret;
	ret = b->hash1 % hash_table_size;
#if HASH_LEVELS
	ret &= ~(HASH_LEVELS - 1);
#endif	
	return ret;
}

int check_hash(Position *b, 
	       int depth, Eval testv, Eval *v, Move *move)
{
	struct hash_entry *t;
	uint32 hashindex;

#if NO_QUIESCE_HASH
	if (depth <= 0) return 0;
#endif

	hashindex = get_index(b);

	hash_lookups++;

	t = &hash_table[hashindex];

#if APLINUX
	if (depth > PAR_HASH_LEVEL) {
		int get_flag = 0;
		int cid = (b->hash1 >> HASH_TABLE_BITS) % num_cells;
		get(cid, &b->h_entry, sizeof(*t), t, &get_flag, NULL);
		amcheck(&get_flag, 1);
	} else {
		b->h_entry = (*t);
	}
	t = &b->h_entry;
#endif
	
#if HASH_LEVELS
	{
		int j;
		for (j=0;j<HASH_LEVELS;j++, t++)
			if (t->hash2 == b->hash2 && 
			    t->hash1 == b->hash1) break;
		if (j == HASH_LEVELS) {
			return HASH_MISS;
		}
	}
#else
	if (t->hash2 != b->hash2 || 
	    t->hash1 != b->hash1) {
		return HASH_MISS;
	}
#endif

	if (move) {
		/* even if we get a hash miss we return the move
		   if we have one - it might be a good guess */
		move->from = t->from;
		move->to = t->to;
	}

	if (t->depth_high >= depth && EV(t->high) < EV(testv)) {
		hash_hits++;
		(*v) = t->high;
		return HASH_HIT;
	}

	if (t->depth_low >= depth && EV(t->low) >= EV(testv)) {
		hash_hits++;
		(*v) = t->low;
		return HASH_HIT;
	}

	if (t->depth_high2 >= depth && EV(t->high2) < EV(testv)) {
		hash_hits++;
		(*v) = t->high2;
		return HASH_HIT;
	}

	if (t->depth_low2 >= depth && EV(t->low2) >= EV(testv)) {
		hash_hits++;
		(*v) = t->low2;
		return HASH_HIT;
	}

	return HASH_MISS;
}


struct hash_entry *fetch_hash(Position *b)
{
	struct hash_entry *t;
	uint32 hashindex;

	init_hash_table();

	hashindex = get_index(b);
	t = &hash_table[hashindex];

#if HASH_LEVELS
	{
		int j;
		for (j=0;j<HASH_LEVELS;j++, t++)
			if (t->hash2 == b->hash2 && 
			    t->hash1 == b->hash1) break;
		if (j == HASH_LEVELS) {
			return NULL;
		}
	}
#else
	if (t->hash2 != b->hash2 || 
	    t->hash1 != b->hash1) {
		return 0;
	}
#endif

	return t;
}

static inline int maxdepth(struct hash_entry *t)
{
	return imax(t->depth_low, t->depth_high);
}

void insert_hash(Position *b,
		 int depth, Eval testv, Eval evaluation, Move *move)
{
	struct hash_entry *t;
	uint32 hashindex;
	int lower;

#if NO_QUIESCE_HASH
	if (depth <= 0) return;
#endif

	hashindex = get_index(b);

	lower = (EV(evaluation) >= EV(testv)); /* is this a new lower bound? */

#if APLINUX
	struct hash_entry *t1;
	int cid;
#endif

	hash_inserts++;
	
	t = &hash_table[hashindex];

#if APLINUX
	t1 = t;

	t = &b->h_entry;
#endif

	/* see if it matches either entry */
	if (t[0].hash1 == b->hash1 && t[0].hash2 == b->hash2) {
		/* we match the deep entry, don't need to do anything */
	} else if (t[1].hash1 == b->hash1 && t[1].hash2 == b->hash2) {
		/* we match the "always" entry. This might refresh the 
		   hash tag on the always entry, so check if the always
		   and deep entries need swapping */
		if (maxdepth(&t[1]) >= maxdepth(&t[0])) {
			struct hash_entry tmp;
			tmp = t[0];
			t[0] = t[1];
			t[1] = tmp;
		} else {
			/* they don't need swapping, just use the
                           "always" entry */
			t++;
		}
	} else if (t[0].tag == hash_tag && maxdepth(&t[0]) > depth) {
		/* the deep entry is deeper than the new entry. Use
		   the always entry unless this is a quiesce entry
		   and the always entry isn't */
		if (depth == 0 && maxdepth(&t[1]) > 0 && t[1].tag == hash_tag)
			return;
		/* put it in the "always replace" slot */
		t++;
	} else {
		/* replace the deep entry, and move the deep entry to the 
		 "always" slot */
		t[1] = t[0];
	}

	t->tag = hash_tag;


	if (t->hash2 != b->hash2 || t->hash1 != b->hash1) {
		t->depth_low = 0;
		t->depth_high = 0;
		t->depth_low2 = 0;
		t->depth_high2 = 0;
		t->low2 = t->low = makeeval(b, -INFINITY);
		t->high2 = t->high = makeeval(b, INFINITY);
		t->hash1 = b->hash1;
		t->hash2 = b->hash2;
		t->from = t->to = A1;
	} else {
		if (depth < (lower?t->depth_low:t->depth_high)) {
			bad_updates++;
		} else if ((lower?t->depth_low:t->depth_high) > 0) {
			good_updates++;
		}
	}

	if (lower) {
		if (move) {
			t->from = move->from;
			t->to = move->to;
		}

		if (depth >= t->depth_low) {
			t->depth_low = depth;
			t->low = evaluation;
			if (EV(t->high) < EV(t->low)) {
				t->high = makeeval(b, INFINITY);
				t->depth_high = 0;
			}
		} else {
			t->depth_low2 = depth;
			t->low2 = evaluation;
			if (EV(t->high2) < EV(t->low2)) {
				t->high2 = makeeval(b, INFINITY);
				t->depth_high2 = 0;
			}			
		}
	} else {
		if (move && t->from == A1 && t->to == A1) {
			t->from = move->from;
			t->to = move->to;
		}

		if (depth >= t->depth_high) {
			t->depth_high = depth;
			t->high = evaluation;
			if (EV(t->high) < EV(t->low)) {
				t->low = makeeval(b, -INFINITY);
				t->depth_low = 0;
			}
		} else {
			t->depth_high2 = depth;
			t->high2 = evaluation;
			if (EV(t->high2) < EV(t->low2)) {
				t->low2 = makeeval(b, -INFINITY);
				t->depth_low2 = 0;
			}
		}
	}


#if APLINUX
	if (depth > PAR_HASH_LEVEL) {
		cid = (b->hash1 >> HASH_TABLE_BITS) % num_cells;
		put(cid, t, sizeof(*t), t1, NULL, NULL, 0);
	} else {
		(*t1) = (*t);
	}
#endif
}

void hash_change_tag(int move_num)
{
	static int last_move_num;
	if (move_num < last_move_num) {
		lprintf(0, "move num decreased %d %d\n", move_num, last_move_num);
		hash_reset();
	}

	last_move_num = move_num;

	hash_tag = (hash_tag % 15) + 1;
}

char *hashstats(void)
{
	static char ret[100];
	sprintf(ret, "hits=%d%% bad=%d%% good=%d%%", 
		(100*hash_hits)/(hash_lookups+1),
		(100*bad_updates)/(hash_inserts+1),
		(100*good_updates)/(hash_inserts+1));
	return ret;
}



int check_hash2(Position *b, int depth, Eval testv, Eval *v)
{
	struct hash_entry *t;
	uint32 hashindex = get_index(b);

	t = &hash_table[hashindex];

#if HASH_LEVELS
	{
		int j;
		for (j=0;j<HASH_LEVELS;j++, t++)
			if (t->hash2 == b->hash2 && 
			    t->hash1 == b->hash1) break;
		if (j == HASH_LEVELS) {
			return 0;
		}
	}
#else
	if (t->hash2 != b->hash2 || 
	    t->hash1 != b->hash1) {
		return 0;
	}
#endif

	if (t->depth_high >= depth && EV(t->high) < EV(testv)) {
		hash_hits++;
		(*v) = t->high;
		return 1;
	}

	return 0;
}

int ettc_check_hash(Position *b, Move *moves, int num_moves,
		    int depth, Eval testv, Eval *v, Move *m1)
{
	int m;
	uint32 hash1, hash2;
	Eval v1;
	int cutoff = 0;

	hash1 = b->hash1;
	hash2 = b->hash2;

	testv = flip(testv);

	depth--;

	for (m=0;m<num_moves;m++) {
		remove_hash(b, moves[m].from, b->board[moves[m].from]);
		if (b->board[moves[m].to]) {
			remove_hash(b, moves[m].to, b->board[moves[m].to]);
		}
		add_hash(b, moves[m].to, b->board[moves[m].from]);
		b->hash1 ^= 1;
		
		if (check_hash2(b, depth, testv, &v1)) {
			/* now confirm it */
			Position b1;

			b->hash1 = hash1;
			b->hash2 = hash2;

			if (!do_move(&b1, b, &moves[m])) continue;
			if (check_repitition(&b1, 1)) continue;

			if (check_hash2(&b1, depth, testv, &v1)) {
				if (!cutoff || (-EV(v1)) > EV(*v)) { 
					cutoff = 1;
					(*v) = v1;
					EV(*v) = -EV(*v);
					if (m1)
						(*m1) = moves[m];
				}
			}
		} else {
			b->hash1 = hash1;
			b->hash2 = hash2;
		}
	}

	return cutoff;
}


/* return >0 if this move could produce a hash hit, <0 if it can't
   and =0 if we can't tell */
int hash_ordering(Position *b, Move *move, Eval testv)
{
	uint32 hash1, hash2, hashindex;
	struct hash_entry *t;

	hash1 = b->hash1;
	hash2 = b->hash2;

	testv = flip(testv);

	remove_hash(b, move->from, b->board[move->from]);
	if (b->board[move->to]) {
		remove_hash(b, move->to, b->board[move->to]);
	}
	add_hash(b, move->to, b->board[move->from]);
	b->hash1 ^= 1;


	hashindex = get_index(b);
	t = &hash_table[hashindex];

#if HASH_LEVELS
	{
		int j;
		for (j=0;j<HASH_LEVELS;j++, t++)
			if (t->hash2 == b->hash2 && 
			    t->hash1 == b->hash1) break;
		if (j == HASH_LEVELS) {
			b->hash1 = hash1;
			b->hash2 = hash2;
			return 0;
		}
	}
#else
	if (t->hash2 != b->hash2 || 
	    t->hash1 != b->hash1) {
		b->hash1 = hash1;
		b->hash2 = hash2;
		return 0;
	}
#endif

	b->hash1 = hash1;
	b->hash2 = hash2;

	if (EV(t->low) >= EV(testv)) {
		return -(1+((EV(t->low) - EV(testv)) / (PAWN_VALUE/2)));
	}

	if (EV(t->high) < EV(testv)) {
		return 1+((EV(testv) - EV(t->high)) / (PAWN_VALUE/2));
	}

	return 0;
}
