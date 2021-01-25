#include "includes.h"
#include "knightcap.h"

extern struct state *state;

#define LOAD_BAD 0
#define LOAD_OK 1
#define LOAD_OTHER 2

static char *fin_comment;

/* load the next position from a .fin file. Put the best move in m */
static int load_fin(FILE *f, Position *b, Move *m)
{
	char line[1000];
	char *p, *tokp;
	int x, y;
	int count;
	char tok[100];

	memset(line,0,sizeof(line));

	do {
		if (!fgets(line, sizeof(line)-1, f)) return LOAD_BAD;
	} while (line[0] == '#');

	memset(b,0, sizeof(*b));

	tokp = line;

	if (!next_token(&tokp, tok, " \t"))
		return LOAD_BAD;

	for (p=tok, count=0; *p; p++)
		if (*p == '/') count++;

	if (count != 8) return LOAD_BAD;

	y = 7;
	x = 0;
	p = tok;

	for (; y >= 0; p++) {
		if (*p == '/') {
			y--;
			x = 0;
			continue;
		}
		if (isdigit(*p)) {
			x += (*p) - '0';
			continue;
		}
		b->board[POSN(x,y)] = charpiece(*p);
		if (islower(*p)) b->board[POSN(x,y)] = -b->board[POSN(x,y)];
		x++;
	}
	
	/* arbitrary setting of move_num */
	b->move_num=30 + random() % 10;

	if (*p == 'w') 
		b->move_num &= ~1;
	else
		b->move_num |= 1;

	if (b->board[E1] == KING && b->board[H1] == ROOK)
		b->flags |= WHITE_CASTLE_SHORT;
	if (b->board[E1] == KING && b->board[A1] == ROOK)
		b->flags |= WHITE_CASTLE_LONG;

	if (b->board[E8] == -KING && b->board[H8] == -ROOK)
		b->flags |= BLACK_CASTLE_SHORT;
	if (b->board[E8] == -KING && b->board[A8] == -ROOK)
		b->flags |= BLACK_CASTLE_LONG;
	
	if (!create_pboard(b)) {
		lprintf(0,"bad position\n");
		print_board(b->board);
		return LOAD_BAD;
	}

	eval(b, INFINITY, MAX_DEPTH);
	
	fin_comment = tokp;

	if (!next_token(&tokp, tok, " \t"))
		return LOAD_BAD;

	if (!parse_move(tok, b, m)) {
		lprintf(0,"can't parse %s\n", tok);
		return LOAD_BAD;
	}

	if (!legal_move(b, m)) {
		lprintf(0,"illegal %s %s\n", short_movestr(b, m), tok);
		return LOAD_BAD;
	}
	
	if (strchr(tok, '?')) {
		return LOAD_OTHER;
	}

	return LOAD_OK;
}


/* run all the tests in a .fin file */
void test_fin(char *fname)
{
	FILE *f;
	Move m, m2;
	Position *b = &state->position;
	int correct, total;
	int load;

	f = fopen(fname,"r");
	if (!f) {
		perror(fname);
		return;
	}

	correct = total = 0;

	while (!feof(f)) {
		state->computer = 0;
		state->stop_search = 1;
		state->moved = 0;
		
		load = load_fin(f, b, &m);

		if (load == LOAD_BAD) continue;

		timer_reset();
		state->need_reset = 1;
		while (state->need_reset) 
			usleep(1);

		update_display();

		if (fin_comment)
			lprintf(0,"%s", fin_comment);

		if (load == LOAD_OK) {
			lprintf(0,"looking for %s\n", short_movestr(b, &m)); 
		} else {
			lprintf(0,"looking for anything except %s\n", 
				short_movestr(b, &m)); 
		}

		state->computer = next_to_play(&state->position);

		while (!state->moved) 
			sleep(1);

		m2 = state->game_record[b->move_num];

		total++;

		if (load == LOAD_OK) {
			if (same_move(&m2, &m)) {
				correct++;
				lprintf(0,"correct %s  %d/%d %d%%\n",
					short_movestr(b, &m2), 
					correct, total, 
					100*correct/total);
			} else {
				lprintf(0,"incorrect %s - should be %s  %d/%d %d%%\n",
					short_movestr(b, &m2), short_movestr(b, &m), 
					correct, total, 
					100*correct/total);
			}
		} else {
			if (!same_move(&m2, &m)) {
				correct++;
				lprintf(0,"correct %s %d/%d %d%%\n",
					short_movestr(b, &m2), 
					correct, total, 
					100*correct/total);
			} else {
				lprintf(0,"blunder %s %d/%d %d%%\n",
					short_movestr(b, &m2), 
					correct, total, 
					100*correct/total);
			}
		}
	}

	fclose(f);
}
