#include "includes.h"
#include "knightcap.h"

/* the "brain" is an implementation of the permanent brain idea common 
   in chess programs.

   In KnightCap it is also used to develop its own opening book. This
   is done by pondering the positions in the brain while idle,
   constantly refining them.
   */

extern struct state *state;

#define MAX_BRAIN_SIZE 20000
#define MAX_BRAIN_MOVE 30
#define MAX_MULL_TIME (60*2)
#define MIN_MULL_TIME (30)

#define BRAIN_TEMP_SIZE 50

#define FLAG_NEEDS_RESEARCH (1<<0)
#define FLAG_SEARCHED (1<<1)

#define SAVE_FREQUENCY 300 /* in seconds */

struct brain_entry {
	uint32 hash1;
	uint32 hash2;
	int eval_low;
	int eval_high;
	char depth_low;
	char depth_high;
	Square from, to;
	int search_time; /* how long we've spent searching this pos */
	time_t last_play; /* when we last used this entry */
	time_t last_search; /* when we did the last search */
	Piece board[NUM_SQUARES];
	uint32 flags;
	uint32 flags2;
	unsigned short move_num;
	Square enpassent;
	uint32 white_wins, black_wins;
	uint32 unused;
};

static int brain_size;
static struct brain_entry brain[MAX_BRAIN_SIZE + BRAIN_TEMP_SIZE];
static struct brain_entry *brain_temp = &brain[MAX_BRAIN_SIZE];
static int brain_temp_size;
static int mull_entry;
static int last_hit;
static char *brain_fname;
static int done_lookup;
static int last_lookup_hit;
static int load_failed;

int mulling=0;

extern int learning;

static void consistency_check1(struct brain_entry *br);
static void consistency_check2(struct brain_entry *br);


static inline int play_offset(int move_num)
{
	return 60*60*24*move_num;
}

/* return a string describing a brain entry */
static char *entry_str(struct brain_entry *br)
{
	Move m1;
	static char s[1000];

	m1.from = br->from;
	m1.to = br->to;

	sprintf(s,"depth=%d/%d num=%d time=%d v=%d/%d move=%s hash1=%08x %s",
		br->depth_low, 
		br->depth_high, 
		br->move_num,
		br->search_time,
		br->eval_low,
		br->eval_high,
		short_movestr(NULL, &m1),
		br->hash1,
		ctime(&br->last_play));

	return s;
}

static int brain_compare(struct brain_entry *b1, struct brain_entry *b2)
{
	if (b1->hash1 == 0) return 1;
	if (b2->hash1 == 0) return -1;
	if (b1->depth_low == -1) return 1;
	if (b2->depth_low == -1) return -1;
	if (b2->hash1 < b1->hash1)
		return 1;
	if (b2->hash1 > b1->hash1)
		return -1;
	return memcmp(b1->board, b2->board, sizeof(b1->board));
}

/* this one is used when saving to disk so the file remains the same
   with no changes (good for rsync) */
static int brain_comp2(struct brain_entry *b1, struct brain_entry *b2)
{
	if (b1->hash1 == 0) return 1;
	if (b2->hash1 == 0) return -1;
	if (b1->last_play < b2->last_play)
		return 1;
	if (b1->last_play > b2->last_play)
		return -1;
	if (b2->move_num < b1->move_num)
		return 1;
	if (b2->move_num > b1->move_num)
		return -1;
	return memcmp(b1->board, b2->board, sizeof(b1->board));
}


static int setup_brain_pos(Position *b, struct brain_entry *br)
{
	memset(b, 0, sizeof(*b));
	memcpy(b->board, br->board, sizeof(b->board));
	b->move_num = br->move_num;
	b->flags = br->flags;
	b->enpassent = br->enpassent;
	b->fifty_count = 0;
	br->hash1 = 0;
	br->hash2 = 0;
	
	if (!create_pboard(b)) {
		lprintf(0,"brain entry corrupt\n");
		br->depth_low = -1;
		br->depth_high = -1;
		return 0;
	}
	br->hash1 = b->hash1;
	br->hash2 = b->hash2;

	return 1;
}

static void brain_resort(void)
{
	int i;

	for (i=0;i<brain_size;i++) {
		if (brain[i].depth_low == -1)
			memset(&brain[i], 0, sizeof(brain[i]));
	}

	qsort(brain, brain_size, sizeof(brain[0]), brain_compare);
	while (brain_size && brain[brain_size-1].hash1 == 0)
		brain_size--;
}


/* rehash the brain to recreate the hash1 and hash2 entries then sort
   on hash1 */
static void brain_rehash(void)
{
	int i;
	Position b;
	unsigned int sum1, sum2;
	time_t t1;

	/* first work out if the brain needs byte swapping */
	sum1 = sum2 = 0;
	for (i=0;i<brain_size;i++) {
		sum1 += brain[i].move_num;
		sum2 += swapu16(brain[i].move_num);
	}

	if (sum1 > sum2) {
		lprintf(0,"byte swapping brain\n");
		for (i=0;i<brain_size;i++) {
			brain[i].eval_low = swap32(brain[i].eval_low);
			brain[i].eval_high = swap32(brain[i].eval_high);
			brain[i].search_time = swapu32(brain[i].search_time);
			brain[i].last_play = swapu32(brain[i].last_play);
			brain[i].last_search = swapu32(brain[i].last_search);
			brain[i].flags = swapu32(brain[i].flags);
			brain[i].flags2 = swapu32(brain[i].flags);
			brain[i].move_num = swapu16(brain[i].move_num);
			brain[i].white_wins = swapu32(brain[i].white_wins);
			brain[i].black_wins = swapu32(brain[i].black_wins);
		}
	}
	

	t1 = time(NULL);

	for (i=0;i<brain_size;i++) {
		if (brain[i].hash1 == 0) {
			memset(&brain[i], 0, sizeof(brain[i]));
			continue;
		}

		setup_brain_pos(&b, &brain[i]);

		if (brain[i].last_play > t1 - play_offset(brain[i].move_num)) {
			brain[i].last_play = t1 - play_offset(b.move_num);
		}
	}

	brain_resort();

	lprintf(0,"rehashed brain\n");
}


/* find a brain entry using only hash1 */
static int brain_find_hash1(uint32 hash1)
{
	int low,high, t;

	if (brain_size == 0) return -1;

	low = 0;
	high = brain_size-1;
	while (low < high) {
		t = (low+high)/2;
		if (brain[t].hash1 == hash1) {
			return t;
		}
		if (brain[t].hash1 < hash1) {
			low = t+1;
		} else {
			high = t-1;
		}
	}
	if (brain[low].hash1 == hash1)
		return low;

	for (t=MAX_BRAIN_SIZE;t<MAX_BRAIN_SIZE+brain_temp_size;t++)
		if (brain[t].hash1 == hash1)
			return t;
		
	return -1;
}

/* use a bisection search to try to find a brain entry matching the
   specified position. We could possibly replace this with a hash table
   lookup later if this proves too expensive */
static int brain_find(Position *b)
{
	int t = brain_find_hash1(b->hash1);

	if (t == -1) return -1;

	if (brain[t].hash1 == b->hash1 &&
	    brain[t].hash2 == b->hash2) 
		return t;

	return -1;
}


void brain_fill_hash(Position *b)
{
	int i;
	struct hash_entry *t;
	if (brain_size == 0) return;

	/* don't bother filling the hash table if we are very
	   unlikely to hit any of the brain entries */
	if (!mulling && done_lookup > (last_lookup_hit+8))
		return;

	for (i=0;i<brain_size;i++) {
		if (mulling && i == mull_entry)
			continue;
		if (brain[i].depth_low == -1)
			continue;

		t = hash_ptr(brain[i].hash1);
		
		t->hash1 = brain[i].hash1;
		t->hash2 = brain[i].hash2;
		t->from = brain[i].from;
		t->to = brain[i].to;
		t->tag = 0;

		t->depth_low = brain[i].depth_low;
		EV(t->low) = brain[i].eval_low;
		t->depth_high = brain[i].depth_high;
		EV(t->high) = brain[i].eval_high;
	}
}

static void brain_save(void)
{
	int fd;
	char tmpname[200];

	if (!brain_fname || brain_size+brain_temp_size == 0) return;

	sprintf(tmpname,"%s.tmp",brain_fname);

	fd = open(tmpname,O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (fd == -1) {
		lprintf(0,"can't open %s\n", tmpname);
		return;
	}

	brain_resort();

	qsort(brain, brain_size, sizeof(brain[0]), brain_comp2);

	if (brain_temp_size) {
		int t = MAX_BRAIN_SIZE - brain_temp_size;
		if (t > brain_size) {
			t = brain_size;
		} else {
			int i;
			lprintf(0,"overwriting %d entries\n", brain_size - t);
			for (i=t;i<brain_size;i++)
				lprintf(0,"entry %s", entry_str(&brain[i]));
		}
		memcpy(&brain[t], brain_temp, sizeof(brain[0])*brain_temp_size);

		lprintf(0,"added %d brain entries\n", brain_temp_size);
		brain_size = t + brain_temp_size;
		brain_temp_size = 0;
	}

	if (Write(fd, brain, sizeof(brain[0])*brain_size) != 
	    sizeof(brain[0])*brain_size) {
		lprintf(0,"write to %s failed\n", brain_fname);
		close(fd);
		brain_resort();
		return;
	}
	
	close(fd);

	if (rename(tmpname, brain_fname) != 0) {
		lprintf(0,"rename %s %s failed\n",
			tmpname, brain_fname);
	}

	brain_resort();
}

static void brain_timer(time_t t)
{
	static time_t lastt;

	if (lastt + SAVE_FREQUENCY > t && 
	    brain_temp_size < BRAIN_TEMP_SIZE/2)
		return;

	lastt = t;

	brain_save();
}

int brain_open(char *fname)
{
	struct stat st;
	int fd;

	brain_size = 0;
	load_failed = 1;

	brain_fname = fname;

	fd = open(fname,O_RDWR);
	if (fd == -1) {
		lprintf(0,"can't open %s\n", fname);
		return 0;
	}

	if (fstat(fd, &st) != 0) {
		lprintf(0,"fstat %s failed\n", fname);
		close(fd);
		return 0;
	}

	if (st.st_size % sizeof(brain[0]) != 0) {
		lprintf(0,"corrupt brain file %s\n", fname);
		close(fd);
		return 0;
	}

	brain_size = st.st_size / sizeof(brain[0]);
	if (brain_size > MAX_BRAIN_SIZE)
		brain_size = MAX_BRAIN_SIZE;

	if (Read(fd, brain, brain_size*sizeof(brain[0])) != 
	    brain_size*sizeof(brain[0])) {
		lprintf(0,"brain read failed\n");
		close(fd);
		return 0;
	}

	close(fd);

	load_failed=0;
	brain_rehash();
	brain_clean();

	return 1;
}


/* lookup a position - using the brain as an opening book */
int brain_lookup(Position *b, Move *move, Eval *v)
{
	int t;
	Position b1;

	if (book_lookup(b, move, v)) {
		return 1;
	}

	if (mulling || !brain_size || !state->use_pbrain) return 0;

	if (b->move_num < done_lookup) {
		brain_save();
	}

	done_lookup = b->move_num;

	t = brain_find(b);
	if (t == -1) return 0;

	last_lookup_hit = done_lookup;

	brain[t].last_play = timer_start_time() - play_offset(b->move_num);

	consistency_check2(&brain[t]);

	if (brain[t].from == A1 && brain[t].to == A1)
		return 0;

	if (brain[t].depth_low <= 2 || brain[t].depth_high <= 2)
		return 0;

	if (brain[t].eval_low + MTD_THRESHOLD < brain[t].eval_high)
		return 0;

	/* if we have lost more than we have won with this line
	   then give it a rethink */
	if (next_to_play(b) == 1 && 
	    brain[t].black_wins > 1.5*(brain[t].white_wins+2))
		return 0;
	
	if (next_to_play(b) == -1 && 
	    (brain[t].white_wins > 1.5*(brain[t].black_wins+2)))
		return 0;
	
	/* if the stored search is lots shorter than our available
	   search time then don't use it */
	if (brain[t].search_time < state->move_time/4)
		return 0;

	/* if it needs searching and the stored search is a bit
	   shorter than our available search time then don't use it */
	if ((brain[t].flags2 & FLAG_NEEDS_RESEARCH) &&
	    brain[t].search_time < state->move_time*0.6)
		return 0;
	
	last_hit = b->move_num;
	
	brain[t].flags2 |= FLAG_NEEDS_RESEARCH;

	(*v) = makeeval(b, brain[t].eval_low);
	move->from = brain[t].from;
	move->to = brain[t].to;
	move->v = EV(*v);

	brain_timer(time(NULL));

	if (!legal_move(b, move) || !do_move(&b1, b, move)) {
		lprintf(0,"Illegal move from book! move=%s\n", 
			short_movestr(b, move));
		return 0;
	}

	if (check_repitition(&b1, 1)) 
		return 0;

	lprintf(0, "book: %s low=%d high=%d ww/bw=%d/%d depth=%d/%d searcht=%d\n",
		short_movestr(b, move),
		brain[t].eval_low,
		brain[t].eval_high,
		brain[t].white_wins,
		brain[t].black_wins,
		brain[t].depth_low,
		brain[t].depth_high,
		brain[t].search_time);

	return 1;
}


/* possibly insert an entry into the brain */
void brain_insert(Position *b, Move *move)
{
	int t;
	struct brain_entry *br;
	Position b1;
	struct hash_entry *h;

	if (!brain_fname || load_failed) return;

	t = brain_find(b);

	if (mulling && t != mull_entry) {
		return;
	}

	if (t != -1) {
		br = &brain[t];
	} else {
		br = &brain_temp[brain_temp_size];
		brain_temp_size++;
		memset(br, 0, sizeof(*br));
	}

	if (!do_move(&b1, b, move)) {
		lprintf(0,"illegal move passed to brain_insert\n");
		return;
	}

	h = fetch_hash(&b1);

	if (!h) {
		lprintf(0,"no hash entry for brain_insert move\n");
		return;
	}
	
	/* we might decide not to overwrite the existing entry */
	if (br->eval_low + MTD_THRESHOLD >= br->eval_high &&
	    imin(br->depth_low, br->depth_high) > imax(h->depth_low, h->depth_high) &&
	    br->search_time > 1.3*timer_elapsed()) {
		return;
	}
		    
	/* we've decided where to put it, finish it off */
	br->hash1 = b->hash1;
	br->hash2 = b->hash2;
	br->eval_low = -EV(h->high);
	br->eval_high = -EV(h->low);
	br->depth_low = h->depth_low+1;
	br->depth_high = h->depth_high+1;
	br->from = move->from;
	br->to = move->to;

	br->search_time = timer_elapsed();
	memcpy(br->board, b->board, sizeof(b->board));
	br->flags = b->flags;
	br->move_num = b->move_num;
	br->enpassent = b->enpassent;
	br->last_search = time(NULL);

	if (!mulling) {
		br->last_play = time(NULL) - play_offset(b->move_num);
		br->flags2 |= FLAG_NEEDS_RESEARCH;
	}
		

	lprintf(0,"inserted entry %s", entry_str(br));

	/* now check for consistency with the moves around this one,
	   thus giving us instant brain updates */
	consistency_check1(br);

	if (brain_temp_size > 1) {
		consistency_check2(&brain_temp[brain_temp_size-2]);
	}

	t = brain_find_hash1(state->hash_list[br->move_num-1]);
	if (t != -1) {
		consistency_check2(&brain[t]);
	}

	t = brain_find_hash1(state->hash_list[br->move_num-2]);
	if (t != -1) {
		consistency_check2(&brain[t]);
	}

	brain_timer(time(NULL));
}

void brain_close(void)
{
	brain_save();
}


static int choose_mull(void)
{
	int t, r;
	int bestt;
	int mull_limit = 10;


	lprintf(0,"stage 1 mulling\n");

	/* the second priority is brain moves that need a re-search */
	while (mull_limit < 30) {
		bestt = -1;

		for (t=0;t<brain_size;t++) {
			if (brain[t].hash1 == 0) continue;
			if (brain[t].flags2 & FLAG_SEARCHED) continue;

			if ((brain[t].flags2 & FLAG_NEEDS_RESEARCH) && 
			    brain[t].move_num <= mull_limit) {
				if (bestt == -1 || 
				    brain[t].move_num > brain[bestt].move_num) {
					bestt = t;
				}
			}
		}
		if (bestt != -1) {
			return bestt;
		}
		mull_limit += 10;
	}

	lprintf(0,"all entries searched - using random mull\n");

	/* now pick somewhat randomly, weighting by various factors */
	while (!state->quit && state->computer == 0) {
		t = random() % brain_size;

		if (brain[t].hash1 == 0) continue;

		/* don't mull on mate! */
		if (brain[t].eval_low > (WIN-100) || 
		    brain[t].eval_high < -(WIN-100))
			continue;

		r = 1;

		if (brain[t].move_num < 10)
			r += (10 - brain[t].move_num)*100;
		
		if (brain[t].depth_low < 10) 
			r += (10 - brain[t].depth_low)/3;

		if (brain[t].search_time < 20) 
			r += (20 - brain[t].search_time)/10;

		if (random() % 1000 >= r)
			continue;
	
		return t;
	}
	
	return -1;
}


void brain_mull(void)
{
	Position b;
	int t;
	Move move;
	float old_move_time=0;

	if (brain_size == 0) return;

	lprintf(0,"starting mulling on %d brain entries\n", brain_size);

	mulling = 1;

	brain_clean();

	while (!state->quit && state->computer == 0 && 
	       state->use_mulling) {
		t = choose_mull();
		if (t == -1) continue;
		if (!setup_brain_pos(&b, &brain[t]))
			continue;

		old_move_time = state->move_time;

		state->move_time = brain[t].search_time * 2;
		if (state->move_time > MAX_MULL_TIME)
			state->move_time = MAX_MULL_TIME;
		if (state->move_time < MIN_MULL_TIME)
			state->move_time = MIN_MULL_TIME;
		timer_reset();
		hash_reset();
		order_reset();

		mull_entry = t;

		move.from = brain[t].from;
		move.to = brain[t].to;

		print_board(b.board);

		lprintf(0,"mulling %d depth=%d/%d mtime=%2.1f ww=%d bw=%d move=%d v=%d/%d %s\n",
			t, 
			brain[t].depth_low, 
			brain[t].depth_high, 
			state->move_time,
			brain[t].white_wins,
			brain[t].black_wins,
			brain[t].move_num, 
			brain[t].eval_low,
			brain[t].eval_high,
			short_movestr(&b, &move));

		make_move(&b, &move);

		state->move_time = old_move_time;

		if (state->computer) continue;
		brain[t].flags2 &= ~FLAG_NEEDS_RESEARCH;
		brain[t].flags2 |= FLAG_SEARCHED;
		/* This resumes any available match in the adjourned list 
		   (possibly ICS only) */
		if (state->ics_robot && state->autoplay)
			prog_printf("resume\n");
	}


	mulling = 0;
}


/* this is used when inserting a move into the brain. The insert may
   obsolete later brain entries */
static void consistency_check1(struct brain_entry *br)
{
	int t2, num_moves, m;
	Move m1;
	Position b, b1, b2;
	static Move moves[MAX_MOVES];
	struct hash_entry *h;

	/* first some basic sanity checks */
	if (br->search_time < 0)
		br->search_time = 0;

	if (!setup_brain_pos(&b, br)) {
		lprintf(0,"removing brain entry\n");
		br->depth_low = -1;
		return;
	}

	if ((br->move_num & 1) != (br->hash1 & 1)) {
		lprintf(0,"wrong player in consistency_check1 %s",
			entry_str(br));
		br->depth_low = -1;
		return;
	}


	/* now see if this brain entry leads to another brain entry.
	   If it does then the value of this position is bounded by
	   the value of the other position */

	m1.from = br->from;
	m1.to = br->to;

	if (!do_move(&b1, &b, &m1)) {
		lprintf(0,"removing brain entry\n");
		br->depth_low = -1;
		return;
	}
		
	t2 = brain_find(&b1);

	if (t2 != -1 && (h = fetch_hash(&b1))) {
		if (brain[t2].eval_high != EV(h->high) || 
		    brain[t2].eval_low != EV(h->low)) {
			brain[t2].eval_high = EV(h->high);
			brain[t2].eval_low = EV(h->low);
			brain[t2].depth_low = h->depth_low;
			brain[t2].depth_high = h->depth_high;
			brain[t2].from = h->from;
			brain[t2].to = h->to;
			brain[t2].flags2 |= FLAG_NEEDS_RESEARCH;
			lprintf(0,"refined entry1 %s", entry_str(&brain[t2]));
		}
	}

	/* now generate and try all moves from this position. If they
	   lead to positions in the brain then these positions also
	   bound the eval at the current position */
	num_moves = generate_moves(&b1, moves);
		
	for (m=0;m<num_moves;m++) {
		if (!do_move(&b2, &b1, &moves[m])) continue;
		
		t2 = brain_find(&b2);
		if (t2 == -1) continue;
		
		if (t2 != -1 && (h = fetch_hash(&b2))) {
			if (brain[t2].eval_high != EV(h->high) || 
			    brain[t2].eval_low != EV(h->low)) {
				brain[t2].eval_high = EV(h->high);
				brain[t2].eval_low = EV(h->low);
				brain[t2].depth_low = h->depth_low;
				brain[t2].depth_high = h->depth_high;
				brain[t2].from = h->from;
				brain[t2].to = h->to;
				brain[t2].flags2 |= FLAG_NEEDS_RESEARCH;
				lprintf(0,"refined entry2 %s", entry_str(&brain[t2]));
			}
		}
	}
}


/* this is used when thinking about using an entry in the brain, or
   when mulling entries. It may invalidate the current entry based on
   later entries */
static void consistency_check2(struct brain_entry *br)
{
	int t2, num_moves, m;
	Move m1;
	Position b, b1, b2;
	static Move moves[MAX_MOVES];
	struct hash_entry *h;

	/* first some basic sanity checks */
	if (br->search_time < 0)
		br->search_time = 0;

	/* now see if this brain entry leads to another brain entry.
	   If it does then the value of this position is bounded by
	   the value of the other position */
	if (!setup_brain_pos(&b, br)) {
		lprintf(0,"removing brain entry\n");
		br->depth_low = -1;
		return;
	}

	if ((br->move_num & 1) != (br->hash1 & 1)) {
		lprintf(0,"wrong player in consistency_check2 %s",
			entry_str(br));
		br->depth_low = -1;
		return;
	}


	m1.from = br->from;
	m1.to = br->to;

	if (!legal_move(&b, &m1)) {
		lprintf(0,"illegal move %s", entry_str(br));
		br->depth_low = -1;
		return;
	}

	if (!do_move(&b1, &b, &m1)) {
		lprintf(0,"removing brain entry\n");
		br->depth_low = -1;
		return;
	}
		
	t2 = brain_find(&b1);

	if (t2 != -1 && (h = fetch_hash(&b1))) {
		if (br->eval_high < -EV(h->low) || 
		    br->eval_low > -EV(h->high)) {
			br->eval_high = imax(br->eval_high, -EV(h->low));
			br->eval_low = imin(br->eval_low, -EV(h->high));
			br->flags2 |= FLAG_NEEDS_RESEARCH;
			lprintf(0,"refined entry3 %s", entry_str(br));
		}
	}

	/* now generate and try all moves from this position. If they
	   lead to positions in the brain then these positions also
	   bound the eval at the current position */
	num_moves = generate_moves(&b1, moves);
		
	for (m=0;m<num_moves;m++) {
		if (!do_move(&b2, &b1, &moves[m])) continue;
		
		t2 = brain_find(&b2);
		if (t2 == -1) continue;
		
		if (t2 != -1) {
			if (br->eval_low > brain[t2].eval_low) {
				br->eval_low = brain[t2].eval_low;
				br->flags2 |= FLAG_NEEDS_RESEARCH;
				lprintf(0,"refined entry4 %s", entry_str(br));
			}
		}
	}
}


void analyse_game(void)
{
	int i, t;
	int winner=0;
	int num_moves = done_lookup;

	done_lookup = 0;

	/* couldn't think of a more reliable place to reset this */
	state->rating_change = -2;

	/* don't analyse _really_ short games */
	if (num_moves < 6) {
		return;
	}

#if LEARN_EVAL
	if (learning) {
		td_update();
		td_dump("coeffs.dat");
	}
#endif

	lprintf(0,"analysis: %d moves\n", num_moves);

	/* work out who won (or was winning when it ended) */
	for (i=num_moves-2;i>=0;i--) {
		lprintf(0,"move %d eval=%d\n", i, state->game_record[i].v);
		if (state->game_record[i].v != INFINITY) {
			if (state->game_record[i].v > PAWN_VALUE) {
				winner = 1;
			} else if (state->game_record[i].v < -PAWN_VALUE) {
				winner = -1;
			}
			break;
		}
	}

	lprintf(0,"game analysis winner=%d moves=%d\n",
		winner, num_moves);

	if (winner == 0) return;

	/* now run backwards through the game, setting the evaluation of
	   each position to the minimum of the current evaluation and
	   the evaluation after the opponents move */
	for (i=num_moves-3;i>=0;i--) {
		t = brain_find_hash1(state->hash_list[i]);

		if (t == -1) continue;

		if (winner == 1)
			brain[t].white_wins++;
		else if (winner == -1)
			brain[t].black_wins++;
	}	

	lprintf(0,"game analysis did %d moves\n",
		num_moves);

	brain_save();
}


static int clean_comp(int *i1, int *i2)
{
	if (brain[*i1].move_num != brain[*i2].move_num)
		return brain[*i1].move_num - brain[*i2].move_num;
	return memcmp(brain[*i1].board, brain[*i2].board, 
		      sizeof(brain[0].board));	
}


/* clean the brain. This means finding inconsisant brain entries
   and fixing them */
void brain_clean(void)
{
	int i, t;
	int brindex[MAX_BRAIN_SIZE];
	int duplicates=0;
#define MAX_BCOUNTS 20
	int bcounts[MAX_BCOUNTS];

	brain_save();

	if (!brain_size) return;

	lprintf(0,"cleaning brain\n");

	for (i=0;i<brain_size-1;i++) {
		brain[i].flags2 &= ~FLAG_SEARCHED;
		if (brain[i].hash1 == brain[i+1].hash1) {
			if (brain[i].hash2 == brain[i+1].hash2) {
				lprintf(0,"duplicate entry %s",
					entry_str(&brain[i]));
				lprintf(0,"duplicate entry %s",
					entry_str(&brain[i+1]));
				brain[i+1].depth_low = -1;
				brain[i+1].depth_high = -1;
				duplicates++;
			} else {
				lprintf(0,"duplicate hash1 values\n");
			}
		}
	}

	brain_resort();

	/* build a list of indexes sorted by move number */
	for (i=0;i<brain_size;i++) brindex[i] = i;
	qsort(brindex, brain_size, sizeof(brindex[0]), clean_comp);

	memset(bcounts, 0, sizeof(bcounts));

	/* now run through the moves in move order */
	for (i=0;i<brain_size;i++) {
		t = brindex[i];

		/* first some basic sanity checks */
		if (brain[t].search_time < 0)
			brain[t].search_time = 0;

		/* now the real tests */
		consistency_check2(&brain[t]);

		if (state->computer != 0)
			return;
		
		if (brain[t].move_num < MAX_BCOUNTS && 
		    (brain[t].flags2 & FLAG_NEEDS_RESEARCH)) {
			bcounts[brain[t].move_num]++;
		}
	}

	brain_save();

	lprintf(0,"finished cleaning brain\n");
	lprintf(0,"bcounts: ");
	for (t=0;t<MAX_BCOUNTS;t++)
		lprintf(0,"%3d ", bcounts[t]);
	lprintf(0,"\n");
}




